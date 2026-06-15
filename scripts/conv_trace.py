import optparse
from pathlib import Path
from settings import settings
from util import run_cmd, get_benchmarks, unzip_file, zip_file
import os

# TODO: Use lazy
# TODO: Make fallback logic for SPDOffline

# Converts trace from "from_fmt" to "to_fmt". Returns the path to trace
# Additionally zips traces if needed
def conv_trace(bench_name: str, from_fmt="java_enc", to_fmt="local_enc", lazy:bool=True) -> Path|None:
    trace_path_map = {"std" :  settings.trace_std_dir / (f"{bench_name}.std"),
                     "java_enc" : settings.trace_bin_java_enc_dir / (bench_name + f".data"),
                     "local_enc" : settings.trace_bin_loc_enc_dir / (bench_name + f".data")}
    
    invalid_comb = {"java_enc":{"std"}}

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
    if from_fmt == "std" and to_fmt == "java_enc":
        cmd = ['java', '-jar', settings.spd_trace_conv_jar_path, f"-p={from_path}", "-f=std", f"-q={to_path}"]
    else:
        # lazy = False
        cmd = [settings.sunhang_conv_exe_path, from_path, to_path, from_fmt, to_fmt]

    return _conv_trace(from_path, to_path, cmd)

def conv_trace_pred(bench_name: str, predictor: str) -> Path:
    from_fmt, to_fmt = get_fmt_pair(bench_name, predictor)

    return conv_trace(bench_name, from_fmt, to_fmt)

def _conv_trace(from_path: Path, to_path: Path, cmd: list[str]) -> Path:
    to_path_zip = to_path.with_suffix(".zip")
    from_path_zip = from_path.with_suffix(".zip")

    # print(from_path)
    # print(to_path)

    if to_path_zip.exists(): # Unzip already existing zipped trace
        unzip_file(to_path_zip)
    elif to_path.exists(): # Zip already existing unzipped trace
        zip_file(to_path, to_path_zip)
    else: # Create the trace file, zip, remove trace file
        # First extract if from_path doesn't exist
        if not from_path.exists():
            unzip_file(from_path_zip)
        elif not from_path_zip.exists():  # Zip if no zip
            zip_file(from_path, from_path_zip)

        # Run the conversion command and zip
        run_cmd(cmd)
        zip_file(to_path, to_path_zip)

        # Keep only the zip file
        os.remove(from_path)
    
    return to_path

def get_cmd(from_path, to_path, from_fmt, to_fmt):
    if from_fmt == "std" and to_fmt == "java_enc":
        return ['java', '-jar', settings.spd_trace_conv_jar_path, f"-p={from_path}", f"-f={from_fmt}", f"-q={to_path}"]
    return[settings.sunhang_conv_exe_path, from_path, to_path, from_fmt, to_fmt]

def get_fmt_pair(bench_name: str, predictor: str) -> tuple[str, str]:
    from_fmt, to_fmt = get_bin_fmt_pair(predictor)
    if from_fmt:
        from_path = settings.trace_bin_dir / from_fmt / f"{bench_name}.data"
        from_path_zipped = settings.trace_bin_dir / from_fmt / f"{bench_name}.zip"
        
        # Fallback on std for from_fmt
        if predictor == settings.sunhang_name and \
           ((not from_path.exists() and not from_path_zipped.exists()) or \
           settings.has_cond_var_suite()):
            return "std", to_fmt
    
    return from_fmt, to_fmt

# Return the "from" and "to" fmts that should be used for the given predictor
# Only binary fmts
def get_bin_fmt_pair(predictor: str) -> tuple[str, str]:
    if predictor == settings.sunhang_name:
        return "java_enc", "local_enc"
    elif predictor == settings.spdoffline_name:
        return "local_enc", "java_enc"
    else:
        print(f"    [ERROR]: Invalid predictor ({predictor}) should be one of {settings.available_predictors}")
        return "", ""

def main():
    parser = optparse.OptionParser()

    parser.add_option("--bs", "--bench_suite", dest="bench_suite", default="cond_var",
                      help="run the script on the given benchmark suite. " \
                            f"Should be one of {settings.available_bench_suites}" \
                            "(Default: cond_var).")
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
    
    # Set benchmark suite
    settings.set_bench_suite(options.bench_suite)
    if settings.bench_suite == "":
        print(f"[ERROR]: Invalid benchmark suite ({options.bench_suite}), should be one of {settings.available_bench_suites}")
        return
    
    print(f"Benchmark suite: {settings.bench_suite}")
    
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
    
    # Convert for each benchmark
    for bench in benchmarks:
        if bench in ignored_bench:
            print(f"Ignoring bench {bench}")
            continue
        print(f"Benchmark: {bench}")

        if predictor:
            trace_path = conv_trace_pred(bench, predictor)
        else:
            trace_path = conv_trace(bench, from_fmt, to_fmt)

        if trace_path is not None:
            os.remove(trace_path)
    
if __name__ == "__main__":
    main()