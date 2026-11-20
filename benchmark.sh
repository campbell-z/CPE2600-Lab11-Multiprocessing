#!/bin/bash
PROCESSES=(1 2 5 10 20 50)
THREADS=(1 2 5 10 20 50)
RUNS=5

echo "Processes,Run,Runtime" > benchmark_results.csv

# Temporary file for GNU time output
timefile=$(mktemp)
trap 'rm -f "$timefile"' EXIT

for processes in "${PROCESSES[@]}"
do

    for threads in "${THREADS[@]}"
    do
        for run in $(seq 1 "$RUNS")
        do

            echo "Testing $processes processes, $threads threads, run $run..."

            if /usr/bin/time -f "%e" -o "$timefile" ./mandelmovie -p "$processes" -t "$threads" > /dev/null
            then
                runtime=$(cat "$timefile")
                status="SUCCESS"
            else
                exit_code=$?
                runtime=$(cat "$timefile")
                status="FAILED ($exit_code)"
            fi

	    echo "$processes,$run,$runtime" >> benchmark_results.csv
        done
    done
done

echo ""
echo "Benchmarking complete"
echo "Results saved to benchmark_results.csv"
