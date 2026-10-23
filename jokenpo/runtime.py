"""Bounded background workers: a slow task never queues old camera frames."""
import threading
import time


class LatestWorker:
    def __init__(self, process):
        self.process = process
        self.condition = threading.Condition()
        self.pending = None
        self.result = None
        self.error = None
        self.stopped = False
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def submit(self, value):
        with self.condition:
            self.pending = value
            self.condition.notify()

    def snapshot(self):
        with self.condition:
            if self.error:
                raise RuntimeError(str(self.error)) from self.error
            return self.result

    def _run(self):
        while True:
            with self.condition:
                self.condition.wait_for(lambda: self.stopped or self.pending is not None)
                if self.stopped:
                    return
                value, self.pending = self.pending, None
            try:
                result = self.process(value)
            except Exception as error:
                with self.condition:
                    self.error = error
                return
            with self.condition:
                self.result = result

    def close(self):
        with self.condition:
            self.stopped = True
            self.condition.notify()
        self.thread.join(timeout=2)


class NetworkClient:
    def __init__(self, base, request):
        self.base, self.request = base, request
        self.lock = threading.Lock()
        self.stop = threading.Event()
        self.state = {"state": "offline"}
        self.error = ""
        self.pending_move = None
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def update(self, move, observed_at, start=False):
        with self.lock:
            if start and move in (0, 1, 2) and time.monotonic() - observed_at <= .35:
                if self.pending_move is None:
                    self.pending_move = move

    def snapshot(self):
        with self.lock:
            return dict(self.state), self.error

    def _run(self):
        while not self.stop.is_set():
            with self.lock:
                move, self.pending_move = self.pending_move, None
            try:
                if move is not None:
                    # Consume once, even on timeout: never replay an ambiguous start.
                    response = self.request(self.base, "/start", {"move": move})
                else:
                    response = self.request(self.base, "/status")
                with self.lock:
                    self.state, self.error = response, ""
            except (OSError, ValueError) as error:
                with self.lock:
                    self.state, self.error = {"state": "offline"}, str(error)
            self.stop.wait(.1 if not self.error else .5)

    def close(self):
        self.stop.set()
        self.thread.join(timeout=2)
