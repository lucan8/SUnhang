from pathlib import Path
import os

root_dir = Path(os.path.dirname(os.path.dirname(__file__)))
bench_suite = "cond_var"
bench_dir = root_dir / "benchmarks" / bench_suite

out_files_base = bench_dir / "output"

# Trace stuff
trace_dir = bench_dir / "traces"
# trace_meta_dir = trace_dir / "meta"
trace_std_dir = trace_dir / "std"
trace_bin_dir = trace_dir / "bin"
trace_bin_loc_enc_dir = trace_bin_dir / "local_enc"
trace_bin_java_enc_dir = trace_bin_dir / "java_enc"
os.makedirs(trace_bin_loc_enc_dir, exist_ok=True)
os.makedirs(trace_bin_java_enc_dir, exist_ok=True)

# Predictor names
sunhang_pred_extra_title = "-1-lvl-locks-as-deps"
sunhang_base_name = "SUnhang"

spdoffline_name, sunhang_name = "spdoffline", sunhang_base_name + sunhang_pred_extra_title
predictors = [sunhang_name, spdoffline_name]

# Executables
spdoffline_dir = root_dir / "vendor" / spdoffline_name
bin_dir = root_dir / "bin"

sunhang_exe_path = bin_dir / sunhang_base_name
sunhang_conv_exe_path = bin_dir / "conv_trace"

spdoffline_jar_path = spdoffline_dir / "fat_spdoffline1.jar"
spd_trace_conv_jar_path = spdoffline_dir / "fat_convert.jar"
