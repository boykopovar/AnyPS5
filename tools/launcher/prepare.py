import sys
sys.dont_write_bytecode = True
import argparse
import json
import os
import shutil
import struct
import subprocess
import tempfile
import time
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
if (Path(__file__).resolve().parent.parent / 'engine/bundle.py').is_file():
    sys.path.extend([str(Path(__file__).resolve().parent.parent / 'engine'), str(Path(__file__).resolve().parent.parent)])
from bundle import SYSTEM_DLLS, sha256, verify_bundle
from pe import inspect_pe
from unwrap_self import SELF_MAGIC, SELF_MAGIC_KERNEL


class Cancelled(Exception):
    pass


def atomic_json(path, value, timeout=5):
    payload = json.dumps(value, ensure_ascii=True)
    pending = path.with_name(path.name + '.' + uuid.uuid4().hex + '.pending')
    deadline = time.monotonic() + timeout
    written = False
    try:
        while True:
            try:
                if not written:
                    pending.write_text(payload, encoding='utf-8')
                    written = True
                os.replace(pending, path)
                return
            except PermissionError as error:
                if getattr(error, 'winerror', None) not in (5, 32, 33) or time.monotonic() >= deadline:
                    raise
                time.sleep(0.025)
    finally:
        try:
            pending.unlink(missing_ok=True)
        except OSError:
            pass


def linked(path):
    return path.is_symlink() or path.is_junction()


