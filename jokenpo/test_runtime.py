import threading
import time
import unittest
from runtime import LatestWorker, NetworkClient
from game import GestureGate


class RuntimeTests(unittest.TestCase):
    def test_slow_inference_keeps_only_newest_pending_frame(self):
        entered, release = threading.Event(), threading.Event()
        processed = []
        def process(value):
            processed.append(value)
            if value == 1:
                entered.set()
                release.wait(1)
            return value
        worker = LatestWorker(process)
        try:
            worker.submit(1)
            self.assertTrue(entered.wait(1))
            started = time.monotonic()
            for frame in range(2, 100):
                worker.submit(frame)
                worker.snapshot()
            self.assertLess(time.monotonic() - started, .05)
            release.set()
            deadline = time.monotonic() + 1
            while worker.snapshot() != 99 and time.monotonic() < deadline:
                time.sleep(.005)
            self.assertEqual(processed, [1, 99])
        finally:
            release.set()
            worker.close()

    def test_slow_wifi_does_not_block_preview_or_send_later_gestures(self):
        entered, release, sent = threading.Event(), threading.Event(), threading.Event()
        calls = []
        def request(base, path, fields=None):
            calls.append((path, fields))
            if not entered.is_set():
                entered.set()
                release.wait(1)
            else:
                sent.set()
            return {'state': 'countdown', 'round': 7}
        network = NetworkClient('http://esp', request)
        try:
            self.assertTrue(entered.wait(1))
            started = time.monotonic()
            for _ in range(100):
                network.update(2, time.monotonic() - 1)
                network.snapshot()
            self.assertLess(time.monotonic() - started, .05)
            release.set()
            self.assertTrue(sent.wait(1))
            self.assertEqual(calls[1], ('/status', None))
        finally:
            release.set()
            network.close()

    def test_brief_miss_does_not_reset_starting_move(self):
        gate = GestureGate()
        for gesture, now in [('Closed_Fist', 0), ('Closed_Fist', .1), ('None', .12),
                             ('Closed_Fist', .16), ('Closed_Fist', .35)]:
            trigger, _ = gate.update(gesture, now, True)
        self.assertTrue(trigger)

    def test_missing_hand_eventually_invalidates_move(self):
        gate = GestureGate()
        gate.update('Closed_Fist', 0, False)
        self.assertEqual(gate.update('Closed_Fist', .3, False)[1], 0)
        self.assertEqual(gate.update('None', .35, False)[1], 0)
        self.assertEqual(gate.update('None', .5, False)[1], -1)


    def test_start_preserves_first_move_while_network_is_busy(self):
        entered, release, sent = threading.Event(), threading.Event(), threading.Event()
        calls = []
        def request(base, path, fields=None):
            calls.append((path, fields))
            if len(calls) == 1:
                entered.set()
                release.wait(1)
            if path == '/start':
                sent.set()
            return {'state': 'standby', 'round': 0}
        network = NetworkClient('http://esp', request)
        try:
            self.assertTrue(entered.wait(1))
            network.update(1, time.monotonic(), start=True)
            network.update(-1, time.monotonic())
            network.update(2, time.monotonic(), start=True)
            release.set()
            self.assertTrue(sent.wait(1))
            self.assertEqual(calls[1], ('/start', {'move': 1}))
        finally:
            release.set()
            network.close()

    def test_stale_observation_cannot_start_new_round(self):
        calls = []
        def request(base, path, fields=None):
            calls.append((path, fields))
            return {'state': 'standby', 'round': 0}
        network = NetworkClient('http://esp', request)
        try:
            network.update(1, time.monotonic() - 2, start=True)
            time.sleep(.25)
            self.assertFalse(any(path == '/start' for path, _ in calls))
        finally:
            network.close()
