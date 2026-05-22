# THIS FILE CONTAINS A BUNCH OF FUNCTIONS I FOUND USEFUL FOR AUTOMATION
# OR CLEANING UP

from pathlib import Path
import os
import shutil
import sys
import subprocess
import settings
from run import std_to_bin_trace

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
        elif "notifyAll" in op or "broadcast" in op:
            broadcast_count += 1
            notif_vars.add(var)
        elif "notify" in op:
            notify_count += 1
            notif_vars.add(var)

        ev_count += 1
    
    if wait_count or notify_count or broadcast_count:
        print(f"{trace_file_path.stem}:")
        print(f"    {wait_count} waits, {notify_count} notifies, {broadcast_count} broadcasts, {ev_count} events")
        # print(f"    wait_vars: {wait_vars}, notif_vars: {notif_vars}")

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
    for trace_file_path in Path(settings.trace_std_dir).iterdir():
        if not trace_file_path.is_file():
            continue
        print_summary(trace_file_path)

def cleanup_old_predictors():
    keep = ["SUnhang", "SUnhang_cond_var", "SUnhang_no_1_lev_locks-no_dead_th_fp-no_1_th_rw-reen_locks"]

    for bench_dir in Path(settings.out_files_base).iterdir():
        if bench_dir.is_file():
            continue
        print(bench_dir.stem)
        for predictor_dir in bench_dir.iterdir():
            predictor = predictor_dir.stem
            if predictor not in keep:
                print(f"    Removing {predictor}...")
                shutil.rmtree(predictor_dir)

def format_meta_file(meta_file_path: Path):
    in_file = open(meta_file_path, 'r')

    output = ""

    # Skip the errors and warnings until you reach the number of threads
    while True:
        line = in_file.readline()

        if line.find("numThreads") != -1:
            th_count = int(line.strip().split(": ")[1])
            # All other suites wrongly say that the number of threads is doubled(explained more src/main.cpp)
            # The +5 is just in case another thing arises

            if settings.bench_suite != 'original':
                th_count = th_count // 2 + 3

            output += str(th_count) + " "
            break
    
    # Add the number of variables and locks
    output += in_file.readline().strip().split(": ")[1] + ' '
    output += in_file.readline().strip().split(": ")[1]
    in_file.close()

    # Output to the same file
    out_file = open(meta_file_path, 'w')
    out_file.write(output)

    return out_file


def create_spd_trace():
    out_dir = settings.trace_dir / "tmp"
    os.makedirs(out_dir, exist_ok=True)

    for trace_path in settings.trace_std_dir.iterdir():
        bench_name = trace_path.stem
        out_path = out_dir / f"{bench_name}.std"

        out_file = open(out_path, 'w')
        for line in open(trace_path, 'r'):
            split_line = line.split("|")
            l, r = split_line[1].find("("), split_line[1].find(")")
            ev, target = split_line[1][:l], split_line[1][l+1:r]

            if "acq" == ev:
                out_file.write("|".join([split_line[0], f"req({target})", split_line[2]]))
            out_file.write(line)
        
        out_file.close()

# Put the ids of threads that called notify at least once
def add_notif_meta(meta_file, trace_file_path):
    notif_threads = set()
    tid_map = {}
    tid_cnt = 0

    trace_file = open(trace_file_path, 'r')
    for line in trace_file:
        line = line.strip()
        if not line:
            continue
        
        data = line.split("|")
        tid, ev = data[0], data[1][:data[1].find("(")]
        
        if tid not in tid_map:
            tid_map[tid] = tid_cnt
            tid_cnt += 1

        if ev == "notify" or ev == "broadcast":
            notif_threads.add(str(tid_map[tid]))

    meta_file.write(f"\n{len(notif_threads)}\n{" ".join(notif_threads)}")

def trace_meta_from_log(meta_path: Path, bench_name: str):
    log_file_path = settings.out_files_base / bench_name / settings.spdoffline_dir / "log.txt"
    if not log_file_path.exists():
        return None
    
    log_file = open(log_file_path, "r")
    meta_file = open(meta_path, "w")

    # Skip header
    log_file.readline()
    for i in range(4):
        line = log_file.readline()
        meta_file.write(f"{int(line.split(": ")[1].strip()) + 1} ")
    
    return meta_file

def create_trace_meta():
    os.makedirs(settings.trace_meta_dir, exist_ok=True)

    for trace_path in settings.trace_std_dir.iterdir():
        bench_name = trace_path.stem
        print(bench_name)

        meta_file_path = settings.trace_meta_dir / (bench_name + ".meta")

        print(f"TRACE PATH: {trace_path}")
        print(f"META PATH: {meta_file_path}")

        # First try from log
        print("Trying from log file...")
        meta_file = trace_meta_from_log(meta_file_path, bench_name)

        # If we don't have one use the converter
        if meta_file is None:
            print("Failed. Getting it from trace conversion")
            # Redirect the output of the converter to the meta file
            std_to_bin_trace(trace_path, bench_name, meta_file_path)

            # Reformat the redirected output and add the missing pieces
            meta_file = format_meta_file(meta_file_path)

        if settings.bench_suite != "original":
            print("Adding notify metadata...")
            add_notif_meta(meta_file, trace_path)
        
def split_runtime():
    dic = {"parse" : 0, "enum" : 0, "abs check" : 0, "sync check" : 0}
    keys = list(dic.keys())

    for bench_path in settings.out_files_base.iterdir():
        log_file_path = bench_path / settings.sunhang_name / "log.txt"
        log_file = open(log_file_path)

        for i, line in enumerate(log_file.readlines()[-6:-2]):
            dic[keys[i]] += int(line.split(" = ")[1].split()[0]) / 1000
    
    for k, v in dic.items():
        print(f"{k} = {v:.2f} sec")

def main():
    opt = sys.argv[1]
    match opt:
        case "cv_summ":
            print_summary_for_wait_notify_benchmarks()
        case "clean_old_pred":
            cleanup_old_predictors()
        case "print_th_op":
            tid = sys.argv[2]
            in_trace_path = Path(settings.trace_std_dir) / f"{sys.argv[3]}.std"
            out_trace_path = Path(settings.trace_std_dir) / f"{sys.argv[3]}_{tid}.std"
            print_thread_op(tid, in_trace_path, out_trace_path)
        case "print_last_op":
            print_th_last_op(Path(settings.trace_std_dir) / f"{sys.argv[2]}.std")
        case "create_trace_meta":
            create_trace_meta()
        case "split_runtime":
            split_runtime()
        case "create_spd_trace":
            create_spd_trace()
        case _:
            print("Invalid option")


main()