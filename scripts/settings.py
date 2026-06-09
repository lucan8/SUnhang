from pathlib import Path
import os

class Settings:
    root_dir = Path(os.path.dirname(os.path.dirname(__file__)))
    available_bench_suites = ["cond_var", "original", "generated"]

    # Creates the settings object based on the chosen benchmarks suite
    # If an invalid benchmark is passed, self.bench_name is "" and the assoicated structures are badly defined
    def __init__(self, bench_suite: str):
        self.set_bench_suite(bench_suite)
        self._set_predictors()

    # Sets the benchmarks suite and all associated settings(mostly paths)
    # If an invalid benchmark is passed, self.bench_name is "" and the assoicated structures are badly defined
    def set_bench_suite(self, bench_suite: str):
        if bench_suite not in self.available_bench_suites:
            self.bench_suite = ""
            return
        
        self.bench_suite = bench_suite
        self.bench_dir = Settings.root_dir / "benchmarks" / bench_suite
        
        self._set_output_paths()
        self._set_trace_paths()

    # Just sets to default values for now
    # It also sets the paths to their executables
    def _set_predictors(self):
        # Predictor names
        self.sunhang_pred_extra_title = ""
        self.sunhang_base_name = "SUnhang"

        self.spdoffline_name, self.sunhang_name = "SPDOffline", self.sunhang_base_name + self.sunhang_pred_extra_title
        self.predictors = [self.sunhang_name, self.spdoffline_name]

        # Executables
        self.spdoffline_dir = Settings.root_dir / "vendor" / self.spdoffline_name
        self.bin_dir = Settings.root_dir / "bin"

        self.sunhang_exe_path = self.bin_dir / self.sunhang_base_name
        self.sunhang_conv_exe_path = self.bin_dir / "conv_trace"

        self.spdoffline_jar_path = self.spdoffline_dir / "fat_spdoffline1.jar"
        self.spd_trace_conv_jar_path = self.spdoffline_dir / "fat_convert.jar"

    def _set_output_paths(self):
        self.out_files_base = self.bench_dir / "output"
        self.table_out_dir = self.bench_dir / "tables"
        self.table_out_path = self.table_out_dir / "main.tex"
        self.table_df_out_path = self.table_out_dir / "table.df"
    
    def _set_trace_paths(self):
        self.trace_dir = self.bench_dir / "traces"
        self.trace_std_dir = self.trace_dir / "std"
        self.trace_bin_dir = self.trace_dir / "bin"
        self.trace_bin_loc_enc_dir = self.trace_bin_dir / "local_enc"
        self.trace_bin_java_enc_dir = self.trace_bin_dir / "java_enc"
        os.makedirs(self.trace_bin_loc_enc_dir, exist_ok=True)
        os.makedirs(self.trace_bin_java_enc_dir, exist_ok=True)

settings = Settings("conv_var")
