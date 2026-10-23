import unittest
from game import GestureGate


class GestureTests(unittest.TestCase):
    def test_pending_start_does_not_repeat_when_pose_changes(self):
        gate = GestureGate()
        self.assertFalse(gate.update('Closed_Fist', 0, True)[0])
        self.assertTrue(gate.update('Closed_Fist', 0.7, True)[0])
        self.assertFalse(gate.update('Closed_Fist', 9, True)[0])
        gate.update('None', 10, True)
        gate.update('None', 10.6, True)
        gate.update('Closed_Fist', 11, True)
        self.assertFalse(gate.update('Closed_Fist', 11.7, True)[0])

    def test_each_move_starts_after_stability(self):
        for gesture in ('Closed_Fist', 'Open_Palm', 'Victory'):
            with self.subTest(gesture=gesture):
                gate = GestureGate()
                self.assertFalse(gate.update(gesture, 0, True)[0])
                self.assertTrue(gate.update(gesture, .35, True)[0])

    def test_changes_during_round_do_not_rearm(self):
        gate = GestureGate()
        gate.update('Closed_Fist', 0, True)
        self.assertTrue(gate.update('Closed_Fist', .35, True)[0])
        gate.update('None', 1, False)
        gate.update('None', 2, False)
        gate.update('Closed_Fist', 3, False)
        self.assertFalse(gate.update('Closed_Fist', 9, True)[0])
        gate.update('Victory', 10, True)
        self.assertTrue(gate.update('Victory', 10.6, True)[0])

    def test_same_paper_restarts_after_result_without_removing_hand(self):
        gate = GestureGate()
        gate.update('Open_Palm', 0, True)
        self.assertTrue(gate.update('Open_Palm', .35, True)[0])
        self.assertFalse(gate.update('Open_Palm', .5, True)[0])
        self.assertFalse(gate.update('Open_Palm', 1, False)[0])
        self.assertFalse(gate.update('Open_Palm', 4, False)[0])
        self.assertFalse(gate.update('Open_Palm', 6, True)[0])
        self.assertTrue(gate.update('Open_Palm', 6.35, True)[0])
        self.assertFalse(gate.update('Open_Palm', 6.5, True)[0])

    def test_joinha_does_not_start(self):
        gate = GestureGate()
        gate.update('Thumb_Up', 0, True)
        self.assertFalse(gate.update('Thumb_Up', 1, True)[0])

    def test_moves_need_stability_and_unknown_invalidates(self):
        gate = GestureGate()
        for gesture, move in [('Closed_Fist', 0), ('Open_Palm', 1), ('Victory', 2)]:
            self.assertEqual(gate.update(gesture, move * 2, False)[1], -1)
            self.assertEqual(gate.update(gesture, move * 2 + 0.3, False)[1], move)
        self.assertEqual(gate.update('None', 8, False)[1], -1)

    def test_cannot_start_during_round(self):
        gate = GestureGate()
        gate.update('Closed_Fist', 0, False)
        self.assertFalse(gate.update('Closed_Fist', 1, False)[0])


if __name__ == '__main__':
    unittest.main()
