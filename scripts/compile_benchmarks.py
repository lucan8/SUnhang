from pathlib import Path
import pandas as pd
import itertools
import os
from copy import deepcopy

root_dir = Path(os.path.dirname(os.path.dirname(__file__))) / "benchmarks"
bench_suite = "generated"
bench_root_path = root_dir / bench_suite

out_files_base = bench_root_path / "output"
bench_in_path = bench_root_path / "data"

sunhang_pred_extra_title = "-1-lvl-locks-as-deps"

sunhang_base_name = "SUnhang"
spdoffline_name, sunhang_name = "SPDOffline", sunhang_base_name + sunhang_pred_extra_title

predictors = [spdoffline_name, sunhang_base_name, sunhang_name]
mini_columns = ["dep", "cyc", "abs", "dlk", "time"]

# ignored_bench = set(["eclipse", "jigsaw"])
# THIS SHOULD NOT BE IGNORED IN THE FUTURE!
ignored_bench = set([])
ignored_pred = set([])
    
def from_log_file_SPD(file_path: Path) -> list:
    file = open(file_path, 'r')
    dic = {}
    
    line = file.readline()
    if not line:
        return [0] * 5
    
    # Skip first lines
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
    
    # Skip until end
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
    
    # Skip first 5 lines
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

    # Skip until end
    for line in file:
        pass

    dic["time"] = float(line.strip())
    
    return list(dic.values())

log_conv_func = {spdoffline_name : from_log_file_SPD, sunhang_name: from_log_file_SUnhang, sunhang_base_name: from_log_file_SUnhang}
def from_log_file(file_path: Path, pred: str) -> list:
    return log_conv_func[pred](file_path)

def get_df_col():
    global predictors, mini_columns

    tuples = [("Benchmark", "")]
    tuples.extend(itertools.product(predictors, mini_columns))
    
    columns = pd.MultiIndex.from_tuples(tuples)

    print(f"[INFO]: Compiled {len(columns)} columns!")
    return columns

def get_df_rows(pred: str) -> dict[str, list[int]]:
    global out_bench_path, out_files_base

    out_files = out_files_base

    rows = {}
    for out_bench_path in Path(out_files).iterdir():
        if out_bench_path.is_file():
            continue
        
        bench_name = out_bench_path.stem
        if bench_name in ignored_bench:
            continue
        
        row = []

        info = from_log_file(out_bench_path / pred / "log.txt", pred)
        row.extend(info)
        
        rows[bench_name] = row
    
    print(f"[INFO]: Compiled {len(rows)} rows({pred})!")
    
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
    global bench_root_path

    num_columns = len(df.columns)
    column_format = "l|" + "c|" * (num_columns - 1)

    latex_table = df.to_latex(
        index=False, 
        multicolumn=True, 
        escape=False, 
        column_format=column_format,
        multicolumn_format="c|"
    )

    latex_table = latex_table.replace("_", r"\_")
    latex_table = "\\resizebox{\\textwidth}{!}{\n" + latex_table + "}\n"
    
    table_out_path = Path(bench_root_path) / "tables" / "main.tex"
    os.makedirs(table_out_path.parent, exist_ok=True)
    table_out_path.write_text(latex_table, encoding="utf-8")
    print(f"[INFO]: Saved table to {table_out_path}")

def merge_rows(rows1: dict[str, list[int]], rows2: dict[str, list[int]]) -> dict[str, list[int]]:
    merged_rows = {}

    for bench_name in rows1:
        if bench_name not in rows2:
            print(f"[WARN]: {bench_name} in rows1, but not in rows2!")
        else:
            merged_rows[bench_name] = rows1[bench_name] + rows2[bench_name]

    return merged_rows

def merge_rows_list(row_list: list[dict[str, list[int]]]) -> dict[str, list[int]]:
    rows = merge_rows(row_list[0], row_list[1])
    
    for i in range(2, len(row_list)):
        rows = merge_rows(rows, row_list[i])
    
    return rows

def from_row_dict_to_row_list(row_dic: dict[str, list[int]]) -> list[list]:
    return [[bench_name] + r for bench_name, r in row_dic.items()]

def main():
    cols = get_df_col()

    rows = [get_df_rows(pred) for pred in predictors]
    rows = from_row_dict_to_row_list(merge_rows_list(rows))

    df = pd.DataFrame(rows, columns=cols)
    # print(df)

    # Aggregate results into a "Total" column
    total_row = ["Total"] + df.select_dtypes(include='number').sum().to_list()
    df.loc[len(df)] = total_row

    df = format_df(df)
    save_latex(df)

main()