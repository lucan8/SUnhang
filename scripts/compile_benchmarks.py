from pathlib import Path
import pandas as pd
import itertools
import os

root_path = "benchmarks/original/"
out_files_base = f"{root_path}/output"
bench_in_path = f"{root_path}/data"

predictors = ["SUnhang1", "SUnhang2", "SUnhang3"]
mini_columns = ["dep", "cyc", "abs", "dlk", "time"]
ignored_bench = ["eclipse", "JDBCMySQL-3"]

def from_log_file(file_path: Path) -> list:
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
    
    # Go until the last line where the is only a number
    agg_time = 0
    for i in range(4):
        line = file.readline()
        agg_time += int(line.split(" = ")[1].split()[0])

    dic["time"] = agg_time / 1000 # Convert to seconds
    
    return list(dic.values())

def get_df_col():
    global predictors, mini_columns

    tuples = [("Benchmark", "")]
    tuples.extend(itertools.product(predictors, mini_columns))
    
    columns = pd.MultiIndex.from_tuples(tuples)

    print("[INFO]: Compiled columns!")
    return columns

def get_df_rows() -> list[list[int]]:
    global out_bench_path, out_files_base

    rows = []
    for out_bench_path in Path(out_files_base).iterdir():
        if out_bench_path.is_file():
            continue
        
        bench_name = out_bench_path.stem.split("_")[0]
        if bench_name in ignored_bench:
            continue

        row = [bench_name]

        for i, out_pred_path in enumerate(out_bench_path.iterdir()):
            info = from_log_file(out_pred_path / "log.txt")
            row.extend(info)
        
        rows.append(row)
    
    print("[INFO]: Compiled rows!")
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

    table_out_path = Path(root_path) / "tables" / "optimization.tex"
    os.makedirs(table_out_path.parent, exist_ok=True)

    table_out_path.write_text(latex_table, encoding="utf-8")
    print(f"[INFO]: Saved table to {table_out_path}")

def main():
    df = pd.DataFrame(get_df_rows(), columns=get_df_col())

    # Aggregate results into a "Total" column
    total_row = ["Total"] + df.select_dtypes(include='number').sum().to_list()
    df.loc[len(df)] = total_row

    df = format_df(df)
    save_latex(df)

main()