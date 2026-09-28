#!/bin/sh
# Run the batch-processing example end to end: make the inputs and the
# pipeline, then process every input with one tomviz-pipeline call.
set -e
cd "$(dirname "$0")"

python make_example.py example
tomviz-pipeline -s example/particles.tvsm -o example/results \
    --input 'example/inputs/*.emd'

echo
echo "Results:"
find example/results -type f | sort
