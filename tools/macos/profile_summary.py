import re
import statistics
import sys


def number(pattern, line, default=0.0):
    match = re.search(pattern, line)
    return float(match.group(1)) if match else default


def windows(path):
    rows = []
    current = None
    for line in open(path, errors="replace"):
        if "[present]" in line:
            presents = number(r"(\d+) presents over", line)
            seconds = number(r"presents over (\d+)", line, 10.0)
            current = {"time": line[:8], "fps": presents / seconds, "busy": number(r"GPU busy ([\d.]+)", line), "draws": 0.0, "us": 0.0, "misses": 0.0, "longest": 0.0, "phase": ""}
            rows.append(current)
        elif current is None:
            continue
        elif "[draw] driver phases" in line:
            current["draws"] = number(r"(\d+) draw packets", line)
            current["us"] = number(r"us per packet:.*total ([\d.]+)", line)
        elif "[pipecache]" in line:
            current["misses"] = number(r"(\d+) misses", line)
        elif "[draws]" in line and "longest draw" in line:
            current["longest"] = number(r"longest draw ([\d.]+)", line)
            phases = re.search(r"longest phase of any draw: (.*?);", line)
            if phases:
                pairs = [(name, float(value)) for name, value in re.findall(r"(\w+) ([\d.]+)", phases.group(1))]
                if pairs:
                    current["phase"] = max(pairs, key=lambda pair: pair[1])[0]
    return rows


def main():
    if len(sys.argv) < 2:
        print("usage: python3 -I profile_summary.py <profile log> [--windows]")
        return 2
    rows = windows(sys.argv[1])
    if not rows:
        print("no [present] lines: was the log captured with run-profile.sh?")
        return 1
    if "--windows" in sys.argv:
        print(f"{'time':8} {'fps':>5} {'gpu ms':>6} {'draws':>6} {'draws/f':>7} {'us/draw':>7} {'pmiss':>5} {'longest':>8} phase")
        for row in rows:
            per_frame = row["draws"] / (row["fps"] * 10) if row["fps"] > 0 else 0
            print(f"{row['time']:8} {row['fps']:5.1f} {row['busy']:6.1f} {row['draws']:6.0f} {per_frame:7.0f} {row['us']:7.1f} {row['misses']:5.0f} {row['longest']:8.1f} {row['phase']}")
        print()
    played = [row for row in rows if row["fps"] > 0 and row["draws"] > 0]
    print(f"{len(rows)} windows of 10 s, {len(played)} with draws")
    for name, low, high in (("heavy", 200, 10**9), ("medium", 120, 200), ("light", 0, 120)):
        group = [row for row in played if low <= row["draws"] / (row["fps"] * 10) < high]
        if group:
            print(f"  {name:6} (draws per frame {low}+): {len(group):3} windows, {statistics.mean(r['fps'] for r in group):5.1f} fps, {statistics.mean(r['us'] for r in group):5.0f} us per draw, {statistics.mean(r['busy'] for r in group):5.1f} ms GPU per frame")
    hitches = [row for row in rows if row["longest"] >= 100]
    print(f"  windows with a draw of 100 ms or more: {len(hitches)}; pipeline cache misses: {sum(r['misses'] for r in rows):.0f}")
    for row in sorted(hitches, key=lambda r: -r["longest"])[:5]:
        print(f"    {row['time']} {row['longest']:6.0f} ms in {row['phase']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
