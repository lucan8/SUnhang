import os
import zipfile
import optparse
import subprocess
from pathlib import Path
import time
import psutil
from enum import Enum
from settings import settings
from conv_trace import conv_trace
from util import get_benchmarks, run_cmd
from compile_benchmarks import from_log_file, create_table_from_rows
import compile_benchmarks
import re

# TODO: Add handler for ctrl-c and clean-up the file created by the java gc
# TODO: Cleanup the memory tracking functions

class PLang(Enum):
    CPP = 1
    JAVA = 2


def extract_peak_java_memory() -> float:
    def to_mb(value_str: str, unit: str) -> float:
        val = float(value_str)
        if unit == 'K': return val / 1024.0
        if unit == 'M': return val
        if unit == 'G': return val * 1024.0
        return val
    
    log_path="gc.log"
    if not os.path.exists(log_path):
        print(f"[ERROR]: Java GC log file {log_path} not found.")
        return 0.0

    peak_mb = 0.0

    with open(log_path, "r", encoding="utf-8") as f:
        for line in f:
            # Check runtime spikes
            gc_match = extract_peak_java_memory.gc_pattern.search(line)
            if gc_match:
                val, unit = gc_match.group(1), gc_match.group(2)
                current_mb = to_mb(val, unit)
                if current_mb > peak_mb:
                    peak_mb = current_mb
                continue

            # Check termination footprint (important for small runs that never trigger a GC)
            exit_match = extract_peak_java_memory.exit_pattern.search(line)
            if exit_match:
                val, unit = exit_match.group(1), exit_match.group(2)
                current_mb = to_mb(val, unit)
                if current_mb > peak_mb:
                    peak_mb = current_mb

    os.remove(log_path)
    return peak_mb

# Static function variables. Compiles the regexes only ONCE

# Matches runtime GC transitions (e.g., " 4096M->1024M" or " 2G->1G")
# Capture the value BEFORE the arrow, which represents the allocation peak
extract_peak_java_memory.gc_pattern = re.compile(r"\s(\d+)([KMG])->\d+([KMG])")

# Matches the final exit summary line (e.g., "used 14769K")
extract_peak_java_memory.exit_pattern = re.compile(r"used\s(\d+)([KMG])")

def get_paths(bench_name: str, predictor: str):
    out_path = (settings.out_files_base / bench_name / predictor / "log.txt")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    
    input_path = conv_trace(bench_name, predictor)

    return input_path, out_path

def run_predictor(bench_name: str, predictor: str):
    input_path, out_path = get_paths(bench_name, predictor)

    # print("Input path: ", input_path)
    # print("Output path: ", out_path)

    stdout = None
    if predictor == settings.sunhang_name:
        lang = PLang.CPP
        cmd = [settings.sunhang_exe_path, input_path, out_path]
    else:
        lang = PLang.JAVA
        jvm_max_mem_flag = "-Xmx8g"

        # These need more memory. I would make -Xmx10g the default
        # But it prints 0 mem usage for the small benchmarks(WHY???)
        if bench_name in ["graphchi", "biojava"]:
            jvm_max_mem_flag = "-Xmx10g"

        cmd = ["java", jvm_max_mem_flag, "-Xlog:gc=info,gc+heap+exit=info:file=gc.log", "-jar", settings.spdoffline_jar_path, f"-p={input_path}"]
        stdout = out_path
    
    run_cmd(cmd, stdout)

    # Parse the GC log file to determine the peak mem usage for java and print it to log file
    # The cpp version does this automatically
    if lang == PLang.JAVA:
        peak_mem_usage = extract_peak_java_memory()
        open(out_path, 'a').write(f"Peak memory usage = {peak_mem_usage} MB")

