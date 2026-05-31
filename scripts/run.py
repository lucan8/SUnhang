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
from util import get_benchmarks

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
        print(f"Error profiling process: {e}")
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
    
    # meta_path = settings.trace_meta_dir / (bench_name + ".meta")
    input_path = conv_trace(bench_name, predictor)

    return input_path, out_path

def run_predictor(bench_name: str, predictor: str):
    print(f"Running predictor: {predictor}")
    input_path, out_path = get_paths(bench_name, predictor)

    print("Input path: ", input_path)
    print("Output path: ", out_path)

    stdout = None
    if predictor == settings.sunhang_name:
        lang = PLang.CPP
        cmd = [settings.sunhang_exe_path, input_path, out_path]
    else:
        lang = PLang.JAVA
        cmd = ["java", "-Xmx8g", "-jar", settings.spdoffline_jar_path, f"-p={input_path}"]
        stdout = out_path
    
    peak_mem_usage = run_and_profile_cmd(cmd, lang, stdout)
    open(out_path, 'a').write(f"Peak memory usage = {peak_mem_usage} MB")
    print()

def main():
    parser = optparse.OptionParser()
    parser.add_option("-b", "--benchmarks", dest="benchmarks", default="all",
                      help="run the script on a selected group of benchmarks. " \
                            "Specify the names of the benchmarks and seperate them with a comma " \
                            "(e.g., Bensalem,Account) (Default: all).")
    parser.add_option("-i", "--ignore", dest="ignored_bench", default="",
                      help="ignores the specified benchmarks")
    
    (options, args) = parser.parse_args()
    if options.benchmarks == "all":
        benchmarks = get_benchmarks()
    else:
        benchmarks = options.benchmarks.split(",")
    
    ignored_bench = options.ignored_bench.split(",")
    print(f"Benchmarks to ignore: {ignored_bench}")
    for bench in benchmarks:
        if bench in ignored_bench:
            print(f"Ignoring bench {bench}")
            continue
        # run_predictor(bench, settings.spdoffline_name)
        run_predictor(bench, settings.sunhang_name)

if __name__ == "__main__":
    main()