from pathlib import Path
import pandas as pd
import itertools
import os
from copy import deepcopy
import settings

# TODO: MAKE COMMON COLUMNS INDEPENDENT OF PREDICTOR ORDER
common_columns = [("Benchmark", ""), ("N", ""), ("T", ""), ("V", ""), ("L", ""), ("A/R", "")]
mini_columns = ["dep", "cyc", "abs", "dlk", "time(s)", "mem(MB)"]

# ignored_bench = set(["eclipse", "jigsaw"])
# THIS SHOULD NOT BE IGNORED IN THE FUTURE!
ignored_bench = set([])
ignored_pred = set([])

def from_log_file_SPD(file_path: Path) -> list:
    # print(file_path)
    file = open(file_path, 'r')
    dic = {}
    empty_res = [0] * (len(mini_columns))

    lines = file.readlines()
    if not lines:
        return empty_res
    
    # Skip first lines
    lines = lines[6:]

    # Next 3 lines are actually relevant
    for i in range(3):
        split_line = lines[i].strip().split(": ")
        dic[split_line[0].split()[1][:4]] = int(split_line[1])
    
    # Deaadlocks, time and memory
    dic['dlk'] = int(lines[-3].strip().split(": ")[1])
    dic['time'] = float(lines[-2].strip().split('=')[1].split()[0]) / 1000
    dic['mem'] = float(lines[-1].strip().split('=')[1].split()[0])

    return list(dic.values())

def from_log_file_SUnhang(file_path: Path) -> list:
    # print(file_path)
    file = open(file_path, 'r')
    dic = {}
    empty_res = [0] * (len(common_columns) + len(mini_columns) - 1)
    
    lines = file.readlines()
    if not lines:
        return empty_res
    
    # Common columns
    for i in range(1, len(common_columns)):
        split_line = lines[i].strip().split(": ")
        dic[split_line[0].split()[1][:4]] = int(split_line[1])
    
    # Specific columns
    for i in range(3):
        split_line = lines[len(common_columns) + i].strip().split(": ")
        dic[split_line[0].split()[1][:3]] = int(split_line[1])

    # Deadlocks, time and memory
    dic['dlk'] = int(lines[-3].strip().split(": ")[1])
    dic['time'] = float(lines[-2].strip().split('=')[1].split()[0]) / 1000
    dic['mem'] = float(lines[-1].strip().split('=')[1].split()[0])

    # Swap the places of N and T
    res = list(dic.values())
    res[0], res[1] = res[1], res[0]

    return res

log_conv_func = {settings.spdoffline_name : from_log_file_SPD, settings.sunhang_name: from_log_file_SUnhang,
                 settings.sunhang_base_name: from_log_file_SUnhang}

def from_log_file(file_path: Path, pred: str) -> list:
    return log_conv_func[pred](file_path)

def get_df_col(predictors:list[str]=settings.predictors):
    tuples = common_columns + list(itertools.product(predictors, mini_columns))
    columns = pd.MultiIndex.from_tuples(tuples)
    # print(columns)

    print(f"[INFO]: Compiled {len(columns)} columns!")
    return columns

def get_df_rows(pred: str) -> dict[str, list[int]]:
    out_files = settings.out_files_base

    rows = {}
    for out_bench_path in Path(out_files).iterdir():
        if out_bench_path.is_file():
            continue
        
        bench_name = out_bench_path.stem
        if bench_name in ignored_bench:
            continue

        info = from_log_file(out_bench_path / pred / "log.txt", pred)
        
        rows[bench_name] = info
    
    print(f"[INFO]: Compiled {len(rows)} rows({pred})!")
    
    return rows

def format_df(df: pd.DataFrame, predictors: list[str]=settings.predictors):
    sep = r"\textbackslash{}" 
    sub_header = sep.join(mini_columns)
    
    # Set new dataframe with only one concatenated subcolumn
    new_tuples = common_columns + [(p, sub_header) for p in predictors]
    res_df = pd.DataFrame(columns=pd.MultiIndex.from_tuples(new_tuples))

    # Copy over the common stuff and apply formatting
    res_df[common_columns[0]] = df[common_columns[0]]
    for col in common_columns[1:]:
        res_df[col] = df[col].astype(int).apply(format_suffix)

    # Convert the types of predictor columns and add the separator between them
    for p in predictors:
        info = [
            df[(p, "dep")].astype(int).apply(format_suffix),
            df[(p, "cyc")].astype(int).apply(format_suffix),
            df[(p, "abs")].astype(int).apply(format_suffix),
            df[(p, "dlk")].astype(int).apply(format_suffix),
            df[(p, "time(s)")].apply(format_suffix),
            df[(p, "mem(MB)")].apply(format_suffix)
        ]

        res_df[(p, sub_header)] = pd.concat(info, axis=1).agg(sep.join, axis=1)

    print("[INFO] Formatted table!")

    return res_df

def save_latex(df: pd.DataFrame):
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
    
    table_out_path = settings.bench_dir / "tables" / "main.tex"
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
    if len(row_list) == 0:
        return {}
    
    if len(row_list) == 1:
        return row_list[0]
    
    rows = merge_rows(row_list[0], row_list[1])
    
    for i in range(2, len(row_list)):
        rows = merge_rows(rows, row_list[i])
    
    return rows

def from_row_dict_to_row_list(row_dic: dict[str, list[int]]) -> list[list]:
    return [[bench_name] + r for bench_name, r in row_dic.items()]

def format_suffix(num):
    if isinstance(num, str):
        return num
    
    if pd.isna(num):
        return num
    
    num_new, prefix = num, ""
    
    if num >= 1_000_000:
        num_new, prefix = num / 1_000_000, "M"
    elif num >= 1_000:
        num_new, prefix = num / 1_000, "K"
    
    if isinstance(num, int):
        num_new = int(num_new)
        return f"{num_new}{prefix}"
    
    return f"{num_new:.2f}{prefix}"
    

def compile_rows(rows, predictors:list[str]=settings.predictors):
    if not rows:
        print("[INFO] Empty rows, nothing to compile!")
        return
    
    cols = get_df_col(predictors)
    rows = from_row_dict_to_row_list(merge_rows_list(rows))
    
    df = pd.DataFrame(rows, columns=cols).sort_values('N')

    # Aggregate results into a "Total" column
    total_row = ["Total"] + df.select_dtypes(include='number').sum().to_list()
    df.loc[len(df)] = total_row

    df = format_df(df, predictors)
    save_latex(df)

def main():
    rows = [get_df_rows(pred) for pred in settings.predictors]

    compile_rows(rows)

if __name__ == "__main__":
    main()