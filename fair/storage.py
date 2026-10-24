"""Durable sessions and boot-scoped result deduplication. No camera data."""
import json
import sqlite3

PROJECTS = ('jokenpo', 'tic-tac-toe')


def valid_message(message):
    return (isinstance(message, dict) and type(message.get('v')) is int and message.get('v') == 1
            and message.get('project') in PROJECTS
            and all(isinstance(message.get(k), str) and 0 < len(message[k]) < 128
                    for k in ('device', 'boot'))
            and type(message.get('game')) is int and message['game'] >= 0
            and message.get('type') in ('hello', 'state', 'result')
            and (message['type'] != 'result' or
                 (message.get('outcome') in ('win', 'draw', 'loss')
                  and (message['project'] != 'jokenpo' or
                       all(type(message.get(k)) is int and message[k] in (0, 1, 2)
                           for k in ('player', 'computer')))
                  and (message['project'] != 'tic-tac-toe' or
                       message.get('mode') in ('random', 'trained')))))


class Store:
    def __init__(self, path):
        self.db = sqlite3.connect(path)
        self.db.executescript('''
          PRAGMA journal_mode=WAL;
          CREATE TABLE IF NOT EXISTS sessions(id INTEGER PRIMARY KEY, started TEXT DEFAULT CURRENT_TIMESTAMP);
          CREATE TABLE IF NOT EXISTS results(
            project TEXT, device TEXT, boot TEXT, game INTEGER,
            session INTEGER REFERENCES sessions(id), payload TEXT,
            received TEXT DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY(project,device,boot,game));
        ''')
        row = self.db.execute('SELECT max(id) FROM sessions').fetchone()
        self.session = row[0] or self.new_session()

    def new_session(self):
        with self.db:
            self.session = self.db.execute('INSERT INTO sessions DEFAULT VALUES').lastrowid
        return self.session

    def record(self, message):
        if not valid_message(message) or message['type'] != 'result':
            raise ValueError('Resultado inválido')
        with self.db:
            cursor = self.db.execute('INSERT OR IGNORE INTO results(project,device,boot,game,session,payload) VALUES(?,?,?,?,?,?)',
                                     (*[message[k] for k in ('project', 'device', 'boot', 'game')],
                                      self.session, json.dumps(message)))
        return cursor.rowcount == 1

    def results(self, project):
        return [json.loads(r[0]) for r in self.db.execute(
            'SELECT payload FROM results WHERE session=? AND project=? ORDER BY rowid',
            (self.session, project))]

    def close(self):
        self.db.close()
