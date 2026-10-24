"""Nonblocking serial discovery/reconnect, with acknowledgements after DB commit."""
import json
import logging
import queue
import threading
import time
from storage import valid_message


class SerialHub:
    def __init__(self, config):
        self.config = dict(config)
        self.lock = threading.Lock()
        self.events = queue.Queue()
        self.commands = queue.Queue()
        self.stop = threading.Event()
        self.states = {}
        self.ports = []
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def configure(self, config):
        with self.lock:
            self.config = dict(config)

    def snapshot(self):
        with self.lock:
            return {k: (dict(v[0]), v[1]) for k, v in self.states.items()}, list(self.ports)

    def send(self, message):
        self.commands.put(message)

    def run(self):
        import serial
        from serial.tools import list_ports
        connections, buffers, identities = {}, {}, {}
        scan_at = 0
        try:
            while not self.stop.wait(.01):
                now = time.monotonic()
                if now >= scan_at:
                    scan_at = now + 2
                    available = list(list_ports.comports())
                    with self.lock:
                        self.ports = [p.device for p in available]
                    for port in available:
                        if port.device in connections:
                            continue
                        try:
                            link = serial.Serial(baudrate=115200, timeout=0, write_timeout=.1)
                            link.dtr = False
                            link.rts = False
                            link.port = port.device
                            link.open()
                            link.write(b'{"v":1,"type":"hello"}\n')
                            connections[port.device] = link
                            buffers[port.device] = b''
                        except (OSError, serial.SerialException):
                            logging.debug('Porta indisponível: %s', port.device)
                with self.lock:
                    config = dict(self.config)
                for port, link in list(connections.items()):
                    try:
                        chunk = link.read(4096)
                        buffers[port] += chunk
                        while b'\n' in buffers[port]:
                            line, buffers[port] = buffers[port].split(b'\n', 1)
                            try:
                                message = json.loads(line)
                            except (ValueError, UnicodeError):
                                continue
                            if not valid_message(message):
                                continue
                            project = message['project']
                            selected = config.get(project, 'auto')
                            if selected not in ('auto', port):
                                continue
                            identities[port] = message
                            with self.lock:
                                old = self.states.get(project)
                                if selected == 'auto' and old and old[0].get('_port') != port and now-old[1] < 3:
                                    continue
                                message['_port'] = port
                                if message['type'] == 'state':
                                    self.states[project] = (message, now)
                            if message['type'] == 'result':
                                self.events.put(message)
                        if len(buffers[port]) > 16384:
                            buffers[port] = b''
                    except (OSError, serial.SerialException):
                        link.close()
                        del connections[port]
                        identities.pop(port, None)
                while not self.commands.empty():
                    command = self.commands.get_nowait()
                    for port, identity in list(identities.items()):
                        if all(command.get(k) == identity.get(k) for k in ('project', 'device', 'boot')):
                            try:
                                connections[port].write((json.dumps(command)+'\n').encode())
                            except (OSError, serial.SerialException):
                                logging.exception('Falha ao enviar comando')
        finally:
            for link in connections.values():
                link.close()

    def close(self):
        self.stop.set()
        self.thread.join(3)
