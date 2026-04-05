from pathlib import Path

bench_in_path = "benchmarks/generated/data"

def print_summary(trace_file_path: Path):
    ev_count = 0
    wait_count = 0
    notify_count = 0

    for line in open(trace_file_path, "r"):
        if "wait" in line:
            wait_count += 1
        elif "notify" in line:
            notify_count += 1

        ev_count += 1
    
    if wait_count or notify_count:
        print(f"{trace_file_path.stem}: {wait_count} waits, {notify_count} notifies, {ev_count} events")


for trace_file_path in Path(bench_in_path).iterdir():
    if not trace_file_path.is_file():
        continue
    print_summary(trace_file_path)