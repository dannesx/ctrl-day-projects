#!/usr/bin/env python3
"""Webcam local + MediaPipe; HTTP connects to the ESP32 on the same Wi-Fi."""
import argparse
import json
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

from game import GestureGate, recognize_gesture
from runtime import LatestWorker, NetworkClient

ROOT = Path(__file__).resolve().parent
MODEL_URL = "https://storage.googleapis.com/mediapipe-models/gesture_recognizer/gesture_recognizer/float16/1/gesture_recognizer.task"


def request(base, path, fields=None):
    data = urllib.parse.urlencode(fields).encode() if fields is not None else None
    req = urllib.request.Request(base + path, data=data)
    with urllib.request.urlopen(req, timeout=1.2) as response:
        return json.load(response)


def camera_source(value):
    if value != "auto":
        return int(value) if value.isdecimal() else value
    for device in sorted(Path("/sys/class/video4linux").glob("video*")):
        name = (device / "name").read_text().lower()
        index = (device / "index").read_text().strip()
        if index == "0" and "integrated" not in name:
            return str(Path("/dev") / device.name)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--esp", default="http://jokenpo.local", help="IP ou URL do ESP32")
    parser.add_argument("--camera", default="auto",
                        help="auto prefere webcam externa; aceita indice ou /dev/videoN")
    parser.add_argument("--confidence", type=float, default=0.5)
    parser.add_argument("--inference-width", type=int, default=640,
                        help="Largura processada; reduza para 320 em PCs lentos")
    args = parser.parse_args()
    if not 0 <= args.confidence <= 1 or args.inference_width < 160:
        parser.error("confidence deve estar entre 0 e 1; inference-width >= 160")
    base = args.esp if args.esp.startswith("http") else "http://" + args.esp
    base = base.rstrip("/")
    import cv2
    import mediapipe as mp
    from mediapipe.tasks import python
    from mediapipe.tasks.python import vision

    model = ROOT / "models" / "gesture_recognizer.task"
    if not model.exists():
        model.parent.mkdir(exist_ok=True)
        print("Baixando modelo oficial de gestos (apenas na primeira execucao)...")
        partial = model.with_suffix(".part")
        try:
            with urllib.request.urlopen(MODEL_URL, timeout=60) as response:
                partial.write_bytes(response.read())
            partial.replace(model)
        finally:
            partial.unlink(missing_ok=True)
    options = vision.GestureRecognizerOptions(
        base_options=python.BaseOptions(model_asset_path=str(model)),
        running_mode=vision.RunningMode.VIDEO,
        num_hands=1,
    )
    source = camera_source(args.camera)
    print(f"Camera selecionada: {source}", flush=True)
    device = int(source.removeprefix("/dev/video")) if isinstance(source, str) and source.startswith("/dev/video") and source.removeprefix("/dev/video").isdecimal() else source
    camera = cv2.VideoCapture(device, cv2.CAP_V4L2)
    if not camera.isOpened():
        camera.release()
        raise RuntimeError("Webcam indisponivel. Feche outros apps ou tente --camera 1.")
    camera.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
    camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 360)
    camera.set(cv2.CAP_PROP_FPS, 30)
    print(f"Captura: {camera.get(cv2.CAP_PROP_FRAME_WIDTH):.0f}x"
          f"{camera.get(cv2.CAP_PROP_FRAME_HEIGHT):.0f}, "
          f"{camera.get(cv2.CAP_PROP_FPS):.0f} FPS negociados (30 solicitados)", flush=True)
    camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    gate = GestureGate()
    network = NetworkClient(base, request)
    worker = None
    try:
        with vision.GestureRecognizer.create_from_options(options) as recognizer:
            last_timestamp = 0

            def infer(sample):
                nonlocal last_timestamp
                raw, captured_at = sample
                width = min(args.inference_width, raw.shape[1])
                small = cv2.resize(raw, (width, int(raw.shape[0] * width / raw.shape[1])))
                image = mp.Image(image_format=mp.ImageFormat.SRGB,
                                 data=cv2.cvtColor(small, cv2.COLOR_BGR2RGB))
                last_timestamp = max(last_timestamp + 1, int(captured_at * 1000))
                result = recognizer.recognize_for_video(image, last_timestamp)
                return result, captured_at

            worker = LatestWorker(infer)
            try:
                run_camera(camera, cv2, worker, network, gate, args)
            finally:
                worker.close()
    finally:
        network.close()
        camera.release()
        cv2.destroyAllWindows()


