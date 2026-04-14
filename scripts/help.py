# THIS FILE CONTAINS A BUNCH OF FUNCTIONS I FOUND USEFUL FOR AUTOMATION
# OR CLEANING UP

from pathlib import Path
import os
import shutil
import sys

bench_in_path = "benchmarks/extra/data"
bench_out_path = "benchmarks/generated/output_SUnhang"

def print_summary(trace_file_path: Path):
    ev_count = 0
    wait_count = 0
    notify_count = 0
    broadcast_count = 0
    for line in open(trace_file_path, "r"):
        if "wait" in line:
            wait_count += 1
        elif "notify" in line:
            notify_count += 1
        elif "broadcast" in line:
            broadcast_count += 1

        ev_count += 1
    
    if wait_count or notify_count or broadcast_count:
        print(f"{trace_file_path.stem}: {wait_count} waits, {notify_count} notifies, {broadcast_count} broadcasts, {ev_count} events")

def print_summary_for_wait_notify_benchmarks():
    global bench_in_path
    for trace_file_path in Path(bench_in_path).iterdir():
        if not trace_file_path.is_file():
            continue
        print_summary(trace_file_path)

def cleanup_old_predictors():
    global bench_out_path
    keep = ["SUnhang", "SUnhang_cond_var", "SUnhang_no_1_lev_locks-no_dead_th_fp-no_1_th_rw-reen_locks"]

    for bench_dir in Path(bench_out_path).iterdir():
        if bench_dir.is_file():
            continue
        print(bench_dir.stem)
        for predictor_dir in bench_dir.iterdir():
            predictor = predictor_dir.stem
            if predictor not in keep:
                print(f"    Removing {predictor}...")
                shutil.rmtree(predictor_dir)

def main():
    opt = sys.argv[1]
    match opt:
        case "cv_summ":
            print_summary_for_wait_notify_benchmarks()
        case "clean_old_pred":
            cleanup_old_predictors()
        case _:
            print("Invalid option")

main()