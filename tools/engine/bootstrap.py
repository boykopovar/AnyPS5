import json
import zipfile
from pathlib import Path
from sources import download


lock = json.loads(Path('/opt/buildscripts/dependencies.lock').read_text())
for record in lock['downloads']:
    if record['name'] == 'winlibs':
        continue
    archive = download(record, Path('/tmp') / (record['name'] + '.zip'))
    with zipfile.ZipFile(archive) as source:
        source.extractall(Path('/opt') / record['name'])
    archive.unlink()
Path('/opt/python/python313._pth').unlink()
