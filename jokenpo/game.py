"""Small dependency-free gesture debounce, also used by the webcam client."""
MOVES = {"Closed_Fist": 0, "Open_Palm": 1, "Victory": 2}


class GestureGate:
    def __init__(self):
        self.gesture = "None"
        self.since = 0.0
        self.armed = True
        self.was_standby = False
        self.last_valid_at = float("-inf")

    def update(self, gesture, now, standby):
        if standby and not self.was_standby:
            # A new round accepts the same pose, but requires fresh stability.
            self.armed = True
            self.gesture = "None"
            self.since = now
            self.last_valid_at = float("-inf")
        self.was_standby = standby
        # Smooth brief misses, but never retain a gesture from an old observation.
        if gesture == "None" and now - self.last_valid_at < 0.15:
            gesture = self.gesture
        elif gesture != "None":
            self.last_valid_at = now
        if gesture != self.gesture:
            self.gesture, self.since = gesture, now
        trigger = standby and self.armed and gesture in MOVES and now - self.since >= 0.3
        if trigger:
            self.armed = False
        move = MOVES.get(gesture, -1) if now - self.since >= 0.2 else -1
        return trigger, move


def finger_gesture(points):
    """Classify four fingers using 3D joint geometry, independent of palm side.

    Points are MediaPipe world landmarks. Ambiguous/bent fingers reject the
    pose rather than turning a transition or pointing hand into a game move.
    """
    from math import dist, sqrt

    if len(points) != 21:
        return "None"
    xyz = [(p.x, p.y, p.z) for p in points]
    fingers = []
    for knuckle, joint, tip in [(5, 6, 8), (9, 10, 12), (13, 14, 16), (17, 18, 20)]:
        a = [x - y for x, y in zip(xyz[knuckle], xyz[joint])]
        b = [x - y for x, y in zip(xyz[tip], xyz[joint])]
        length = sqrt(sum(x*x for x in a) * sum(x*x for x in b))
        radius = dist(xyz[joint], xyz[0])
        if length < 1e-12 or radius < 1e-9:
            return "None"
        cosine = sum(x*y for x, y in zip(a, b)) / length
        reach = dist(xyz[tip], xyz[0]) / radius
        if cosine < -0.5 and reach > 1.2:  # extended, allowing moderate curvature
            fingers.append(True)
        elif reach < 1.1 or (cosine > -0.5 and reach < 1.25):  # folded
            fingers.append(False)
        else:
            return "None"
    return {
        (False, False, False, False): "Closed_Fist",
        (True, True, True, True): "Open_Palm",
        (True, True, False, False): "Victory",
    }.get(tuple(fingers), "None")


def recognize_gesture(result, minimum_confidence=0.5):
    """Return only rock, paper or scissors; unsupported poses are ignored."""
    best = result.gestures[0][0] if result.gestures and result.gestures[0] else None
    if (best and best.category_name not in (*MOVES, "None")
            and best.score >= max(.65, minimum_confidence)):
        return "None", best.score
    if result.hand_world_landmarks:
        gesture = finger_gesture(result.hand_world_landmarks[0])
        if gesture != "None":
            return gesture, None  # Geometry is a rule, not a confidence score.
        if best and best.category_name in MOVES and best.score >= max(.65, minimum_confidence):
            return best.category_name, best.score
    return "None", best.score if best else 0.0
