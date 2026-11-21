import argparse
import json
from pathlib import Path
import re
import struct
import subprocess


def currency_map(path):
    lines = Path(path).read_text(encoding='utf-8').splitlines()
    start = next(i for i, line in enumerate(lines) if line.strip() == 'CurrencyMap{')
    regions = {}
    region = None
    entry = None
    field = None
    values = []
    for raw in lines[start + 1:]:
        line = raw.strip()
        if not line or line.startswith('//'):
            continue
        if field is not None:
            if line == '}':
                if len(values) != 2:
                    raise ValueError('currency date must have two signed words')
                entry[field] = struct.unpack('>q', struct.pack('>ii', *values))[0]
                field = None
            else:
                values.append(int(line.removesuffix(',')))
        elif line == '}':
            if entry is not None:
                if not re.fullmatch('[A-Z]{3}', entry.get('id', '')):
                    raise ValueError('currency entry has no ISO code')
                regions[region].append(entry)
                entry = None
            elif region is not None:
                region = None
            else:
                return regions
        elif region is None:
            match = re.fullmatch(r'([A-Z]{2}|[0-9]{3})\{', line)
            if not match:
                raise ValueError('unknown region declaration: ' + line)
            region = match.group(1)
            if region in regions:
                raise ValueError('duplicate region')
            regions[region] = []
        elif line == '{':
            if entry is not None:
                raise ValueError('nested currency entry')
            entry = {}
        elif line in ('from:intvector{', 'to:intvector{'):
            field = line.split(':')[0]
            values = []
        else:
            match = re.fullmatch(r'(id|tender)\{("[^"]*")\}', line)
            if not match or entry is None:
                raise ValueError('unknown currency field: ' + line)
            entry[match.group(1)] = json.loads(match.group(2))
    raise ValueError('unterminated currency map')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True)
    parser.add_argument('--shard', required=True, type=int)
    parser.add_argument('--shards', required=True, type=int)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if not 0 <= args.shard < args.shards or not args.command:
        raise ValueError('invalid shard or missing probe command')
    regions = currency_map(args.source)
    rows = []
    expected = []
    safe_rows = 0
    extension_rows = 0
    for ordinal, (region, entries) in enumerate(sorted(regions.items())):
        if ordinal % args.shards != args.shard:
            continue
        dates = {0, 1700000000000}
        for entry in entries:
            for field in ('from', 'to'):
                if field in entry:
                    dates.update((entry[field] - 1, entry[field], entry[field] + 1))
        safe = all('from' in entry and (len(entry) <= 2 or 'to' in entry) for entry in entries)
        for date in sorted(dates):
            if abs(date) >= 2 ** 53:
                raise ValueError('date fixture exceeds exact UDate integers')
            codes = [entry['id'] for entry in entries if entry.get('from', float('-inf')) <= date < entry.get('to', float('inf'))]
            rows.append(f'{region} {date} {int(safe)}\n')
            expected.append(f'HISTORY {region} {date} {len(codes)} ' + ','.join(codes))
            safe_rows += int(safe)
            extension_rows += int(not safe)
    run = subprocess.run(args.command, input=''.join(rows), text=True, capture_output=True, timeout=18)
    if run.returncode:
        raise RuntimeError(f'currency probe exit {run.returncode}: {run.stderr[-6000:]}')
    actual = []
    for line in run.stdout.splitlines():
        if line.startswith('HISTORY '):
            actual.append(line)
        elif line.startswith('[src/HostCpu.cpp:') and ' host CPU ' in line:
            print(line)
        else:
            raise AssertionError('unexpected currency probe output: ' + line)
    if actual != expected:
        for index, (got, want) in enumerate(zip(actual, expected)):
            if got != want:
                raise AssertionError(f'currency history row {index}: {got!r} != {want!r}')
        raise AssertionError(f'currency history output length {len(actual)} != {len(expected)}')
    print(f'currency history shard {args.shard}: {len(rows)} data fixtures; {safe_rows} direct native comparisons; {extension_rows} optional-date fixtures')


if __name__ == '__main__':
    main()
