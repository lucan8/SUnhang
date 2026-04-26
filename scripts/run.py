import os
import zipfile
import optparse
import subprocess
from pathlib import Path
import time

root_dir = os.path.dirname(os.path.dirname(__file__))
bench_suite = "generated"
out_files_base = f"{root_dir}/benchmarks/{bench_suite}/output"
trace_path = f"{root_dir}/benchmarks/{bench_suite}/traces"
bin_dir = Path(root_dir) / "bin"


# cmd: [exe, exe_arg1, exe_arg2...]
def execute_cmd(cmd: list[str]|str, stdout: str|None=None, timeout: str|None=None):
    print(f"Running cmd: {cmd}")

    if stdout is not None:
        stdout = open(stdout, 'w')
    
    start_time = time.perf_counter()

    p = subprocess.Popen(cmd, shell=True, stdout=stdout, stderr=subprocess.STDOUT)
    err = p.wait(timeout=timeout)
    
    end_time = time.perf_counter()
    execution_time = end_time - start_time
    
    if err:
        print(f"[ERROR]: {cmd}: {err}")
    
    return execution_time

def get_paths(bench_name: str, predictor: str):
    global out_files_base, trace_path

    out_path = (Path(out_files_base) / bench_name / predictor / "log.txt")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    
    input_path = Path(trace_path) / "std" / (bench_name + ".std")
    if predictor == "SPDOffline":
        input_path = std_to_bin_trace(input_path)

    return input_path, out_path
    
def run_sunhang(bench_name: str):
    global out_files_base, trace_path

    print(f"Running benchmark: {bench_name}...\n")

    input_path, out_path = get_paths(bench_name, "SUnhang")

    print("Input path: ", input_path)
    print("Output path: ", out_path)
    
    pred_path = bin_dir / "SUnhang"
    cmd = [pred_path, input_path, out_path]

    execution_time = execute_cmd(cmd)
    open(out_path, 'a').write(f"\n{execution_time:.2f}")
    print()
    
def run_spd_offline(bench_name: str):
    print(f"Running benchmark: {bench_name}...\n")

    input_path, out_path = get_paths(bench_name, "SPDOffline")

    print("Input path: ", input_path)
    print("Output path: ", out_path)

    pred_path = bin_dir / "fat_spdoffline.jar"
    cmd = f"java -jar {pred_path} -p={input_path}"

    execution_time = execute_cmd(cmd, out_path)
    open(out_path, 'a').write(f"\n{execution_time:.2f}")
    print()


def std_to_bin_trace(input_path: Path):
    output_path = input_path.parent.parent / "bin" / (input_path.stem + ".data")
   
    jar_path = bin_dir / "fat_convert.jar"
    cmd = f'java -jar {jar_path} -p={input_path} -f=std -q={output_path}'
    execute_cmd(cmd)

    return output_path

def get_benchmarks():
    return [path.stem for path in (Path(trace_path) / "std").iterdir()]

def main():
    global trace_path
    parser = optparse.OptionParser()
    parser.add_option("-b", "--benchmarks", dest="benchmarks", default="all",
                    help="run the script on a selected group of benchmarks. " \
                            "Specify the names of the benchmarks and seperate them with a comma " \
                            "(e.g., Bensalem,Account) (Default: all).")
    
    (options, args) = parser.parse_args()
    if options.benchmarks == "all":
        benchmarks = get_benchmarks()
    else:
        benchmarks = options.benchmarks.split(",")

    for bench in benchmarks:
        run_sunhang(bench)
        run_spd_offline(bench)

main()