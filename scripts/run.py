import os
import zipfile
import optparse
import subprocess
from pathlib import Path

out_files_base = "benchmarks/original/output"
bench_in_path = "benchmarks/original/data"
predictor1 = "SUnhang_no_1_lev_locks-no_dead_th_fp"
predictor2 = "SUnhang_no_1_lev_locks-no_dead_th_fp-no_1_th_ev"
predictor3 = "SUnhang_no_1_lev_locks-no_dead_th_fp-no_1_th_ev-reen_locks"
predictor = predictor3

# cmd: [exe, exe_arg1, exe_arg2...]
def execute_cmd(cmd: list[str]):
    p = subprocess.Popen(cmd, shell=True)
    err = p.wait()
    if err:
        print(f"[ERROR]: {cmd}: {err}")

def run_cpp_spdoffline(bench_name):
    global out_files_base, bench_in_path

    print(f"Running benchmark: {bench_name}...\n")

    out_path = (Path(out_files_base) / bench_name / predictor / "log.txt")
    extra_out_path = (Path(out_files_base) / bench_name / predictor / "extra_log.txt")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    
    input_path = os.path.join(bench_in_path, bench_name + ".std")

    print("Input path: ", input_path)
    print("Output path: ", out_path)
    # print("Extra output path: ", extra_out_path)
    
    cmd = [Path("./build/SUnhang.exe").resolve(), input_path, out_path]
    execute_cmd(cmd)
    print()

def get_benchmarks():
    return [path.stem for path in Path(bench_in_path).iterdir()]

def main():
    global bench_in_path
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
        run_cpp_spdoffline(bench)

main()