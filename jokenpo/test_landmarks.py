"""Regression fixtures extracted from the user's three back-of-hand images."""
import json
import unittest
from pathlib import Path
from types import SimpleNamespace as Point
from game import finger_gesture, recognize_gesture


class LandmarkTests(unittest.TestCase):
    def test_reference_poses_both_sides_and_rotations(self):
        fixtures = json.loads((Path(__file__).parent / 'tests/back_hand_landmarks.json').read_text())
        for fixture in fixtures:
            for mirrored in (1, -1):
                for rotated in (False, True):
                    points = []
                    for x, y, z in fixture['points']:
                        if rotated:
                            x, y = -y, x
                        points.append(Point(x=mirrored*x, y=y, z=z))
                    result = Point(gestures=[[Point(category_name='None', score=.8)]],
                                   hand_world_landmarks=[points])
                    with self.subTest(pose=fixture['expected'], mirrored=mirrored, rotated=rotated):
                        self.assertEqual(recognize_gesture(result), (fixture['expected'], None))

    def test_missing_and_degenerate_hands_are_invalid(self):
        self.assertEqual(finger_gesture([]), 'None')
        self.assertEqual(finger_gesture([Point(x=0,y=0,z=0)]*21), 'None')
        result = Point(gestures=[], hand_world_landmarks=[])
        self.assertEqual(recognize_gesture(result), ('None', 0))

    def test_joinha_is_ignored(self):
        result = Point(gestures=[[Point(category_name='Thumb_Up', score=.9)]],
                       hand_world_landmarks=[])
        self.assertEqual(recognize_gesture(result), ('None', .9))
        result.gestures[0][0].score = .2
        self.assertEqual(recognize_gesture(result)[0], 'None')

    def test_confident_trained_gesture_can_resolve_ambiguous_geometry(self):
        points = [Point(x=0, y=0, z=0)] * 21
        for gesture in ('Closed_Fist', 'Open_Palm', 'Victory'):
            result = Point(gestures=[[Point(category_name=gesture, score=.92)]],
                           hand_world_landmarks=[points])
            self.assertEqual(recognize_gesture(result), (gesture, .92))
            result.gestures[0][0].score = .4
            self.assertEqual(recognize_gesture(result)[0], 'None')

    def test_slightly_bent_paper_fingers(self):
        points = [Point(x=0, y=0, z=0) for _ in range(21)]
        for x, (m, p, d, t) in zip((-.6, -.2, .2, .6), ((5,6,7,8),(9,10,11,12),(13,14,15,16),(17,18,19,20))):
            points[m] = Point(x=x,y=1,z=0)
            points[p] = Point(x=x,y=2,z=0)
            points[d] = Point(x=x+.6,y=2.5,z=0)
            points[t] = Point(x=x+1.15,y=3,z=0)
        self.assertEqual(finger_gesture(points), 'Open_Palm')

    def test_confident_model_without_a_hand_does_not_play(self):
        result = Point(gestures=[[Point(category_name='Victory', score=.99)]],
                       hand_world_landmarks=[])
        self.assertEqual(recognize_gesture(result)[0], 'None')


    def test_other_confident_poses_are_not_misread_as_fists(self):
        fixture = json.loads((Path(__file__).parent / 'tests/back_hand_landmarks.json').read_text())[0]
        points = [Point(x=x,y=y,z=z) for x,y,z in fixture['points']]
        for gesture in ('Thumb_Up', 'Thumb_Down', 'Pointing_Up', 'ILoveYou'):
            result = Point(gestures=[[Point(category_name=gesture,score=.95)]],
                           hand_world_landmarks=[points])
            self.assertEqual(recognize_gesture(result)[0], 'None')
