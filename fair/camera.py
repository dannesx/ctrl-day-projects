"""Capture and recognition each have one background thread and no frame backlog."""
import sys
import logging
import threading
import time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'jokenpo'))
from runtime import LatestWorker
from game import recognize_gesture


class Camera:
    def __init__(self, index, model):
        self.index, self.model = index, model
        self.stop = threading.Event()
        self.lock = threading.Lock()
        self.latest = None
        self.error = 'Abrindo câmera…'
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def snapshot(self):
        with self.lock:
            return self.latest, self.error

    def run(self):
        camera = None
        worker = None
        try:
            import cv2
            import mediapipe as mp
            from mediapipe.tasks import python
            from mediapipe.tasks.python import vision
            options = vision.GestureRecognizerOptions(
                base_options=python.BaseOptions(model_asset_path=str(self.model)),
                running_mode=vision.RunningMode.VIDEO, num_hands=1)
            with vision.GestureRecognizer.create_from_options(options) as recognizer:
                stamp = 0

                def infer(sample):
                    nonlocal stamp
                    frame, captured = sample
                    stamp = max(stamp+1, int(captured*1000))
                    result = recognizer.recognize_for_video(mp.Image(
                        image_format=mp.ImageFormat.SRGB,
                        data=cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)), stamp)
                    return result, captured

                worker = LatestWorker(infer)
                while not self.stop.is_set():
                    if camera is None:
                        camera = cv2.VideoCapture(self.index, cv2.CAP_DSHOW if sys.platform == 'win32' else cv2.CAP_ANY)
                        camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
                        camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 360)
                        camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)
                    ok, frame = camera.read()
                    if not ok:
                        with self.lock:
                            self.latest = None
                            self.error = 'Câmera indisponível: feche outros apps; confira Configurações > Privacidade > Câmera.'
                        camera.release()
                        camera = None
                        self.stop.wait(2)
                        continue
                    now = time.monotonic()
                    worker.submit((frame.copy(), now))
                    sample = worker.snapshot()
                    gesture, observed, points = 'None', 0, []
                    if sample and now-sample[1] <= .35:
                        result, observed = sample
                        gesture, _ = recognize_gesture(result)
                        points = result.hand_landmarks[0] if result.hand_landmarks else []
                    frame = cv2.flip(frame, 1)
                    joints = [(int((1-p.x)*frame.shape[1]), int(p.y*frame.shape[0])) for p in points]
                    for chain in ((0,1,2,3,4),(0,5,6,7,8),(5,9,10,11,12),(9,13,14,15,16),(13,17,18,19,20),(0,17)):
                        for a, b in zip(chain, chain[1:]):
                            if joints:
                                cv2.line(frame, joints[a], joints[b], (50,230,70), 2)
                    for point in joints:
                        cv2.circle(frame, point, 3, (50,230,70), -1)
                    with self.lock:
                        self.latest = (cv2.cvtColor(frame, cv2.COLOR_BGR2RGB), gesture, observed)
                        self.error = ''
        except Exception as error:
            logging.exception('Falha ao iniciar ou executar o reconhecimento')
            with self.lock:
                self.error = 'Reconhecimento indisponível. Execute o iniciador; detalhes no log.'
                self.latest = None
        finally:
            if worker:
                worker.close()
            if camera:
                camera.release()

    def close(self):
        self.stop.set()
        self.thread.join(2)
