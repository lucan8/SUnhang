#!/bin/bash
set -e

echo "RR Setup"
echo "================================================"
echo
echo "JAVA_HOME   =  " $JAVA_HOME
echo "JVM_ARGS    =  " $JVM_ARGS
echo "rrrun       -> " `which rrrun`
echo "RR_HOME     =  " $RR_HOME
echo "RR_TOOLPATH =  " $RR_TOOLPATH
echo
echo "================================================"

echo "Available Processors"
echo "================================================"
echo ""

cd $RR_HOME

javac test/Proc.java
AVAIL_PROCS=$(java test/Proc)
export AVAIL_PROCS
echo "export AVAIL_PROCS=${AVAIL_PROCS}" > /etc/dynamic_env.sh

echo "Runtime.getRuntime().availableProcessors() returned ${AVAIL_PROCS}"
echo ""
echo "================================================"

cd /app
exec "$@"