import optparse
from pathlib import Path
import settings
import numpy as np
from util import run_cmd, get_benchmarks

# Converts trace from std format. Returns the path to the binary trace
# java_enc for SPDOffline, local_enc for SUnhang
def conv_trace(bench_name: str, predictor: str, lazy:bool=True) -> Path:
    std_trace_path = settings.trace_std_dir / (f"{bench_name}.std")
    java_enc_trace_path = settings.trace_bin_java_enc_dir / (bench_name + f".data")

    if predictor == settings.spdoffline_name:
        bin_trace_path = java_enc_trace_path
        cmd = ['java', '-jar', settings.spd_trace_conv_jar_path, f"-p={std_trace_path}", "-f=std", f"-q={bin_trace_path}"]
    else:
        lazy = False
        # Use the binary path if it exists, otherwise use the std one
        bin_trace_path = settings.trace_bin_loc_enc_dir / (bench_name + f".data")
        from_trace_path = java_enc_trace_path
        
        # For cond_var always convert from std as the java bin version
        # Ignores cond var related actions
        if settings.bench_suite in ["cond_var", "generated"] or not from_trace_path.exists():
            from_trace_path = std_trace_path

        cmd = [settings.sunhang_conv_exe_path, from_trace_path, bin_trace_path]

    if not lazy or not bin_trace_path.exists():
        # print(f"Converting Trace for: {predictor}, {bench_name}")
        run_cmd(cmd)
    else:
        ...
        # print(f"Skipping Trace Conversion for: {predictor}, {bench_name}")

    return bin_trace_path

def main():
    parser = optparse.OptionParser()
    parser.add_option("-b", "--benchmarks", dest="benchmarks", default="all",
                      help="converts the trace from std to bin for the specified benchmarks. Default is all")
    parser.add_option("-i", "--ignore", dest="ignored_bench", default="",
                      help="ignores the specified benchmarks")
    
    (options, args) = parser.parse_args()
    if options.benchmarks == "all":
        benchmarks = get_benchmarks()
    else:
        benchmarks = options.benchmarks.split(",")
    
    ignored_bench = options.ignored_bench.split(",")
    print(f"Benchmarks to ignore: {ignored_bench}")

    for bench in benchmarks:
        if bench in ignored_bench:
            print(f"Ignoring bench {bench}")
            continue
        # conv_trace(bench, settings.spdoffline_name)
        conv_trace(bench, settings.sunhang_name, False)
    
if __name__ == "__main__":
    main()