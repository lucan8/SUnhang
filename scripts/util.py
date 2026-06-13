import subprocess
from settings import settings
from zipfile import ZipFile, ZIP_DEFLATED
from pathlib import Path

# Just runs the command and waits
def run_cmd(cmd: list[str], stdout: str|None=None, timeout: int|None=None):
    # print(f"Running cmd: {cmd}")

    if stdout is not None:
        stdout = open(stdout, 'w')

    process = subprocess.Popen(cmd, shell=False, stdout=stdout, stderr=subprocess.STDOUT)

    err = process.wait(timeout=timeout)
    if err:
        print(f"[ERROR]: {cmd}: {err}")

    if stdout is not None:
        stdout.close()


def get_benchmarks() -> list[str]:
    bench_java_enc = [path.stem for path in settings.trace_bin_java_enc_dir.iterdir()]
    bench_std = [path.stem for path in settings.trace_std_dir.iterdir()]

    return list(set(bench_java_enc + bench_std))

def unzip_file(zip_path: Path):
    with ZipFile(zip_path, 'r') as my_zip:
        my_zip.extractall(zip_path.parent)

def zip_file(file_path: Path, zip_path: Path):
    with ZipFile(zip_path, 'w',  ZIP_DEFLATED) as myzip:
        myzip.write(file_path, arcname=file_path.name)