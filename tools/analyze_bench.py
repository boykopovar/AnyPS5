import sys
import csv
import re
import math

def parse_fps(title):
    if not title:
        return 0.0
    match = re.search(r"FPS:\s*([\d\.]+)", title)
    if match:
        return float(match.group(1))
    return 0.0

def percentile(data, p):
    if not data:
        return 0.0
    s = sorted(data)
    k = (len(s) - 1) * p
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return s[int(k)]
    d0 = s[int(f)] * (c - k)
    d1 = s[int(c)] * (k - f)
    return d0 + d1

def compute_stats(values):
    if not values:
        return {"min": 0.0, "p10": 0.0, "median": 0.0, "mean": 0.0, "p90": 0.0, "max": 0.0, "count": 0}
    return {
        "min": min(values),
        "p10": percentile(values, 0.10),
        "median": percentile(values, 0.50),
        "mean": sum(values) / len(values),
        "p90": percentile(values, 0.90),
        "max": max(values),
        "count": len(values)
    }

def main():
    if len(sys.argv) < 2:
        print("Usage: python analyze_bench.py <csv_path> [--window start end]")
        sys.exit(1)
    csv_path = sys.argv[1]
    w_start = 40
    w_end = 80
    if "--window" in sys.argv:
        idx = sys.argv.index("--window")
        if idx + 2 < len(sys.argv):
            w_start = int(sys.argv[idx + 1])
            w_end = int(sys.argv[idx + 2])
    rows = []
    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            sec = int(row["Second"])
            fps = parse_fps(row.get("Title", ""))
            rows.append((sec, fps, row.get("Title", "")))
    full_fps = [r[1] for r in rows]
    steady_fps = [r[1] for r in rows if w_start <= r[0] <= w_end]
    full_stats = compute_stats(full_fps)
    steady_stats = compute_stats(steady_fps)
    print(f"=== Benchmark Analysis: {csv_path} ===")
    print(f"Total samples: {len(rows)}")
    print(f"Full run (all seconds):")
    print(f"  Min:    {full_stats['min']:.2f}")
    print(f"  P10:    {full_stats['p10']:.2f}")
    print(f"  Median: {full_stats['median']:.2f}")
    print(f"  Mean:   {full_stats['mean']:.2f}")
    print(f"  P90:    {full_stats['p90']:.2f}")
    print(f"  Max:    {full_stats['max']:.2f}")
    print(f"Steady-State Window (s{w_start} - s{w_end}):")
    print(f"  Samples:{steady_stats['count']}")
    print(f"  Min:    {steady_stats['min']:.2f}")
    print(f"  P10:    {steady_stats['p10']:.2f}")
    print(f"  Median: {steady_stats['median']:.2f}")
    print(f"  Mean:   {steady_stats['mean']:.2f}")
    print(f"  P90:    {steady_stats['p90']:.2f}")
    print(f"  Max:    {steady_stats['max']:.2f}")

if __name__ == "__main__":
    main()
