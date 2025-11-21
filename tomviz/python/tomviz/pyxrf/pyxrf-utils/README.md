# PyXRF Utilities

Simple utility functions for interacting with PyXRF.

For example, make a set of PyXRF HDF5 files in a directory:

```bash
pyxrf-utils make-hdf5 -s 157391 -e 157637 -b -l log.csv /path/to/output/directory
```

Make a CSV log file from the HDF5 files:

```bash
pyxrf-utils make-csv -w /path/to/working/directory -s "157391:157637" log.csv
```

Process projections using a PyXRF params.json file:

```bash
pyxrf-utils process-projections -p params.json -l log.csv -i sclr1_ch4 -s -o /output /input/dir
```
