"""Painel público da feira. ESC sai; F2 abre organização."""
import argparse
from charts import SessionCharts
import json
import logging
import os
from pathlib import Path
import queue
import random
import time
import uuid
from storage import Store, PROJECTS
from transport import SerialHub

NAMES = ('Pedra', 'Papel', 'Tesoura')
MOVES = {'Closed_Fist': 0, 'Open_Palm': 1, 'Victory': 2}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--demo', action='store_true', help='Dados separados; nenhuma placa será acessada')
    parser.add_argument('--windowed', action='store_true')
    parser.add_argument('--frames', type=int, default=0, help=argparse.SUPPRESS)
    parser.add_argument('--screenshot', help=argparse.SUPPRESS)
    parser.add_argument('--size', default='1366x768', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.screenshot and not args.demo:
        parser.error('--screenshot exige --demo; imagens da câmera não são gravadas')
    root = Path(os.environ.get('LOCALAPPDATA', str(Path.home()))) / 'CtrlFeira'
    root.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault('MPLCONFIGDIR',str(root/'matplotlib'))
    logging.basicConfig(filename=root/'panel.log', level=logging.INFO,
                        format='%(asctime)s %(levelname)s %(message)s')
    config_path = root/'config.json'
    try:
        config = json.loads(config_path.read_text())
    except (OSError, ValueError):
        config = {}
    config = {'camera': 0, **{p: 'auto' for p in PROJECTS}, **config}
    store = Store(root/('demo.sqlite3' if args.demo else 'results.sqlite3'))
    hub = None if args.demo else SerialHub(config)
    from camera import Camera
    from game import GestureGate
    camera = None if args.demo else Camera(config['camera'], root/'gesture_recognizer.task')
    import pygame as pg
    pg.init()
    screen = pg.display.set_mode(tuple(map(int, args.size.split('x'))), pg.RESIZABLE if args.windowed else pg.FULLSCREEN)
    pg.display.set_caption('Feira · Jokenpo e jogo da velha')
    canvas = pg.Surface((1366,768))
    fonts = {n: pg.font.SysFont('Segoe UI', n) for n in (16,18,20,24,32)}
    clock, gate = pg.time.Clock(), GestureGate()
    organizing, running = False, True
    states, ports = {}, []
    demo_boot, demo_id, next_demo = uuid.uuid4().hex, 0, 0
    history = {p: store.results(p) for p in PROJECTS}
    charts = SessionCharts()
    frame_count = 0
    previous_observation = 0

    def label(text, x, y, size=20, color='#e7edf6'):
        canvas.blit(fonts[size].render(str(text), True, color), (x,y))

    def rect(x,y,w,h,color='#1c2a3d'):
        pg.draw.rect(canvas,color,(x,y,w,h),border_radius=12)

    try:
        while running:
            now=time.monotonic()
            for event in pg.event.get():
                if event.type==pg.QUIT or event.type==pg.KEYDOWN and event.key==pg.K_ESCAPE:
                    running=False
                if event.type!=pg.KEYDOWN:
                    continue
                if event.key==pg.K_F2:
                    organizing=not organizing
                if organizing:
                    if event.key==pg.K_n:
                        store.new_session(); history={p:[] for p in PROJECTS}
                    if event.key==pg.K_c:
                        config['camera']=(int(config['camera'])+1)%8
                        if camera:
                            camera.close(); camera=Camera(config['camera'],root/'gesture_recognizer.task')
                    for key,project in ((pg.K_j,PROJECTS[0]),(pg.K_t,PROJECTS[1])):
                        if event.key==key:
                            choices=['auto']+ports
                            current=config.get(project,'auto')
                            config[project]=choices[(choices.index(current)+1)%len(choices)] if current in choices else 'auto'
                    config_path.write_text(json.dumps(config),encoding='utf8')
                    if hub:
                        hub.configure(config)
            if hub:
                states,ports=hub.snapshot()
                while True:
                    try:
                        message=hub.events.get_nowait()
                    except queue.Empty:
                        break
                    # Never ACK before the SQLite transaction commits, including duplicates.
                    if store.record(message):
                        history[message['project']].append(message)
                    hub.send({**{k:message[k] for k in ('v','project','device','boot','game')},'type':'ack'})
            if args.demo and now>=next_demo:
                next_demo=now+3; demo_id+=1
                for project in PROJECTS:
                    m={'v':1,'project':project,'device':'DEMO','boot':demo_boot,'game':demo_id,
                       'type':'result','player':random.randrange(3),'computer':random.randrange(3),
                       'mode':random.choice(('random','trained')),'board':'XO--X-O--',
                       'result':'D','remaining_ms':240000,'episodes':200000,'state':'result'}
                    m['outcome']=('draw' if m['player']==m['computer'] else 'win' if (m['player']-m['computer'])%3==1 else 'loss')
                    if project=='jokenpo':
                        m['result']={'win':'VOCÊ GANHOU!','loss':'ESP GANHOU!','draw':'EMPATE!'}[m['outcome']]
                    else:
                        m['result']={'win':'X','loss':'O','draw':'D'}[m['outcome']]
                        m['board']={'win':'XXXOO----','loss':'XX-OOO---','draw':'XOXOOXXXO'}[m['outcome']]
                    if store.record(m): history[project].append(m)
                    states[project]=(m,now)
            canvas.fill('#101a29')
            label('FEIRA  /  Jokenpo',28,16,32)
            label('Jogo da velha',710,16,32)
            label(('DEMONSTRAÇÃO · ' if args.demo else '')+f'Sessão {store.session} · F2 Organização',28,738,16,'#a5b6ca')
            active={}
            for i,p in enumerate(PROJECTS):
                m,seen=states.get(p,({},0))
                online=now-seen<3
                active[p]=m if online else {}
                label('DEMO' if args.demo else f"USB {m.get('_port','')} · conectado" if online else 'Placa indisponível · reconectando',28+i*682,58,18,'#5ce1b7' if online else '#ffbd7a')
            j,t=active[PROJECTS[0]],active[PROJECTS[1]]
            sample,error=camera.snapshot() if camera else (None,'Prévia de demonstração · câmera desativada')
            rect(28,88,620,266)
            gesture='None'
            if sample:
                image,gesture,observed=sample
                surface=pg.image.frombuffer(image.tobytes(),(image.shape[1],image.shape[0]),'RGB')
                canvas.blit(pg.transform.smoothscale(surface,(470,264)),(28,88))
                if observed != previous_observation or now-observed>.35:
                    trigger,move=gate.update(gesture if now-observed<=.35 else 'None',now,j.get('state')=='standby')
                    if trigger and now-observed<=.35 and hub:
                        hub.send({**{k:j[k] for k in ('v','project','device','boot','game')},'type':'start','move':move})
                    previous_observation=observed
            if error:
                # Keep long diagnostics within the camera card.
                words=error.split(); lines=['']
                for word in words:
                    if len(lines[-1])+len(word)>66: lines.append('')
                    lines[-1]+=word+' '
                for i,line in enumerate(lines): label(line,40,150+i*26,16,'#ffbd7a')
            label(NAMES[MOVES[gesture]] if gesture in MOVES else 'Mostre a mão',505,110,18)
            phase=j.get('state','offline')
            label(['JO','KEN','PO!'][min(2,j.get('elapsed_ms',0)//700)] if phase=='countdown' else 'Pronto' if phase=='standby' else 'Resultado' if phase=='result' else 'Sem placa',505,152,24)
            label(j.get('result',''),40,320,20,'#5ce1b7')
            if phase=='result' and all(j.get(k) in (0,1,2) for k in ('player','computer')):
                label(f"Você: {NAMES[j['player']]}  /  ESP: {NAMES[j['computer']]}",40,288,18)
            charts_images = charts.images(store.session, history[PROJECTS[0]], history[PROJECTS[1]])
            for chart, position in zip(charts_images, ((28,366),(398,366),(710,366))):
                canvas.blit(chart, position)
            training=t.get('training',False)
            mode='TREINANDO' if training else 'Última partida treinada' if t.get('last_trained') else 'TREINADO' if t.get('mode')=='trained' else 'ALEATÓRIO'
            ms=t.get('remaining_ms',0)
            label(mode,710,94,24,'#5ce1b7')
            episodes=f'{t.get("episodes",0):,}'.replace(',','.')
            label(f'Validade {ms//60000:02}:{ms//1000%60:02} · Aprendizado {episodes} / 200.000',710,130,18)
            if training:
                pg.draw.rect(canvas,'#25364c',(710,346,620,8),border_radius=4)
                pg.draw.rect(canvas,'#5ce1b7',(710,346,int(620*min(1,t.get('episodes',0)/200000)),8),border_radius=4)
            board=t.get('board','---------')
            for cell in range(9):
                x,y=710+(cell%3)*61,172+(cell//3)*55
                rect(x,y,55,49)
                label(board[cell] if len(board)==9 and board[cell]!='-' else '·',x+19,y+8,24)
            label('Treinamento em andamento' if training else {'X':'Vitória do jogador','O':'Vitória do ESP','D':'Empate','-':'Partida em andamento'}.get(t.get('result'),'Aguardando placa'),918,192,20)
            label('Controles nos botões físicos',918,232,18,'#a5b6ca')
            if organizing:
                rect(260,170,850,420,'#2c3c53')
                label('Organização · F2 fecha',290,190,32)
                label(f'C · Alternar câmera (0–7): {config["camera"]}',290,250,24)
                label(f'J · Porta Jokenpo: {config[PROJECTS[0]]}',290,300,24)
                label(f'T · Porta jogo da velha: {config[PROJECTS[1]]}',290,350,24)
                label('N · Iniciar nova sessão (histórico anterior preservado)',290,410,24)
                label('Portas: '+(', '.join(ports) or 'nenhuma · consulte diagnóstico USB no guia'),290,468,18)
                label('Dados e logs: '+str(root),290,520,16)
            screen.blit(pg.transform.smoothscale(canvas,screen.get_size()),(0,0))
            pg.display.flip(); clock.tick(30)
            frame_count+=1
            if args.frames and frame_count>=args.frames: running=False
    finally:
        if args.screenshot:
            pg.image.save(screen,args.screenshot)
        if camera: camera.close()
        if hub: hub.close()
        store.close(); pg.quit()


if __name__=='__main__':
    try:
        main()
    except Exception:
        logging.exception('Falha no painel')
        raise
