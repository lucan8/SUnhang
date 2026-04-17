# THIS FILE CONTAINS A BUNCH OF FUNCTIONS I FOUND USEFUL FOR AUTOMATION
# OR CLEANING UP

from pathlib import Path
import os
import shutil
import sys

bench_in_path = "traces/vendor/jacontebe"
bench_out_path = "benchmarks/generated/output_SUnhang"

def print_summary(trace_file_path: Path):
    ev_count = 0
    wait_count = 0
    notify_count = 0
    broadcast_count = 0

    wait_vars = set()
    notif_vars = set()

    for line in open(trace_file_path, "r"):
        op = line.split("|")[1]
        var = op[op.find("(") + 1:-1]
        if "wait" in op:
            wait_count += 1
            wait_vars.add(var)
        elif "notifyAll" in op:
            broadcast_count += 1
            notif_vars.add(var)
        elif "notify" in op:
            notify_count += 1
            notif_vars.add(var)

        ev_count += 1
    
    if wait_count or notify_count or broadcast_count:
        print(f"{trace_file_path.stem}:")
        print(f"    {wait_count} waits, {notify_count} notifies, {broadcast_count} broadcasts, {ev_count} events")
        print(f"    wait_vars: {wait_vars}, notif_vars: {notif_vars}")

def print_th_last_op(trace_file_path: Path):
    dic = {}
    for line in open(trace_file_path, "r"):
        tid = line.split("|")[0]
        dic[tid] = line
    
    for tid, line in dic.items():
        print(f"{tid}: {line}")

def print_thread_op(tid: str, trace_file_path: Path, trace_out_path: Path):
    out_file = open(trace_out_path, 'w')
    last_line = ""
    for line in open(trace_file_path, "r"):
        if line.split("|")[0] == tid:
            out_file.write(f"{line}")
            last_line = line

    print(f"Last line: {last_line}")
    print(f"Saved to {trace_out_path}")

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
        case "print_tr_op":
            tid = sys.argv[2]
            in_trace_path = Path(bench_in_path) / f"{sys.argv[3]}.std"
            out_trace_path = Path(bench_in_path) / f"{sys.argv[3]}_{tid}.std"
            print_thread_op(tid, in_trace_path, out_trace_path)
        case "print_last_op":
            print_th_last_op(Path(bench_in_path) / f"{sys.argv[2]}.std")
        case _:
            print("Invalid option")

main()