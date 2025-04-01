#!/bin/bash

# run abc for each file in list
# bash run_abc.sh <path_abc> <path_benchmark> <timeout> <path_lib>
# eg: bash run_abc.sh /home/liujunfeng/NewCriterion/abc_criterion/abc_criterion/build/abc  /home/liujunfeng/benchmarks/larger_aig/sim-LSV_exp/benchmarks_dc2syn2/ 1000m

 
####################################################################
binary=$(echo "$1" | awk -F "/" '{print $NF}')
dataname=$(basename "${2%/}")
timestamp=$(date +%Y%m%d%H%M%S)
  
log="${binary}_${dataname}_${libname}_${timestamp}.log"
#touch "$csv"
touch "$log"
#echo "name, command, input, output, lat, gates, edge, area, delay, lev, stime_gates, stime_gates%, stime_cap(ff), stime_cap%, stime_Area, stime_Area%, stime_Delay(ps), stime_Delay%, cut_time, delay_time, total_time" >> $csv

files=$(find "$2" -name "*.aig")
# files=("/home/liujunfeng/benchmarks/mtm/sixteen.aig")

for element in ${files[@]}
do
    echo "process $element"
    command="read $element;  strash; print_stats; rewrite -v; print_stats;"
    outputs=$(timeout $3 $1 -c "$command";)
    echo $outputs >> $log 
done
 
