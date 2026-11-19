import os
import secrets
from pathlib import Path


def prepare(state, owner=(10001, 10001)):
    state = Path(state)
    backend = state / 'backend'
    backend.mkdir(parents=True, exist_ok=True)
    token = state / 'auth.token'
    try:
        with token.open('x') as stream:
            stream.write(secrets.token_hex(32))
    except FileExistsError:
        if len(token.read_text().strip()) < 32:
            raise ValueError('Existing pairing credential is invalid; retained without modification')
    for path in (state, backend, token):
        os.chown(path, *owner)
    os.chmod(token, 0o600)


if __name__ == '__main__':
    prepare(Path(os.environ.get('ANYPS5_WEB_STATE_ROOT', '/state')))
    print('Web app state ready', flush=True)
