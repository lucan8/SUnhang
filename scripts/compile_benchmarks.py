from pathlib import Path
import pandas as pd
import itertools
import os
from settings import settings
import optparse

#TODO: Somehow save the verbosity when saving the dataframe 

comm_col = [("Benchmark", ""), ("N", ""), ("T", ""), ("V", ""), ("L", ""), ("A/R", "")]

mini_col_base = ["Dlk", "Time(s)", "Mem(MB)"] # Used in both clean and verbose
mini_col_extra = ["Dep", "Cyc", "Abs"] # Used only in verbose

mini_col_verb = mini_col_extra + mini_col_base
mini_col_clean = mini_col_base

mini_col_int = mini_col_verb[:4] # Columns that should have integral type

# Settings. When verbose is set mini_columns should be automatically recalculated
verbose = False
mini_columns = []

def set_verbosity(verb:bool):
    global verbose, mini_columns
    verbose = verb
    mini_columns = get_mini_col()

def get_mini_col():
    global verbose
    if verbose:
        return mini_col_verb
    return mini_col_clean

# Returns the data from the log file in a format to be used for row construction in the table
def from_log_file(file_path: Path) -> tuple[list, list]:
    global verbose, mini_columns, comm_col

    # print(file_path)
    file = open(file_path, 'r')
    common_dic = {}
    pred_dic = {}
    empty_res = ([0] * (len(comm_col) - 1), [0] * (len(mini_columns))) 

    # Empty log file? Empty result
    lines = file.readlines()
    if not lines:
        return empty_res
    
    # Common columns
    for i in range(1, len(comm_col)):
        split_line = lines[i].strip().split(": ")
        common_dic[split_line[0].split()[1][:4]] = int(split_line[1])

    # Specific columns
    if verbose:
        for i in range(len(mini_col_extra)):
            split_line = lines[len(comm_col) + i].strip().split(": ")
            pred_dic[split_line[0].split()[1][:4]] = int(split_line[1])
    
    # Deaadlocks, time and memory
    pred_dic['dlk'] = int(lines[-3].strip().split(": ")[1])
    pred_dic['time'] = float(lines[-2].strip().split('=')[1].split()[0]) / 1000
    pred_dic['mem'] = float(lines[-1].strip().split('=')[1].split()[0])
    
    comm_l, pred_l = list(common_dic.values()), list(pred_dic.values())
    comm_l[0], comm_l[1] = comm_l[1], comm_l[0]
    
    return comm_l, pred_l

def get_df_col(predictors:list[str]=settings.available_predictors):
    global mini_columns, comm_col

    tuples = comm_col + list(itertools.product(predictors, mini_columns))
    columns = pd.MultiIndex.from_tuples(tuples)

    print(f"[TABLE INFO]: Compiled {len(columns)} columns!")
    return columns

def get_df_rows(pred: str, first:bool) -> dict[str, list[int]]:
    out_files = settings.out_files_base

    rows = {}
    for out_bench_path in out_files.iterdir():
        if out_bench_path.is_file():
            continue
        
        bench_name = out_bench_path.stem
        comm_col, pred_col = from_log_file(out_bench_path / pred / "log.txt")
        # Add the common columns only if this is the first predictor
        if first:
            rows[bench_name] = comm_col + pred_col
        else:
            rows[bench_name] = pred_col
    
    print(f"[TABLE INFO]: Compiled {len(rows)} rows({pred})!")
    
    return rows

def format_df(df: pd.DataFrame, predictors: list[str]=settings.available_predictors):
    global verbose

    if verbose:
        return format_df_verbose(df, predictors)
    return format_df_clean(df, predictors)

# This was used to format the tables in the paper.
# Each predictor only gives the dlk, time and memory
def format_df_clean(df: pd.DataFrame, predictors: list[str]=settings.available_predictors):
    global mini_columns, comm_col

    # Work on a copy to preserve the original structural layout
    res_df = df.copy()
    
    # Copy over and format the common metadata columns
    for col in comm_col[1:]:
        res_df[col] = df[col].astype(int).apply(format_suffix)

    for p in predictors:
        for mc in mini_columns:
            if mc in mini_col_int:
                res_df[(p, mc)] = df[(p, mc)].astype(int).apply(format_suffix)
            else:
                res_df[(p, mc)] = df[(p, mc)].apply(format_suffix)

    print("[TABLE INFO]: Formatted table!")
    return res_df