def run_camera(camera, cv2, worker, network, gate, args):
    previous_observation = 0.0
    fps = 0.0
    previous_frame = time.monotonic()
    while True:
        ok, frame = camera.read()
        if not ok:
            raise RuntimeError("A webcam parou de fornecer imagens.")
        now = time.monotonic()
        fps = .9 * fps + .1 / max(now - previous_frame, .001)
        previous_frame = now
        # Analyze the original camera image; mirror only the preview.
        worker.submit((frame.copy(), now))
        frame = cv2.flip(frame, 1)
        state, network_error = network.snapshot()
        sample = worker.snapshot()
        gesture, confidence = "None", 0.0
        observed_at = now
        landmarks = []
        if sample is not None:
            result, observed_at = sample
            if now - observed_at <= .35:
                gesture, confidence = recognize_gesture(result, args.confidence)
                landmarks = result.hand_landmarks[0] if result.hand_landmarks else []
        if observed_at != previous_observation or now - observed_at > .35:
            trigger, move = gate.update(gesture, now, state["state"] == "standby")
            network.update(move, observed_at, start=trigger)
            previous_observation = observed_at
        label = {"Closed_Fist": "PEDRA",
                 "Open_Palm": "PAPEL", "Victory": "TESOURA"}.get(gesture, "---")
        phase = state["state"]
        if phase == "countdown":
            phase = ["JO", "KEN", "PO!!"][min(state["elapsed_ms"] // 700, 2)]
        elif phase == "result":
            names = ["PEDRA", "PAPEL", "TESOURA"]
            player = state["player"]
            phase = f"Voce: {names[player] if player >= 0 else '---'} ESP: {names[state['computer']]} | {state['result']}"
        if landmarks:
            joints = [(int((1 - p.x) * frame.shape[1]), int(p.y * frame.shape[0])) for p in landmarks]
            for chain in ((0,1,2,3,4), (0,5,6,7,8), (5,9,10,11,12),
                          (9,13,14,15,16), (13,17,18,19,20), (0,17)):
                for a, b in zip(chain, chain[1:]):
                    cv2.line(frame, joints[a], joints[b], (60, 220, 60), 2)
            for point in joints:
                cv2.circle(frame, point, 3, (0, 220, 255), -1)
        hand_status = "Mao nao detectada: mantenha inteira na imagem"
        if landmarks:
            if gesture in ("Closed_Fist", "Open_Palm", "Victory"):
                hand_status = "Gesto reconhecido: iniciando rodada"
            else:
                hand_status = "Mao detectada: ajuste a pose dos dedos"
        if landmarks and not gate.armed and phase == "standby":
            hand_status = "Iniciando rodada... mantenha o gesto"
        detail = "dedos" if confidence is None else f"{confidence:.0%}"
        if gesture == "None":
            detail = "sem gesto"
        lines = [f"Gesto: {label} ({detail}) | Camera: {fps:.0f} FPS", str(phase),
                 hand_status, "Q ou ESC para sair"]
        if state["state"] == "countdown" and state.get("player", -1) >= 0:
            chosen = ["PEDRA", "PAPEL", "TESOURA"][state["player"]]
            lines[0] = f"Jogada: {chosen} (confirmada) | Camera: {fps:.0f} FPS"
            lines[2] = "Escolha salva: pode retirar a mao"
        if network_error:
            lines[1] = "ESP desconectado: confira IP e Wi-Fi"
        for index, line in enumerate(lines):
            cv2.putText(frame, line, (12, 28 + index * 26), cv2.FONT_HERSHEY_SIMPLEX,
                        0.5, (0, 0, 0), 3)
            cv2.putText(frame, line, (12, 28 + index * 26), cv2.FONT_HERSHEY_SIMPLEX,
                        0.5, (255, 255, 255), 1)
        cv2.imshow("Jokenpo", frame)
        if cv2.waitKey(1) & 0xFF in (27, ord("q")):
            break
        try:
            if cv2.getWindowProperty("Jokenpo", cv2.WND_PROP_VISIBLE) < 1:
                break
        except cv2.error:
            # Qt destroys its receiver when the last window is closed.
            break


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ImportError, OSError, ValueError) as error:
        raise SystemExit(f"Erro: {error}") from error
