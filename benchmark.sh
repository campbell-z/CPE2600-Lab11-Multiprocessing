#!/bin/bash

echo "Processes,Run,Runtime" > benchmark_results.csv

for processes in 1 2 5 10 20
do

    for run in 1 2 3 4 5
    do

        echo "Testing $processes processes, run $run..."

        runtime=$(/usr/bin/time -f "%e" ./mandelmovie -p "$processes" 2>&1 >/dev/null)

	echo "$processes,$run,$runtime" >> benchmark_results.csv
    done
done

echo "Benchmarking complete"
