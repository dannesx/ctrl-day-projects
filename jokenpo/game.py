"""Small dependency-free gesture debounce, also used by the webcam client."""
MOVES = {"Closed_Fist": 0, "Open_Palm": 1, "Victory": 2}


class GestureGate:
    def __init__(self):
        self.gesture = "None"
        self.since = 0.0
        self.armed = True
        self.release_since = None
        self.last_valid_at = float("-inf")

    def update(self, gesture, now, standby):
        raw_gesture = gesture
        # Smooth brief misses, but never retain a gesture from an old observation.
        if gesture == "None" and now - self.last_valid_at < 0.15:
            gesture = self.gesture
        elif gesture != "None":
            self.last_valid_at = now
        if gesture != self.gesture:
            self.gesture, self.since = gesture, now
        if raw_gesture != "Thumb_Up":
            if self.release_since is None:
                self.release_since = now
            if now - self.release_since >= 0.5:
                self.armed = True
        else:
            self.release_since = None
        trigger = standby and self.armed and gesture == "Thumb_Up" and now - self.since >= 0.6
        if trigger:
            self.armed = False
        move = MOVES.get(gesture, -1) if now - self.since >= 0.2 else -1
        return trigger, move