# This was used during development
def format_df_verbose(df: pd.DataFrame, predictors: list[str]=settings.available_predictors):
    global mini_columns, comm_col

    sep = r"\textbackslash{}" 
    sub_header = sep.join(mini_columns)
    
    # Set new dataframe with only one concatenated subcolumn
    new_tuples = comm_col + [(p, sub_header) for p in predictors]
    res_df = pd.DataFrame(columns=pd.MultiIndex.from_tuples(new_tuples))

    # Copy over the common stuff and apply formatting
    res_df[comm_col[0]] = df[comm_col[0]]
    for col in comm_col[1:]:
        res_df[col] = df[col].astype(int).apply(format_suffix)

    # Convert the types of predictor columns and add the separator between them
    for p in predictors:
        pred_info = []
        for mc in mini_columns:
            if mc in mini_col_int:
                col_info = df[(p, mc)].astype(int).apply(format_suffix)
            else:
                col_info = df[(p, mc)].apply(format_suffix)
            
            pred_info.append(col_info)

        res_df[(p, sub_header)] = pd.concat(pred_info, axis=1).agg(sep.join, axis=1)

    print("[TABLE INFO] Formatted verbose table!")

    return res_df

# Add \midrule and makes each component of the row bolt
def _format_total_row(latex_table: str):
    # Find total
    total_start = latex_table.find("Total")
    after_total = latex_table[total_start:]
    before_total = latex_table[:total_start]

    # Add midrule before it
    before_total += "\\midrule\n"

    # Separate the total row from what comes after
    total_end = after_total.find("\\\\")
    total_row = after_total[:total_end]
    after_total_row = after_total[total_end:]

    # Add bolt to each row element
    total_row = " & ".join(map(lambda s: f"\\textbf{{{s}}}", total_row.split(" & ")))

    # Stich everything together
    after_total = total_row + after_total_row
    latex_table = before_total + after_total

    return latex_table

def save_latex(df: pd.DataFrame, predictors: list[str] = settings.available_predictors):
    num_common = len(comm_col)
    num_sub_cols = len(mini_columns)

    common_fmt = "|l|" + "c|" * (num_common - 1)
    common_fmt = common_fmt[:-1] + "||"  # Double line after the common metadata block

    pred_fmts = []
    for _ in range(len(predictors) - 1):
        pred_fmts.append("c|" * (num_sub_cols - 1) + "c||")  # Double line between predictors
    
    # Last predictor doesn't have '||'
    pred_fmts.append("c|" * num_sub_cols)  

    # Concatenate common and the pred fmts
    column_format = common_fmt + "".join(pred_fmts)

    # Generate the baseline LaTeX table layout
    latex_table = df.to_latex(
        index=False, 
        multicolumn=True, 
        escape=False, 
        column_format=column_format,
        multicolumn_format="c|"
    )

    latex_table = _format_total_row(latex_table)

    # Replaces the single vertical pipe with a double vertical pipe for intermediate spanning blocks
    for p in predictors[:-1]:
        old_mc = f"\\multicolumn{{{num_sub_cols}}}{{c|}}{{{p}}}"
        new_mc = f"\\multicolumn{{{num_sub_cols}}}{{c||}}{{{p}}}"
        latex_table = latex_table.replace(old_mc, new_mc)

    # Clean up underscores and enforce text width constraint layout
    latex_table = latex_table.replace("_", r"\_")
    latex_table = "\\resizebox{\\textwidth}{!}{\n" + latex_table + "}\n"
    
    # Save table
    os.makedirs(settings.table_out_dir, exist_ok=True)
    settings.table_out_path.write_text(latex_table, encoding="utf-8")
    print(f"[TABLE INFO]: Saved table to {settings.table_out_path}")


# merged_rows[bench_name] = rows1[bench_name] + rows2[bench_name]
def merge_rows(rows1: dict[str, list[int]], rows2: dict[str, list[int]]) -> dict[str, list[int]]:
    merged_rows = {}

    for bench_name in rows1:
        if bench_name not in rows2:
            print(f"[WARN]: {bench_name} in rows1, but not in rows2!")
        else:
            merged_rows[bench_name] = rows1[bench_name] + rows2[bench_name]

    return merged_rows