def elf(path):
    with path.open('rb') as stream:
        header = stream.read(64)
    if len(header) < 64 or header[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', header, 18)[0] != 62:
        raise ValueError('Expected an ELF64 little-endian x86-64 image: ' + str(path))


def resources(source):
    for directory in (source, source / 'app0'):
        if not directory.is_dir():
            continue
        for name in ('eboot.bin', 'eboot.elf', 'app.elf'):
            main = directory / name
            if main.is_file():
                return directory, main
    children = []
    for child in source.iterdir():
        if child.is_dir() and not linked(child):
            for directory in (child, child / 'app0'):
                for name in ('eboot.bin', 'eboot.elf', 'app.elf'):
                    if (directory / name).is_file():
                        children.append((directory, directory / name))
                        break
    if len(children) == 1:
        return children[0]
    if children:
        raise ValueError('Multiple game folders found. Scan and select an individual game folder.')
    for directory in (source, source / 'app0'):
        if not directory.is_dir():
            continue
        images = [p for p in directory.iterdir() if p.is_file() and p.suffix.lower() in ('.elf', '.self')]
        if len(images) == 1:
            return directory, images[0]
        if len(images) > 1:
            raise ValueError('Multiple executable inputs found. An unambiguous main executable or eboot.bin is required.')
    raise ValueError('No extracted eboot.bin or unambiguous ELF/SELF input found. Package extraction and decryption are not supported.')


def inventory(root):
    result = []
    for directory, children, files in os.walk(root, followlinks=False):
        for name in children + files:
            path = Path(directory) / name
            if linked(path):
                raise ValueError('Linked files and directory junctions are not supported: ' + str(path))
        for name in files:
            path = Path(directory) / name
            if not path.is_file():
                raise ValueError('Unsupported resource type: ' + str(path))
            result.append((path, path.stat()))
            if len(result) > 250000:
                raise ValueError('Preparation supports at most 250,000 resource files per game')
    return result


def discover_modules(root, files, main, needed=()):
    from import_audit import module_stem
    required = {module_stem(Path(name).name, True) for name in needed}
    standard = {root / name for name in ('sce_module', 'sce_modules', 'prx') if (root / name).is_dir()}
    directories = set(standard)
    modules = []
    for path, _ in files:
        if path == main:
            continue
        in_standard = any(path.is_relative_to(directory) for directory in standard)
        candidate = in_standard or (module_stem(path.name, True) is not None and module_stem(path.name, True) in required)
        if not candidate:
            continue
        with path.open('rb') as stream:
            magic = stream.read(4)
        image = magic == b'\x7fELF' or len(magic) == 4 and int.from_bytes(magic, 'little') in (SELF_MAGIC, SELF_MAGIC_KERNEL)
        if not image:
            continue
        modules.append(path)
        directories.add(path.parent)
    return sorted(modules), sorted(directories)


def discover_executables(root, files, main, module_directories):
    result = []
    for path, _ in files:
        if path == main or path.suffix.lower() not in ('.elf', '.self') or any(path.is_relative_to(directory) for directory in module_directories):
            continue
        with path.open('rb') as stream:
            header = stream.read(64)
        if len(header) >= 64 and header[:6] == b'\x7fELF\x02\x01' and struct.unpack_from('<H', header, 18)[0] == 62:
            result.append(path)
        elif len(header) >= 4 and int.from_bytes(header[:4], 'little') in (SELF_MAGIC, SELF_MAGIC_KERNEL):
            result.append(path)
    return sorted(result)


def pe_tables(path):
    data = path.read_bytes()
    header = struct.unpack_from('<I', data, 60)[0]
    count = struct.unpack_from('<H', data, header + 6)[0]
    optional = header + 24
    table = optional + struct.unpack_from('<H', data, header + 20)[0]
    sections = [struct.unpack_from('<IIII', data, table + i * 40 + 8) for i in range(count)]

    def offset(rva):
        for size, address, raw_size, raw in sections:
            if address <= rva < address + min(size, raw_size):
                value = raw + rva - address
                if value < len(data):
                    return value
        raise ValueError('Invalid PE symbol address in ' + str(path))

    def string(rva):
        start = offset(rva)
        end = data.find(b'\0', start, start + 4096)
        if end < 0:
            raise ValueError('Invalid PE symbol name in ' + str(path))
        return data[start:end].decode('ascii')

    exports, ordinals, imports = set(), set(), {}
    export_rva = struct.unpack_from('<I', data, optional + 112)[0]
    if export_rva:
        export = offset(export_rva)
        base, functions, names, addresses, strings, indices = struct.unpack_from('<IIIIII', data, export + 16)
        if max(functions, names) > 1000000:
            raise ValueError('PE export table exceeds preparation limits')
        for i in range(functions):
            if struct.unpack_from('<I', data, offset(addresses) + i * 4)[0]:
                ordinals.add(base + i)
        for i in range(names):
            exports.add(string(struct.unpack_from('<I', data, offset(strings) + i * 4)[0]))
    import_rva = struct.unpack_from('<I', data, optional + 120)[0]
    if import_rva:
        entry = offset(import_rva)
        for i in range(4096):
            original, stamp, chain, name, first = struct.unpack_from('<IIIII', data, entry + i * 20)
            if not any((original, stamp, chain, name, first)):
                break
            values = []
            thunk = offset(original or first)
            for j in range(1000000):
                value = struct.unpack_from('<Q', data, thunk + j * 8)[0]
                if not value:
                    break
                values.append(value & 0xffff if value >> 63 else string(value + 2))
            else:
                raise ValueError('PE import table exceeds preparation limits')
            imports[string(name).lower()] = values
        else:
            raise ValueError('PE dependency table exceeds preparation limits')
    return imports, exports, ordinals


def validate_output(stage):
    binaries = [stage / 'app.exe', *sorted(p for p in stage.glob('*.exe') if p.name.lower() != 'app.exe'), *sorted((stage / 'libs').glob('*.prx')), *sorted((stage / 'libs').glob('*.dll')),
                *sorted((stage / 'app0').rglob('*.guest.prx'))]
    available = {}
    for path in binaries:
        inspect_pe(path)
        name = path.name.lower()
        if name in available:
            raise ValueError('Ambiguous Windows dependency name: ' + path.name)
        available[name] = pe_tables(path)
    missing, visited = [], set()
    pending = [*(p.name.lower() for p in stage.glob('*.exe')), *(p.name.lower() for p in (stage / 'app0').rglob('*.guest.prx'))]
    while pending:
        current = pending.pop()
        if current in visited:
            continue
        visited.add(current)
        imports = available[current][0]
        for name, symbols in imports.items():
            if name.startswith(('api-ms-win-', 'ext-ms-win-')) or name.endswith('.dll') and name[:-4] in SYSTEM_DLLS:
                continue
            if name not in available:
                missing.append(current + ': missing library ' + name)
                continue
            pending.append(name)
            _, exports, ordinals = available[name]
            for symbol in symbols:
                if symbol not in (ordinals if isinstance(symbol, int) else exports):
                    missing.append(current + ': missing export ' + str(symbol) + ' in ' + name)
    if missing:
        raise ValueError('Windows dependencies are incomplete:\n' + '\n'.join(missing[:30]) + ('\nAdditional missing imports were omitted.' if len(missing) > 30 else ''))


class Preparation:
    def __init__(self, engine, source, output, job):
        self.engine, self.source, self.output, self.job = map(Path, (engine, source, output, job))
        self.engine = self.engine.resolve()
        self.job = self.job.resolve()
        self.job.mkdir(parents=True, exist_ok=True)
        self.value = {'status': 'running', 'phase': 'Checking input', 'message': '', 'completed_bytes': 0, 'total_bytes': 0,
                      'output': str(self.output), 'log': str(self.job / 'preparation.log')}
        self.last_update = 0

    def check(self):
        if (self.job / 'cancel').exists():
            raise Cancelled('Preparation cancelled. Original files were not changed.')

    def report(self, phase=None, message=None, force=False):
        self.check()
        if phase is not None:
            self.value['phase'] = phase
        if message is not None:
            self.value['message'] = message
        now = time.monotonic()
        if force or now - self.last_update > 0.4:
            atomic_json(self.job / 'status.json', self.value)
            self.last_update = now

    def copy(self, source, destination, expected=None):
        self.check()
        before = source.stat()
        destination.parent.mkdir(parents=True, exist_ok=True)
        with source.open('rb') as incoming, destination.open('xb') as outgoing:
            for block in iter(lambda: incoming.read(1048576), b''):
                self.check()
                outgoing.write(block)
                self.value['completed_bytes'] += len(block)
                self.report(message=str(source))
        after = source.stat()
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns) or expected and (before.st_size, before.st_mtime_ns) != (expected.st_size, expected.st_mtime_ns):
            raise ValueError('Source changed during preparation: ' + str(source))

    def command(self, arguments, directory, description):
        environment = {key: value for key, value in os.environ.items() if not key.startswith(('ANYPS5_', 'APS5_', 'PYTHON'))}
        environment['PATH'] = str(Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32')
        with (self.job / 'preparation.log').open('ab') as log:
            log.write((json.dumps(arguments) + '\n').encode())
            log.flush()
            process = subprocess.Popen(arguments, cwd=directory, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                       stdin=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            try:
                while process.poll() is None:
                    self.report()
                    time.sleep(0.2)
                if process.returncode:
                    log.flush()
                    with (self.job / 'preparation.log').open('rb') as incoming:
                        incoming.seek(max(0, incoming.seek(0, 2) - 4000))
                        detail = incoming.read().decode('utf-8', errors='replace')
                    if 'Forbidden syscall instruction' in detail:
                        self.value['failure_kind'] = 'unsupported_console_syscall'
                        raise ValueError('This executable contains a console syscall that the Windows converter cannot support yet. No game was added. Engine support is required; changing the folder layout will not resolve it.\n' + detail)
                    raise ValueError(description + ' failed (exit ' + str(process.returncode) + ').\n' + detail)
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()

    def image(self, path):
        with path.open('rb') as stream:
            magic = stream.read(4)
        if len(magic) == 4 and int.from_bytes(magic, 'little') in (SELF_MAGIC, SELF_MAGIC_KERNEL):
            temporary = path.with_name('.anyps5-unwrapped-' + uuid.uuid4().hex)
            try:
                unwrapper = Path(__file__).with_name('unwrap_self.py')
                if not unwrapper.is_file():
                    unwrapper = Path(__file__).resolve().parent.parent / 'unwrap_self.py'
                self.command([sys.executable, '-I', '-S', '-B', str(unwrapper), str(path), str(temporary)], self.job, 'SELF unwrapping')
                temporary.replace(path)
            finally:
                temporary.unlink(missing_ok=True)
        elf(path)

    def run(self):
        staging = None
        outcome = 'failed'
        try:
            self.report(force=True)
            if linked(self.source) or not self.source.is_dir():
                raise ValueError('The discovered input folder is missing or linked. Scan again.')
            self.source = self.source.resolve(strict=True)
            self.output = self.output.parent.resolve() / self.output.name
            if self.output.exists() or self.output.is_relative_to(self.source) or self.source.is_relative_to(self.output):
                raise ValueError('Preparation requires a new output folder outside the original input')
            self.report('Verifying engine', 'Checking converter and matching runtime hashes', True)
            manifest = verify_bundle(self.engine)
            resource, main = resources(self.source)
            files = inventory(resource)
            if main.stat().st_size > 2147483648:
                raise ValueError('Executables larger than 2 GiB are not supported by automatic preparation')
            self.output.parent.mkdir(parents=True, exist_ok=True)
            if linked(self.output.parent):
                raise ValueError('The preparation output parent must not be linked')
            staging = Path(tempfile.mkdtemp(prefix='.anyps5-preparing-', dir=self.output.parent))
            app0 = staging / 'app0'
            app0.mkdir()
            self.report('Checking executable', 'Unwrapping decrypted SELF when required', True)
            self.copy(main, app0 / main.relative_to(resource))
            input_hash = sha256(app0 / main.relative_to(resource))
            self.image(app0 / main.relative_to(resource))
            total = sum(info.st_size for _, info in files) + sum(p.stat().st_size for p in (self.engine / 'libs').rglob('*') if p.is_file())
            self.value['total_bytes'] = total
            required = total + main.stat().st_size * 4 + 268435456
            available = shutil.disk_usage(staging).free
            self.value.update(required_bytes=required, available_bytes=available, storage_root=str(self.output.parent))
            if available < required:
                raise ValueError(f'Not enough free space in {self.output.parent}: need {required / 1000000000:.1f} GB, available {available / 1000000000:.1f} GB, short by {(required - available) / 1000000000:.1f} GB. Choose a larger preparation folder in Settings or free space, then retry. No game was added.')
            import import_audit
            staged_main = app0 / main.relative_to(resource)
            needed = set(import_audit.elf_imports(staged_main, staged_main.read_bytes())[1])
            expected = dict(files)
            standard = [resource / name for name in ('sce_module', 'sce_modules', 'prx') if (resource / name).is_dir()]
            executables = discover_executables(resource, files, main, standard)
            executable_hashes = {main: input_hash}
            for path in executables:
                target = app0 / path.relative_to(resource)
                self.copy(path, target, expected[path])
                executable_hashes[path] = sha256(target)
                self.image(target)
                needed.update(import_audit.elf_imports(target, target.read_bytes())[1])
            modules = set()
            source_directories = set()
            self.report('Preparing modules', 'Checking executable dependencies before the full resource copy', True)
            while True:
                discovered, directories = discover_modules(resource, files, main, needed)
                source_directories.update(directories)
                pending = [path for path in discovered if path not in modules]
                if not pending:
                    break
                for path in pending:
                    target = app0 / path.relative_to(resource)
                    self.copy(path, target, expected[path])
                    self.image(target)
                    needed.update(import_audit.elf_imports(target, target.read_bytes())[1])
                    modules.add(path)
            module_directories = [app0 / directory.relative_to(resource) for directory in sorted(source_directories)]
            if executables and not module_directories:
                module_directories.append(app0 / 'sce_module')
            for path in executables:
                if app0 / path.parent.relative_to(resource) in module_directories:
                    raise ValueError('An executable shares a module directory and needs a manual layout: ' + str(path.relative_to(resource)))
            for directory in module_directories:
                directory.mkdir(parents=True, exist_ok=True)
            self.report('Copying runtime', 'Using the selected engine bundle', True)
            for path in (self.engine / 'libs').rglob('*'):
                if path.is_file():
                    self.copy(path, staging / 'libs' / path.relative_to(self.engine / 'libs'))
            arguments = [str(self.engine / 'relinker.exe'), '--windows', '--to-intel', '--registry']
            if module_directories:
                arguments += ['--sce-module-path', str(app0)]
                for directory in module_directories:
                    relative = directory.relative_to(app0).as_posix()
                    if relative not in ('sce_module', 'sce_modules', 'prx'):
                        arguments += ['--module-dir', relative]
            else:
                arguments += ['--skip-sce-module']
            for path in executables:
                arguments += ['--executable', path.relative_to(resource).as_posix()]
            arguments += [str(app0 / main.relative_to(resource)), str(staging / 'app.exe')]
            self.report('Converting for Windows', 'Converting the executable and bundled modules; the game is not executed', True)
            self.command(arguments, staging, 'Windows conversion')
            self.report('Validating Windows output', 'Checking PE architecture and imported libraries/functions', True)
            validate_output(staging)
            audit_source = self.job / 'audit-source'
            audit_source.mkdir(exist_ok=True)
            arguments = [sys.executable, '-I', '-S', '-B', str(Path(__file__).resolve()), '--audit', *map(str, sorted(staging.glob('*.registry.json'))),
                         '--libs', str(staging / 'libs'), '--source', str(audit_source),
                         '--json', str(staging / 'import-audit.json')]
            for path in (main, *executables):
                arguments += ['--exclude-module-file', str(app0 / path.relative_to(resource))]
            for directory in module_directories:
                arguments += ['--modules', str(directory)]
            self.report('Auditing game imports', 'Checking runtime-resolved imports and module dependencies; gameplay remains unverified', True)
            self.command(arguments, staging, 'Game import audit')
            self.report('Copying resources', 'Conversion and import checks passed. Original files remain in place.', True)
            for directory, children, _ in os.walk(resource, followlinks=False):
                self.check()
                for name in children:
                    (app0 / (Path(directory) / name).relative_to(resource)).mkdir(parents=True, exist_ok=True)
            copied = {main, *modules, *executables}
            for path, info in files:
                if path not in copied:
                    self.copy(path, app0 / path.relative_to(resource), info)
            for path, digest in executable_hashes.items():
                if sha256(path) != digest:
                    raise ValueError('An input executable changed during preparation. Scan and retry after its files stop changing.')
            report = {'format': 1, 'source': str(self.source), 'engine_id': manifest['id'], 'input_sha256': input_hash,
                      'gameplay': 'Unverified', 'automatically_executed': False,
                      'executables': {path.relative_to(resource).as_posix(): digest for path, digest in executable_hashes.items()}}
            (staging / 'preparation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
            (staging / 'preparation-source.txt').write_text('AnyPS5 Prepared Source 1\n' + str(self.source) + '\n', encoding='utf-8', newline='\n')
            self.report('Publishing prepared copy', 'Output will be added to the library after publication', True)
            staging.rename(self.output)
            staging = None
            outcome = 'succeeded'
            self.value.update(status=outcome, phase='Converted', message='Windows copy prepared. Gameplay has not been tested.')
        except Cancelled as error:
            outcome = 'cancelled'
            self.value.update(status=outcome, phase='Cancelled', message=str(error))
        except Exception as error:
            self.value.update(status='failed', phase='Preparation failed', message=str(error))
            with (self.job / 'preparation.log').open('ab') as log:
                log.write(('\nPreparation failed: ' + str(error) + '\n').encode('utf-8', errors='replace'))
        finally:
            if staging is not None:
                shutil.rmtree(staging)
            atomic_json(self.job / 'status.json', self.value)
            (self.job / 'done').write_text(outcome, encoding='ascii')
        return 0 if outcome == 'succeeded' else 2


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--audit':
        import import_audit
        raise SystemExit(import_audit.main(sys.argv[2:]))
    parser = argparse.ArgumentParser()
    parser.add_argument('--verify-engine', type=Path)
    parser.add_argument('--engine', type=Path)
    parser.add_argument('--input', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--job', type=Path)
    arguments = parser.parse_args()
    if arguments.verify_engine:
        verify_bundle(arguments.verify_engine)
    else:
        if not all((arguments.engine, arguments.input, arguments.output, arguments.job)):
            parser.error('engine, input, output and job are required')
        raise SystemExit(Preparation(arguments.engine, arguments.input, arguments.output, arguments.job).run())
