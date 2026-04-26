from pathlib import Path
import os
import shutil
import sys

for bench_suite_dir in (Path(os.path.dirname(__file__)) / "benchmarks").iterdir():
    std_dir = bench_suite_dir / "trace" / "std"
    data_dir = bench_suite_dir / "data"
    if not data_dir.exists():
        continue

    print(std_dir)
    print(data_dir)

    os.makedirs(bench_suite_dir / "trace" / "bin", exist_ok=True)
    os.makedirs(std_dir, exist_ok=True)

    for file in data_dir.iterdir():
        if file.suffix == ".std":
            shutil.move(file, std_dir)

    # shutil.rmtree(data_dir)