# Calls merge_rows until there is only one dict standing
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
    
# Constructs a latex table using rows, formats and saves it
def create_table_from_rows(rows, predictors:list[str]=settings.available_predictors):
    if not rows or not rows[0]:
        print("[TABLE INFO] Empty rows, nothing to compile!")
        return

    # Construct dataframe
    cols = get_df_col(predictors)
    rows = from_row_dict_to_row_list(merge_rows_list(rows))
    
    df = pd.DataFrame(rows, columns=cols).sort_values('N')

    # Aggregate results into a "Total" column
    total_row = ["Total"] + df.select_dtypes(include='number').sum().to_list()
    df.loc[len(df)] = total_row

    # Save the dataframe object for later
    df.to_pickle(settings.table_df_out_path)

    # Format and save
    df = format_df(df, predictors)
    save_latex(df, predictors)


# Using all valid user predictors, builds the latex table from the log files
def run(user_predictors:list[str], all_predictors:set[str]):
    rows = []
    valid_pred = []

    first = True
    for pred in user_predictors:
        if pred not in all_predictors:
            print(f"Skipping invalid predictor: {pred}")
            continue

        valid_pred.append(pred)
        rows.append(get_df_rows(pred, first))
        first = False

    # Build table
    create_table_from_rows(rows, valid_pred)

# Loads the dataframe object, formats and constructs the latex table
def run_lazy():
    if not settings.table_df_out_path.exists():
        return
    
    df = pd.read_pickle(settings.table_df_out_path)
    
    format_df(df)
    save_latex(df)

def main():
    parser = optparse.OptionParser()

    parser.add_option("--bs", "--bench_suite", dest="bench_suite", default="cond_var",
                      help="run the script on the given benchmark suite. " \
                            f"Should be one of {settings.available_bench_suites}" \
                            "(Default: cond_var).")
    
    parser.add_option("-p", "--predictor", dest="predictors", default="all",
                      help="compiles benchmarks only the desired predictors." \
                           "Specify the names of the predictors and seperate them with a comma " \
                           "For correct table construction, always specify SUnhang, and put it first" \
                           "(e.g SUnhang,SPDOffline) (Default: all)")

    parser.add_option("--vt", "--verbose_table", dest="verbose_table", default="N",
                      help="Specifies whether the table should be verbose(Y) or not(N). Default is N")
    
    parser.add_option("--lt", "--lazy_table", dest="lazy_table", default="Y",
                      help="Specifies whether the table should be loaded using the tmp dataframe or built from log files." \
                           "Options: Y/N. Default N")
    
    (options, args) = parser.parse_args()

    # Set benchmark suite
    settings.set_bench_suite(options.bench_suite)
    if settings.bench_suite == "":
        print(f"[ERROR]: Invalid benchmark suite ({options.bench_suite}), should be one of {settings.available_bench_suites}")
        return
    
    print(f"Benchmark suite: {settings.bench_suite}")
    
    # Get predictors
    all_predictors = set(settings.available_predictors)
    if options.predictors == "all":
        user_predictors = settings.available_predictors
    else:
        user_predictors = options.predictors.split(",")

    # Set the verbosity flag for table construction
    verbose_table = False
    if options.verbose_table == "Y":
        verbose_table = True
    elif options.verbose_table != "N":
        print("[ERROR]: Invalid option for verbose_table. Should be Y or N")
        return
    
    print(F"[TABLE INFO]: Verbosity: {verbose_table}")
    set_verbosity(verbose_table)

    # Set the laziness flaf for table construction
    lazy_table = False
    if options.lazy_table == "Y":
        lazy_table = True
    elif options.lazy_table != "N":
        print("[ERROR]: Invalid option for lazy_table. Should be Y or N")
        return
    
    print(F"[TABLE INFO]: Laziness: {lazy_table}")

    # Run
    if lazy_table:
        run_lazy()
    else:
        run(user_predictors, all_predictors)

if __name__ == "__main__":
    main()