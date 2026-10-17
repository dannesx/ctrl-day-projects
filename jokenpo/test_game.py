import unittest
from game import GestureGate


class GestureTests(unittest.TestCase):
    def test_hold_does_not_repeat_and_release_rearms(self):
        gate = GestureGate()
        self.assertFalse(gate.update('Thumb_Up', 0, True)[0])
        self.assertTrue(gate.update('Thumb_Up', 0.7, True)[0])
        self.assertFalse(gate.update('Thumb_Up', 9, True)[0])
        gate.update('None', 10, True)
        gate.update('None', 10.6, True)
        gate.update('Thumb_Up', 11, True)
        self.assertTrue(gate.update('Thumb_Up', 11.7, True)[0])

    def test_moves_need_stability_and_unknown_invalidates(self):
        gate = GestureGate()
        for gesture, move in [('Closed_Fist', 0), ('Open_Palm', 1), ('Victory', 2)]:
            self.assertEqual(gate.update(gesture, move * 2, False)[1], -1)
            self.assertEqual(gate.update(gesture, move * 2 + 0.3, False)[1], move)
        self.assertEqual(gate.update('None', 8, False)[1], -1)

    def test_cannot_start_during_round(self):
        gate = GestureGate()
        gate.update('Thumb_Up', 0, False)
        self.assertFalse(gate.update('Thumb_Up', 1, False)[0])


if __name__ == '__main__':
    unittest.main()
