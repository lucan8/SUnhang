import os
import zipfile
import optparse
import subprocess
from pathlib import Path

out_files_base = "benchmarks/output"
bench_in_path = "benchmarks/data"

def create_out_folders(path):
    if not os.path.exists(os.path.join(path)):
        os.makedirs(os.path.join(path))

# [exe, exe_arg1, exe_arg2...]
def execute_cmd(cmd: list[str]):
    p = subprocess.Popen(cmd, shell=True)
    p.wait()

def run_cpp_spdoffline(bench_name, bench_path):
    global out_files_base, bench_in_path

    out_path = (Path(out_files_base) / bench_name / "SUnhang" / "log.txt").resolve()
    create_out_folders(os.path.dirname(out_path))
    
    input_path = os.path.join(bench_in_path, bench_path)

    print("Input path: ", input_path)
    print("Output path: ", out_path)
    
    cmd = [Path("./build/SUnhang.exe").resolve(), bench_path, out_path]
    execute_cmd(cmd)

def main():
    global bench_in_path

    for i, path in enumerate(Path(bench_in_path).iterdir()):
        run_cpp_spdoffline(path.stem, str(path.resolve()))

main()