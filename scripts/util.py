import subprocess
from settings import settings

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
    return [path.stem for path in settings.trace_bin_java_enc_dir.iterdir()]

