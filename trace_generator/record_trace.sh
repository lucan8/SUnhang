#!/usr/bin/bash

set -o xtrace

trace_gen_path="/app/trace_generator"

javac -cp /app/RoadRunner/classes -proc:none $trace_gen_path/StdInstrumentor.java -d $trace_gen_path/classes/

trace_dir_root="/app/traces"
bench_dir_root="/app/benchmarks"
bench_suite="vendor/JaConTeBe"

trace_dir_out="$trace_dir_root/$bench_suite"
trace_path_out="$trace_dir_out/$test_name.std"
mkdir -p $trace_dir_out

cd $bench_dir_root/$bench_suite

runtime_dependencies="./versions.alt/lib/${test_name}.jar:./source"

Opt="-quiet -noxml -noTidGC"

rrrun ${Opt} \
  -classpath=${runtime_dependencies} \
  -toolpath=${trace_gen_path}/classes \
  -tool=StdInstrumentor \
  -classes="-org.junit..*,-junit..*,default=ACCEPT" \
  ${class_to_run} | grep --line-buffered "|" > $trace_path_out

cd /app