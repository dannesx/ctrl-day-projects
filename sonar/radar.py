"""Radar HC-SR04: serial angle,distance_cm at 115200 baud."""
import argparse
import math
import os
import time
from collections import deque

os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
import pygame
import serial


def parse_sample(line):
    try:
        angle, distance = map(float, line.split(','))
        if not math.isfinite(angle) or not math.isfinite(distance):
            return None
        if 0 <= angle <= 180 and (distance == -1 or 2 <= distance <= 400):
            return angle, distance
    except (ValueError, TypeError):
        pass
    return None


class Connection:
    def __init__(self, port):
        self.port = port
        self.device = None
        self.retry = 0
        self.buffer = b''
        self.ready = False
        self.status = 'USB desconectada · tentando conectar'

    def poll(self, now):
        samples = []
        try:
            if self.device is None and now >= self.retry:
                self.retry = now + 2
                self.device = serial.Serial(self.port, 115200, timeout=0, write_timeout=0.2)
                self.opened = now
                self.status = 'Conectado · aguardando firmware'
                self.buffer = b''
            if self.device:
                self.buffer += self.device.read(min(self.device.in_waiting, 8192))
                while b'\n' in self.buffer:
                    line, self.buffer = self.buffer.split(b'\n', 1)
                    line = line.decode('ascii', errors='replace').strip()
                    if line == 'READY':
                        self.ready = True
                        self.status = 'Pronto · posicione em 90° e pressione ESPAÇO'
                    else:
                        sample = parse_sample(line)
                        if sample:
                            samples.append(sample)
                if len(self.buffer) > 8192:
                    self.buffer = b''
                if not self.ready and now - self.opened > 4:
                    self.status = 'Sem resposta · envie o firmware com make upload'
        except (serial.SerialException, OSError):
            self.close()
            self.status = 'USB desconectada · tentando conectar'
        return samples

    def send(self, command):
        try:
            if self.device and self.ready:
                self.device.write(command)
                return True
        except (serial.SerialException, OSError):
            self.close()
        return False

    def close(self):
        if self.device:
            self.device.close()
        self.device = None
        self.ready = False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='/dev/ttyUSB0')
    parser.add_argument('--demo', action='store_true')
    parser.add_argument('--range', type=int, choices=[20, 50, 100, 200, 400], default=100)
    parser.add_argument('--screenshot', help='Salva uma previa e encerra')
    parser.add_argument('--fullscreen', action='store_true', help='Inicia em tela cheia')
    parser.add_argument('--size', default='1100x720', help='Tamanho inicial, ex.: 1920x1080')
    args = parser.parse_args()
    try:
        window_size = tuple(map(int, args.size.lower().split('x')))
        if len(window_size) != 2 or min(window_size) < 320:
            raise ValueError
    except ValueError:
        parser.error('--size deve ser LARGURAxALTURA, com dimensões >= 320')
    pygame.display.init()
    pygame.font.init()
    fullscreen = args.fullscreen
    display = pygame.display.set_mode(
        (0, 0) if fullscreen else window_size,
        pygame.FULLSCREEN if fullscreen else pygame.RESIZABLE,
    )
    screen = None
    rendered_size = None
    pygame.display.set_caption('Sonar · HC-SR04')
    font = pygame.font.SysFont('DejaVu Sans Mono', 20)
    small = pygame.font.SysFont('DejaVu Sans Mono', 14)
    clock = pygame.time.Clock()
    connection = None if args.demo else Connection(args.port)
    echoes = {}
    trail = deque(maxlen=28)
    angle, distance = 90.0, -1
    last_sample = 0
    running, started, paused = True, False, False
    scale = args.range
    frame = 0
    start = time.monotonic()
    green = (63, 244, 72)

    def label(text, position, color=green, compact=False):
        screen.blit((small if compact else font).render(text, True, color), position)

    try:
        while running:
            now = time.monotonic()
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_F11 or (event.key == pygame.K_ESCAPE and fullscreen):
                        if not fullscreen:
                            window_size = display.get_size()
                        fullscreen = not fullscreen
                        display = pygame.display.set_mode(
                            (0, 0) if fullscreen else window_size,
                            pygame.FULLSCREEN if fullscreen else pygame.RESIZABLE,
                        )
                    elif event.key == pygame.K_ESCAPE:
                        running = False
                    elif event.key == pygame.K_c:
                        echoes.clear()
                        trail.clear()
                    elif event.key in (pygame.K_EQUALS, pygame.K_PLUS, pygame.K_KP_PLUS, pygame.K_MINUS, pygame.K_KP_MINUS):
                        scales = [20, 50, 100, 200, 400]
                        delta = -1 if event.key in (pygame.K_MINUS, pygame.K_KP_MINUS) else 1
                        scale = scales[max(0, min(4, scales.index(scale) + delta))]
                    elif event.key == pygame.K_SPACE and connection:
                        command = b'S' if not started else (b'R' if paused else b'P')
                        if connection.send(command):
                            paused = not paused if started else False
                            started = True
            if args.demo:
                elapsed = frame / 60 if args.screenshot else now - start
                phase = (elapsed * 28 + 90) % 360
                a = phase if phase <= 180 else 360 - phase
                d = 32 + 7 * math.sin(math.radians(a * 4)) if 35 < a < 145 else -1
                samples = [(a, d)]
            else:
                samples = connection.poll(now)
                if not connection.ready:
                    started, paused = False, False
            for angle, distance in samples:
                last_sample = now
                trail.append((angle, now))
                bucket = round(angle / 1.4)
                if distance > 0:
                    echoes[bucket] = (angle, distance, now)
                else:
                    echoes.pop(bucket, None)
            # Draw in logical coordinates: text, margins and marks scale together.
            display_size = display.get_size()
            if display_size != rendered_size:
                ui_scale = min(display_size[0] / 1100, display_size[1] / 720)
                logical_size = tuple(round(value / ui_scale) for value in display_size)
                screen = pygame.Surface(logical_size).convert()
                rendered_size = display_size
            screen.fill((0, 13, 5))
            w, h = screen.get_size()
            radius = max(40, min((w - 100) / 2, h - 235))
            origin = (w // 2, int(h - 125))

            def point(a, r):
                radians = math.radians(a)
                return (round(origin[0] - math.cos(radians) * r), round(origin[1] - math.sin(radians) * r))

            for i in range(1, 5):
                r = radius * i / 4
                pygame.draw.arc(screen, (0, 112, 34), (origin[0]-r, origin[1]-r, r*2, r*2), 0, math.pi, 1)
                label(f'{scale*i/4:g} cm', (origin[0]+r-48, origin[1]+12), compact=True)
            for a in (0, 30, 60, 90, 120, 150, 180):
                pygame.draw.line(screen, (0, 100, 29), origin, point(a, radius))
                x, y = point(a, radius + 22)
                label(f'{a}°', (x-15, y-8), compact=True)
            for i, (a, stamp) in enumerate(trail):
                fade = max(0, 1 - (now-stamp)/2)
                strength = int(95 * (i+1)/max(1, len(trail)) * fade)
                pygame.draw.line(screen, (0, strength, 10), origin, point(a, radius), 2)
            if last_sample and now-last_sample < 2:
                pygame.draw.line(screen, green, origin, point(angle, radius), 2)
            for bucket, (a, d, stamp) in list(echoes.items()):
                age = now - stamp
                if age > 22:
                    del echoes[bucket]
                elif d <= scale:
                    color = (int(255 * max(0.2, 1-age/22)), 35, 32)
                    pygame.draw.circle(screen, color, point(a, radius*d/scale), 4)
            pygame.draw.circle(screen, green, origin, 4)
            label('SONAR / HC-SR04', (28, 22))
            label(f'Ângulo    {angle:5.1f}°' if last_sample else 'Ângulo       --', (28, 58))
            measurement = f'{distance:.1f} cm' if distance > 0 else 'sem eco'
            if not last_sample or now-last_sample > 2:
                measurement = '--'
            label(f'Distância {measurement}', (28, 88))
            label('DEMONSTRAÇÃO' if args.demo else 'USB / 115200', (max(28, w-225), 25), compact=True)
            if args.demo:
                status = 'Dados simulados · nenhum sensor conectado'
            elif not started:
                status = connection.status
            elif paused:
                status = 'Pausado · ESPAÇO retoma'
            elif now-last_sample > 2:
                status = 'Aguardando leituras do sensor…'
            else:
                status = f'Varredura ativa · {args.port}'
            label(status, (28, h-65), compact=True)
            label('ESPAÇO iniciar/pausar   +/− alcance   C limpar   F11 tela cheia   ESC voltar/sair', (28, h-36), compact=True)
            pygame.transform.smoothscale(screen, display_size, display)
            pygame.display.flip()
            frame += 1
            if args.screenshot and frame >= 240:
                pygame.image.save(display, args.screenshot)
                running = False
            if not args.screenshot:
                clock.tick(60)
    finally:
        if connection:
            connection.send(b'P')
            connection.close()
        pygame.quit()


if __name__ == '__main__':
    main()
