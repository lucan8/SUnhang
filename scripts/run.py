import os
import zipfile
import optparse
import subprocess
from pathlib import Path
import time
import psutil
from enum import Enum
import settings
from conv_trace import conv_trace
from util import get_benchmarks, run_cmd
from compile_benchmarks import from_log_file, create_table_from_rows
import compile_benchmarks

class PLang(Enum):
    CPP = 1
    JAVA = 2

def get_kb_usage_java(process: psutil.Process) -> float:
    try:
        jstat_out = subprocess.check_output(
            ["jstat", "-gc", str(process.pid)], 
            stderr=subprocess.DEVNULL, 
            universal_newlines=True
        )
        
        lines = jstat_out.strip().split('\n')
        if len(lines) == 2:
            data = dict(zip(lines[0].split(), lines[1].split()))
            current_used_kb = (float(data.get('EU', 0)) + float(data.get('S0U', 0)) +
                               float(data.get('S1U', 0)) + float(data.get('OU', 0)))
            return current_used_kb
            
    except subprocess.CalledProcessError:
        # jstat failed because the Java process just finished
        raise psutil.NoSuchProcess(process.pid)
    
    return 0

def get_kb_usage_cpp(process: psutil.Process) -> float:
     return process.memory_info().rss / 1024

mem_usage_func_dic = {PLang.CPP : get_kb_usage_cpp, PLang.JAVA : get_kb_usage_java}

def pool_proc_mem(process: subprocess.Popen, lang: PLang) -> float:
    peak_mem_kb = 0
    mem_usage_func = mem_usage_func_dic[lang]

    try:
        p = psutil.Process(process.pid)
        
        # Poll memory(rss) while the process is running and calculate the peak memory usage
        while process.poll() is None:
            try:
                curr_mem_kb = mem_usage_func(p)
                if curr_mem_kb > peak_mem_kb:
                    peak_mem_kb = curr_mem_kb
                
                time.sleep(0.01) 
            except psutil.NoSuchProcess:
                break # Program finished exactly between the checks
    except Exception as e:
        print(f"[ERROR]: Error profiling process: {e}")
        process.kill()
    
    return peak_mem_kb


def run_and_profile_cmd(cmd: list[str], lang:PLang, stdout: str|None=None, timeout: int|None=None) -> float:
    # print(f"Running cmd: {cmd}")

    if stdout is not None:
        stdout = open(stdout, 'w')

    process = subprocess.Popen(cmd, shell=False, stdout=stdout, stderr=subprocess.STDOUT)
    # start_time = time.perf_counter()

    # This finishes when the process does
    peak_mem_usage = pool_proc_mem(process, lang)
    
    # end_time = time.perf_counter()
    # execution_time = end_time - start_time
    
    # Convert to KB to MB
    peak_mem_usage /= 1024
    
    # Error checking
    err = process.wait(timeout=timeout)
    if err:
        print(f"[ERROR]: {cmd}: {err}")
    
    if stdout is not None:
        stdout.close()
    
    return peak_mem_usage

def get_paths(bench_name: str, predictor: str):
    out_path = (settings.out_files_base / bench_name / predictor / "log.txt")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    
    input_path = conv_trace(bench_name, predictor)

    return input_path, out_path

def run_predictor(bench_name: str, predictor: str, should_profile: bool):
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

        cmd = ["java", jvm_max_mem_flag, "-jar", settings.spdoffline_jar_path, f"-p={input_path}"]
        stdout = out_path
    
    if should_profile:
        peak_mem_usage = run_and_profile_cmd(cmd, lang, stdout)
        open(out_path, 'a').write(f"Peak memory usage = {peak_mem_usage} MB")
    else:
        run_cmd(cmd, stdout)

def benchmark(all_benchmarks: set[str], user_benchmarks: list[str], predictor: str, first:bool):
    RUN_IT_COUNT = 10
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
        for i in range(RUN_IT_COUNT):
            # Run predictor
            print(f"        Run: {i} / {RUN_IT_COUNT}", end=": ")
            run_predictor(bench, predictor, True)
            
            # Load log file
            comm_col, pred_col = from_log_file(settings.out_files_base / bench / predictor / "log.txt")
            comm_col_res, pred_col_res = comm_col, pred_col

            # Update sum
            avg_mem += pred_col[-1]
            avg_time += pred_col[-2]

            print(f"            {pred_col[-2]:.2f} sec, {pred_col[-1]:.2f} MB")
        
        pred_col_res[-1] = avg_mem / RUN_IT_COUNT
        pred_col_res[-2] = avg_time / RUN_IT_COUNT

        print(f"        Avg: {pred_col_res[-2]:.2f} sec, {pred_col_res[-1]:.2f} MB")
       
        # If this is the first predictor, also use it's common columns, otherwise just it's specific ones
        if first:
            rows[bench] = comm_col_res + pred_col_res
        else:
            rows[bench] = pred_col_res
    
    print(f"[TABLE INFO]: Compiled {len(rows)} rows for {predictor}")

    return rows


def main():
    parser = optparse.OptionParser()
    parser.add_option("-b", "--benchmarks", dest="benchmarks", default="all",
                      help="run the script on a selected group of benchmarks. " \
                            "Specify the names of the benchmarks and seperate them with a comma " \
                            "(e.g., Bensalem,Account) (Default: all).")
    parser.add_option("-i", "--ignore", dest="ignored_bench", default="",
                      help="ignores the specified benchmarks. Default None")
    
    parser.add_option("-p", "--predictor", dest="predictors", default="all",
                      help="runs only the desired predictors." \
                           "Specify the names of the predictors and seperate them with a comma " \
                           "For correct table construction, always specify SUnhang, and put it first" \
                           "(e.g SUnhang,SPDOffline) (Default: all)")

    parser.add_option("--vt", "--verbose_table", dest="verbose_table", default="N",
                      help="Specifies whether the table should be verbose(Y) or not(N). Default is N")
    
    (options, args) = parser.parse_args()
    
    # Get bennchmarks
    all_benchmarks = set(get_benchmarks())
    if options.benchmarks != "all":
        user_benchmarks = options.benchmarks.split(",")
    else:
        user_benchmarks = list(all_benchmarks)

    # Get predictors
    # Order matters: The first one to run should be SUnhang
    all_predictors = set(settings.predictors)
    if options.predictors == "all":
        user_predictors = settings.predictors
    else:
        user_predictors = options.predictors.split(",")

    if not user_predictors or user_predictors[0] != "SUnhang":
        print("[WARNING]: SUnhang should be the first predictor for correct table construction!")
    
    # Ignore undesired benchmarks
    ignored_bench = options.ignored_bench.split(",")
    print(f"Benchmarks to ignore: {ignored_bench}")

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
        rows.append(benchmark(all_benchmarks, user_benchmarks, pred, first))
    
    # Build table
    create_table_from_rows(rows, valid_pred)

if __name__ == "__main__":
    main()