def benchmark(all_benchmarks: set[str], user_benchmarks: list[str], predictor: str, run_it_count:int, first:bool):
    rows = {}

    print(f"Predictor: {predictor}")
    for bench in user_benchmarks:
        if bench not in all_benchmarks:
            print(f"    Skipping invalid benchmark: {bench}")
            continue

        print(f"    Benchmark: {bench}")
        avg_mem = 0
        avg_time = 0

        # Used to hold the row representing the current benchmark
        comm_col_res, pred_col_res = [], []

        # Run for RUN_IT_COUNT and compute average time and memory
        for i in range(run_it_count):
            # Run predictor
            print(f"        Run: {i} / {run_it_count - 1}", end=": ")
            run_predictor(bench, predictor)
            
            # Load log file
            comm_col, pred_col = from_log_file(settings.out_files_base / bench / predictor / "log.txt")
            comm_col_res, pred_col_res = comm_col, pred_col

            # Update sum
            avg_mem += pred_col[-1]
            avg_time += pred_col[-2]

            print(f"            {pred_col[-2]:.2f} sec, {pred_col[-1]:.2f} MB")
        
        pred_col_res[-1] = avg_mem / run_it_count
        pred_col_res[-2] = avg_time / run_it_count

        print(f"        Avg: {pred_col_res[-2]:.2f} sec, {pred_col_res[-1]:.2f} MB")
       
        # If this is the first predictor, also use it's common columns, otherwise just it's specific ones
        if first:
            rows[bench] = comm_col_res + pred_col_res
        else:
            rows[bench] = pred_col_res
    
    print(f"[TABLE INFO]: Compiled {len(rows)} rows for {predictor}")

    return rows


def main():
    # Parser options
    parser = optparse.OptionParser()
    parser.add_option("-b", "--benchmarks", dest="benchmarks", default="all",
                      help="run the script on a selected group of benchmarks. " \
                            "Specify the names of the benchmarks and seperate them with a comma " \
                            "(e.g., Bensalem,Account) (Default: all).")
    
    parser.add_option("--bs", "--bench_suite", dest="bench_suite", default="cond_var",
                      help="run the script on the given benchmark suite. " \
                            f"Should be one of {settings.available_bench_suites}" \
                            "(Default: cond_var).")
    
    parser.add_option("-i", "--ignore", dest="ignored_bench", default="",
                      help="ignores the specified benchmarks. Default None")
    
    parser.add_option("-p", "--predictor", dest="predictors", default="all",
                      help="runs only the desired predictors." \
                           "Specify the names of the predictors and seperate them with a comma " \
                           "For correct table construction, always specify SUnhang, and put it first" \
                           "(e.g SUnhang,SPDOffline) (Default: all)")
    
    parser.add_option("--it", "--it_count", dest="run_it_count", default="10",
                      help="Specifies the number of iterations for benchmarking. Default 10")

    parser.add_option("--vt", "--verbose_table", dest="verbose_table", default="N",
                      help="Specifies whether the table should be verbose(Y) or not(N). Default is N")
    
    # Parse and check arguments

    (options, args) = parser.parse_args()
    
    # Set benchmark suite
    settings.set_bench_suite(options.bench_suite)
    if settings.bench_suite == "":
        print(f"[ERROR]: Invalid benchmark suite ({options.bench_suite}), should be one of {settings.available_bench_suites}")
        return
    
    print(f"Benchmark suite: {settings.bench_suite}")
    
    # Set bennchmarks
    all_benchmarks = set(get_benchmarks())
    if options.benchmarks != "all":
        user_benchmarks = options.benchmarks.split(",")
    else:
        user_benchmarks = list(all_benchmarks)

    # Ignore undesired benchmarks
    ignored_bench = options.ignored_bench.split(",")
    print(f"Benchmarks to ignore: {ignored_bench}")

    # Set predictors
    all_predictors = set(settings.predictors)
    if options.predictors == "all":
        user_predictors = settings.predictors
    else:
        user_predictors = options.predictors.split(",")
    
    # Set the number of iterations
    run_it_count = int(options.run_it_count)
    print(f"Iteration count: {run_it_count}")
    
    # Set the verbosity level for table construction
    verbose_table = False
    if options.verbose_table == "Y":
        verbose_table = True
    elif options.verbose_table != "N":
        print("[ERROR]: Invalid option for verbose_table. Should be Y or N")
        return
    
    print(F"Table Verbosity: {verbose_table}")
    compile_benchmarks.set_verbosity(verbose_table)

    # Benchmarks predictors
    rows = []
    valid_pred = []
    for pred in user_predictors:
        if pred not in all_predictors:
            print(f"Skipping invalid predictor: {pred}")
            continue
        
        valid_pred.append(pred)
        first = len(valid_pred) == 1
        rows.append(benchmark(all_benchmarks, user_benchmarks, pred, run_it_count, first))
    
    # Build table
    create_table_from_rows(rows, valid_pred)

if __name__ == "__main__":
    main()