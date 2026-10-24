import unittest
from charts import aggregate, SessionCharts


class ChartTests(unittest.TestCase):
    def test_empty(self):
        for dist in aggregate([], []):
            self.assertEqual(dist.total, 0)
            self.assertIsNone(dist.percentages)

    def test_player_perspective_and_denominators(self):
        rows = [dict(player=0, computer=2, outcome='win'),
                dict(player=0, computer=0, outcome='draw'),
                dict(player=1, computer=2, outcome='loss')]
        modes = [dict(mode='random', outcome='win'), dict(mode='random', outcome='draw'),
                 dict(mode='trained', outcome='loss')]
        player, esp, outcomes, random, trained = aggregate(rows, modes)
        self.assertEqual(player.counts, (2, 1, 0))
        self.assertEqual(esp.counts, (1, 0, 2))
        self.assertEqual(outcomes.counts, (1, 1, 1))
        self.assertEqual(random.percentages, (50, 50, 0))
        self.assertEqual(trained.percentages, (0, 0, 100))
        for dist in (player, esp, outcomes, random, trained):
            self.assertAlmostEqual(sum(dist.percentages), 100)
        single = aggregate(rows[:1], modes[:1])
        self.assertEqual(single[2].percentages, (100, 0, 0))
        self.assertIsNone(single[4].percentages)

    def test_render_cache_and_session_reset(self):
        charts = SessionCharts()
        initial = charts.images(1, [], [])
        for _ in range(30):
            self.assertEqual(charts.images(1, [], []), initial)
        self.assertEqual(charts.render_count, 3)
        charts.images(1, [dict(player=0, computer=2, outcome='win')], [])
        self.assertEqual(charts.render_count, 5)
        charts.images(2, [], [])
        self.assertEqual(charts.render_count, 8)


if __name__ == '__main__':
    unittest.main()
