"""Local-only preview. Uses the same compiled effects.h as the ESP32.

Requires Node.js to run the included effects.wasm, built from effects.h.
"""
import argparse
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
from pathlib import Path
import subprocess
import time
from urllib.parse import parse_qs

ROOT=Path(__file__).resolve().parent
MODES=('manual','hsl_cycle','snow','halloween','christmas')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--node',default='node')
    parser.add_argument('--port',type=int,default=8765)
    args=parser.parse_args()
    engine=subprocess.Popen([args.node,str(ROOT/'preview_engine.cjs')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
    state={'mode':'hsl_cycle','brightness':255,'snow_speed':3,'snow_density':4,
           'floors':[{'id':name,'on':True,'color':'#ffa04b','leds':12 if i==0 else 9}
                     for i,name in enumerate(('SUPERIOR','CENTRAL','TERREO'))]}
    start=time.monotonic()

    class Handler(BaseHTTPRequestHandler):
        def send(self,code,body,mime='application/json'):
            raw=body.encode();self.send_response(code);self.send_header('Content-Type',mime)
            self.send_header('Content-Length',str(len(raw)));self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(raw)

        def snapshot(self):
            now=int((time.monotonic()-start)*1000)
            values=[now,MODES.index(state['mode']),state['brightness'],state['snow_speed'],state['snow_density']]
            line=' '.join(map(str,values+[int(f['on']) for f in state['floors']]))+' '+ ' '.join(f['color'][1:] for f in state['floors'])+'\n'
            engine.stdin.write(line);engine.stdin.flush()
            pixels=json.loads(engine.stdout.readline())
            for floor,colors in zip(state['floors'],pixels):
                floor['pixels']=colors;floor['rendered_color']=colors[0]
            self.send(200,json.dumps(state))

        def do_GET(self):
            if self.path=='/':
                text=(ROOT/'web_ui.h').read_text(encoding='utf8').split('R"HTML(',1)[1].split(')HTML";',1)[0]
                self.send(200,text,'text/html; charset=utf-8')
            elif self.path=='/status': self.snapshot()
            else:self.send(404,'{}')

        def do_POST(self):
            if self.path!='/control':self.send(404,'{}');return
            if self.headers.get('Origin') not in (None,'http://'+self.headers.get('Host','')):
                self.send(403,'{}');return
            try:
                length=int(self.headers.get('Content-Length',0))
                if length>4096:raise ValueError()
                data={k:v[0] for k,v in parse_qs(self.rfile.read(length).decode()).items()}
                action=data['action']
                if action=='mode':
                    if data['value'] not in MODES:raise ValueError()
                    state['mode']=data['value']
                elif action=='brightness':
                    n=int(data['value']);assert 0<=n<=100;state['brightness']=(n*255+50)//100
                elif action=='all':
                    assert data['value'] in ('on','off')
                    for floor in state['floors']:floor['on']=data['value']=='on'
                elif action=='floor':
                    floor=next(f for f in state['floors'] if f['id']==data['id'])
                    assert ('on' in data) != ('color' in data)
                    if 'on' in data:
                        assert data['on'] in ('true','false');floor['on']=data['on']=='true'
                    else:
                        color=data['color'];assert len(color)==7 and color[0]=='#';int(color[1:],16)
                        floor['color']=color;floor['on']=True;state['mode']='manual'
                elif action=='snow':
                    updates={}
                    for key,maximum in (('speed',5),('density',8)):
                        if key in data:
                            n=int(data[key]);assert 1<=n<=maximum;updates['snow_'+key]=n
                    assert updates;state.update(updates)
                else:raise ValueError()
            except (ValueError,KeyError,AssertionError,StopIteration):self.send(400,'{}');return
            self.snapshot()

        def log_message(self,*args):pass

    server=HTTPServer(('127.0.0.1',args.port),Handler)
    print(f'Prévia local: http://127.0.0.1:{args.port} (CTRL+C encerra)',flush=True)
    try:server.serve_forever()
    finally:
        server.server_close();engine.terminate();engine.wait()


if __name__=='__main__':main()
