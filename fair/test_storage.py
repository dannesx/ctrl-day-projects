import tempfile
from pathlib import Path
import unittest
from storage import Store, valid_message


def result(**fields):
    return {'v':1,'project':'jokenpo','device':'esp-a','boot':'boot-1','game':1,
            'type':'result','outcome':'win','player':1,'computer':0,**fields}


class PersistenceTests(unittest.TestCase):
    def test_reconnect_reopen_new_session_and_reboot(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'test.sqlite3'
            store=Store(path)
            first=store.session
            self.assertTrue(store.record(result()))
            self.assertFalse(store.record(result()))
            store.close()
            store=Store(path)
            self.assertEqual(store.session,first)
            self.assertEqual(len(store.results('jokenpo')),1)
            self.assertFalse(store.record(result()))
            store.new_session()
            self.assertEqual(store.results('jokenpo'),[])
            # Pending retransmissions must never move into a new session.
            self.assertFalse(store.record(result()))
            self.assertTrue(store.record(result(boot='boot-2')))
            self.assertTrue(store.record(result(device='esp-b')))
            self.assertEqual(len(store.results('jokenpo')),2)
            self.assertEqual(store.db.execute('SELECT count(*) FROM results WHERE session=?',(first,)).fetchone()[0],1)
            store.close()

    def test_training_states_are_not_visitor_results(self):
        store=Store(':memory:')
        with self.assertRaises(ValueError):
            store.record(result(type='state',episodes=200000,training=True))
        self.assertEqual(store.results('tic-tac-toe'),[])
        store.close()

    def test_malformed_and_unknown_versions_are_rejected(self):
        for message in (None,[],{},result(v=2),result(game=True),result(player=9),
                        result(boot=''),result(outcome='garbage'),result(project='unrelated'),
                        result(project='tic-tac-toe',mode='simulated')):
            self.assertFalse(valid_message(message),message)


if __name__=='__main__':unittest.main()
