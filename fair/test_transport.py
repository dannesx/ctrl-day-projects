"""Exercise real discovery worker with fake serial endpoints, including reconnect."""
import json
import queue
import threading
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import serial
from serial.tools import list_ports
from storage import PROJECTS
from transport import SerialHub


class SerialTests(unittest.TestCase):
    def test_reversed_ports_diagnostics_repeat_ack_and_reconnect(self):
        endpoints={}
        written=[]

        class Link:
            def __init__(self,**kwargs):
                self.data=queue.Queue();self.closed=False
            def open(self):
                self.failed=False;endpoints[self.port]=self
                p=PROJECTS[1] if self.port=='COM1' else PROJECTS[0]
                self.identity={'v':1,'type':'state','project':p,'device':p,'boot':'boot','game':1,
                               'state':'standby','board':'---------','mode':'random'}
                self.data.put(b'Wi-Fi diagnostic\n{"v":99}\n'+json.dumps(self.identity).encode()+b'\n')
            def read(self,n):
                if self.failed:raise serial.SerialException('unplugged')
                try:return self.data.get_nowait()
                except queue.Empty:return b''
            def write(self,data):written.append((self.port,json.loads(data)))
            def close(self):self.closed=True

        def wait_for(predicate):
            until=time.monotonic()+5
            while time.monotonic()<until:
                if predicate():return
                time.sleep(.02)
            self.fail('Discovery/reconnect deadline exceeded')

        with patch.object(serial,'Serial',Link),patch.object(list_ports,'comports',lambda:[SimpleNamespace(device=p) for p in ('COM1','COM9')]):
            hub=SerialHub({})
            try:
                wait_for(lambda:len(hub.snapshot()[0])==2)
                states,_=hub.snapshot()
                self.assertEqual(states['jokenpo'][0]['_port'],'COM9')
                self.assertEqual(states['tic-tac-toe'][0]['_port'],'COM1')
                link=endpoints['COM9']
                event={**link.identity,'type':'result','outcome':'win','player':1,'computer':0}
                raw=json.dumps(event).encode()+b'\n'
                link.data.put(raw[:12]);link.data.put(raw[12:]+raw)
                wait_for(lambda:hub.events.qsize()==2)
                # Replayed results cannot replace the live standby state.
                self.assertEqual(hub.snapshot()[0]['jokenpo'][0]['state'],'standby')
                ack={**{k:event[k] for k in ('v','project','device','boot','game')},'type':'ack'}
                hub.send(ack)
                wait_for(lambda:any(m==ack for _,m in written))
                link.failed=True
                wait_for(lambda:endpoints['COM9'] is not link)
                self.assertTrue(link.closed)
                hub.configure({'jokenpo':'COM1'})
                # A manually selected mismatched board never identifies as Jokenpo.
                endpoints['COM9'].data.put(json.dumps(endpoints['COM9'].identity).encode()+b'\n')
                time.sleep(.05)
            finally:hub.close()
            self.assertFalse(hub.thread.is_alive())


if __name__=='__main__':unittest.main()
