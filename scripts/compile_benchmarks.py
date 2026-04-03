from pathlib import Path
import pandas as pd
import itertools
import os
from copy import deepcopy

root_path = "benchmarks/generated/"
out_files_base = f"{root_path}/output"
bench_in_path = f"{root_path}/data"

predictors = ["SPD", "SUnhang1", "SUnhang2", "SUnhang3", "SUnhang4", "SUnhang5"]
mini_columns = ["dep", "cyc", "abs", "dlk", "time"]
# ignored_bench = set(["eclipse", "jigsaw"])
# THIS SHOULD NOT BE IGNORED IN THE FUTURE!
ignored_bench = set(["dead-th-fp", "cond-var-fn", "cond-var-fp", "cond-var-tn", "MyHashMap", "dead"])

spd_benchs = []
def from_log_file_SPD(file_path: Path) -> list:
    file = open(file_path, 'r')
    dic = {}
    
    line = file.readline()
    if not line:
        return [0] * 5
    
    # Skip first  lines
    for i in range(6):
        file.readline()

    dic["deps"] = 0
    # Next 3 lines are actually relevant
    for i in range(2):
        split_line = file.readline().strip().split(": ")
        dic[split_line[0].split()[1][:3]] = int(split_line[1])
    
    # Skip until we see num_deadlocks
    while True:
        split_line = file.readline().strip().split(": ")
        if split_line[0] == "num deadlocks":
            dic["dlk"] = int(split_line[1])
            break
    
    for line in file:
        pass

    dic["time"] = float(line.strip())
    
    return list(dic.values())

def from_log_file_SUnhang(file_path: Path) -> list:
    file = open(file_path, 'r')
    dic = {}
    
    line = file.readline()
    if not line:
        return [0] * 5
    
    # Skip first  lines
    for i in range(5):
        file.readline()
    
    # Next 3 lines are actually relevant
    for i in range(3):
        split_line = file.readline().strip().split(": ")
        dic[split_line[0].split()[1][:3]] = int(split_line[1])
    
    # Skip until we see num_deadlocks
    while True:
        split_line = file.readline().strip().split(": ")
        if split_line[0] == "num deadlocks":
            dic["dlk"] = int(split_line[1])
            break
    
    agg_time = 0
    for i in range(4):
        line = file.readline()
        agg_time += int(line.split(" = ")[1].split()[0])

    dic["time"] = agg_time / 1000 # Convert to seconds
    
    return list(dic.values())

def from_log_file(file_path: Path, is_spd_pred: bool) -> list:
    if is_spd_pred:
        return from_log_file_SPD(file_path)
    
    return from_log_file_SUnhang(file_path)

def get_df_col():
    global predictors, mini_columns

    tuples = [("Benchmark", "")]
    tuples.extend(itertools.product(predictors, mini_columns))
    
    columns = pd.MultiIndex.from_tuples(tuples)

    print(f"[INFO]: Compiled {len(columns)} columns!")
    return columns

def get_df_rows(is_spd_pred: bool) -> dict[str, list[int]]:
    global out_bench_path, out_files_base

    if is_spd_pred:
        out_files = out_files_base + "_SPD"
    else:
        out_files = out_files_base + "_SUnhang"

    rows = {}
    for out_bench_path in Path(out_files).iterdir():
        if out_bench_path.is_file():
            continue
        
        bench_name = out_bench_path.stem.split("_")[0]
        if bench_name in ignored_bench:
            continue

        row = []

        for i, out_pred_path in enumerate(out_bench_path.iterdir()):
            info = from_log_file(out_pred_path / "log.txt", is_spd_pred)
            row.extend(info)
        
        rows[bench_name] = row
    
    print(f"[INFO]: Compiled {len(rows)} rows({"spd" if is_spd_pred else "SUnhang"})!")
    
    return rows

def format_df(df: pd.DataFrame):
    global predictors

    sep = r"\textbackslash{}" 
    sub_header = sep.join(mini_columns)
    
    # Set new dataframe with only one concatenated subcolumn
    new_tuples = [("Benchmark", "")] + [(p, sub_header) for p in predictors]
    res_df = pd.DataFrame(columns=pd.MultiIndex.from_tuples(new_tuples))

    # Copy over the Benchmark names
    res_df[("Benchmark", "")] = df[("Benchmark", "")]

    for p in predictors:
        info = [
            df[(p, "dep")].astype(int).astype(str),
            df[(p, "cyc")].astype(int).astype(str),
            df[(p, "abs")].astype(int).astype(str),
            df[(p, "dlk")].astype(int).astype(str),
            df[(p, "time")].map(lambda x: f"{x:.2f}")
        ]

        res_df[(p, sub_header)] = pd.concat(info, axis=1).agg(sep.join, axis=1)
    
    print("[INFO] Formatted table!")

    return res_df

def save_latex(df: pd.DataFrame):
    global root_path

    num_columns = len(df.columns)
    column_format = "l|" + "c|" * (num_columns - 1)

    latex_table = df.to_latex(
        index=False, 
        multicolumn=True, 
        escape=False, 
        column_format=column_format,
        multicolumn_format="c|"
    )
    latex_table = "\\resizebox{\\textwidth}{!}{\n" + latex_table + "}\n"
    
    table_out_path = Path(root_path) / "tables" / "optimization.tex"
    os.makedirs(table_out_path.parent, exist_ok=True)
    table_out_path.write_text(latex_table, encoding="utf-8")
    print(f"[INFO]: Saved table to {table_out_path}")

def merge_rows(rows1: dict[str, list[int]], rows2: dict[str, list[int]]) -> list[list]:
    merged_rows = []

    for bench_name in rows1:
        if bench_name not in rows2:
            print(f"[WARN]: {bench_name} in rows1, but not in rows2!")
        else:
            merged_rows.append([bench_name] + rows1[bench_name] + rows2[bench_name])

    return merged_rows
    
def main():
    spd_rows, sunhang_rows, cols = get_df_rows(True), get_df_rows(False), get_df_col()
    rows = merge_rows(spd_rows, sunhang_rows)

    df = pd.DataFrame(rows, columns=cols)
    # print(df)

    # Aggregate results into a "Total" column
    total_row = ["Total"] + df.select_dtypes(include='number').sum().to_list()
    df.loc[len(df)] = total_row

    df = format_df(df)
    save_latex(df)

main()