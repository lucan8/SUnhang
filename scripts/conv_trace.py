import optparse
from pathlib import Path
from settings import settings
import numpy as np
from util import run_cmd, get_benchmarks

# Converts trace from "from_fmt" to "to_fmt". Returns the path to the binary trace
def conv_trace(bench_name: str, from_fmt="java_enc", to_fmt="local_enc", lazy:bool=True) -> Path|None:
    trace_path_map = {"std" :  settings.trace_std_dir / (f"{bench_name}.std"),
                     "java_enc" : settings.trace_bin_java_enc_dir / (bench_name + f".data"),
                     "local_enc" : settings.trace_bin_loc_enc_dir / (bench_name + f".data")}
    
    invalid_comb = {"java_enc":{"std"}, "local_enc":{"java_enc"}}

    # Input validation
    if from_fmt not in trace_path_map:
        print(f"    [ERROR]: Invalid from_fmt ({from_fmt}), should be one of {list(trace_path_map.keys())}")
        return None
    from_path = trace_path_map[from_fmt]

    # What are you even converting?
    if from_fmt == to_fmt:
        return from_path
    
    if to_fmt not in trace_path_map:
        print(f"    [ERROR]: Invalid to_fmt ({to_fmt}), should be one of {list(trace_path_map.keys())}")
        return None
    to_path = trace_path_map[to_fmt]

    if from_fmt in invalid_comb and to_fmt in invalid_comb[from_fmt]:
        print(f"    [ERROR]: Cannot convert from {from_fmt} to {to_fmt}")
        return None
    
    # The java trace converter ignores cond var actions which is bad for these benchmarks
    if from_fmt == "java_enc" and to_fmt == "local_enc" and settings.has_cond_var_suite():
        print(f"    [ERROR]: Converting from {from_fmt} to {to_fmt} for bench suite {settings.bench_suite} might lose events")
        return None
    
    # Determine the command to run
    if to_fmt == "java_enc":
        cmd = ['java', '-jar', settings.spd_trace_conv_jar_path, f"-p={from_path}", "-f=std", f"-q={to_path}"]
    else:
        # lazy = False
        cmd = [settings.sunhang_conv_exe_path, from_path, to_path]

    if not lazy or not to_path.exists():
        # print(f"    Converting trace from {from_fmt} to {to_fmt} for {bench_name}")
        run_cmd(cmd)
    else:
        # print(f"    Skipping trace conversion {from_fmt} to {to_fmt} for {bench_name}")
        ...

    return to_path

# Converts the trace based on predictor. Returns the path to the binary trace
def get_fmt_pair_for_pred(predictor: str) -> tuple[str, str]:
    if predictor == settings.sunhang_name:
        if settings.has_cond_var_suite():
            return "std", "local_enc"
        else:
            return "java_enc", "local_enc"
    elif predictor == settings.spdoffline_name:
        return "std", "java_enc"
    
    print(f"    [ERROR]: Invalid predictor ({predictor}) should be one of {settings.available_predictors}")
    return "", ""

def main():
    parser = optparse.OptionParser()
    parser.add_option("-b", "--benchmarks", dest="benchmarks", default="all",
                      help="converts the trace from std to bin for the specified benchmarks. Default is all")
    parser.add_option("-i", "--ignore", dest="ignored_bench", default="",
                      help="ignores the specified benchmarks")
    parser.add_option("--from", dest="from_fmt", default="java_enc",
                      help="converts from this format. Default is java_enc")
    parser.add_option("--to", dest="to_fmt", default="local_enc",
                      help="converts to this format. Default is local_enc")
    parser.add_option("-p", dest="predictor", default="",
                      help="converts for the given predictor. If none is specified, --from and --to will be used." \
                           "if pred is specified it will have priority over the rest"
                           "Default is ''(no predictor)")
    
    (options, args) = parser.parse_args()
    
    # Set benchmarks
    if options.benchmarks == "all":
        benchmarks = get_benchmarks()
    else:
        benchmarks = options.benchmarks.split(",")
    
    # Ignore undesired benchmarks
    ignored_bench = options.ignored_bench.split(",")
    print(f"Benchmarks to ignore: {ignored_bench}")

    # Set the formats and predictor
    from_fmt, to_fmt = options.from_fmt, options.to_fmt
    predictor = options.predictor

    # Calculate the formats based on the predictor
    if predictor:
        from_fmt, to_fmt = get_fmt_pair_for_pred(predictor)
        if not from_fmt:
            return
    
    # Convert for each benchmark
    for bench in benchmarks:
        if bench in ignored_bench:
            print(f"Ignoring bench {bench}")
            continue
        print(f"Benchmark: {bench}")
        conv_trace(bench, from_fmt, to_fmt)
    
if __name__ == "__main__":
    main()