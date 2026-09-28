# Batch processing example

One pipeline, many datasets, no GUI. This directory builds a small
reproducible case for the `tomviz-pipeline` command line tool:

 * `make_example.py` writes four volumes with a different number of
   spheres in each (`example/inputs/particles_0N.emd`) and a pipeline
   `example/particles.tvsm` that blurs, thresholds and labels the spheres.
   The pipeline is assembled from the operator scripts in `tomviz/python`,
   the same thing you get by saving it from the tomviz GUI.
 * `run.sh` runs that pipeline over every input in one call.

```sh
pip install "tomviz-pipeline>=3.1.7"   # 3.1.7 added the label map writer
./run.sh
```

`tomviz-pipeline` runs the state file once per matched input and writes
each run's leaf outputs (here, the label map from Connected Components)
to its own directory:

```
example/results/
  run_0/4_Connected_Components__volume.emd
  run_0/state.tvsm
  run_1/4_Connected_Components__volume.emd
  run_1/state.tvsm
  run_2/...
  run_3/...
```

Each `state.tvsm` is the pipeline as it ran, with that run's input pinned;
it can be opened in tomviz or passed back to `tomviz-pipeline -s`.

Open a label map in tomviz, or read it from Python:

```python
from tomviz_pipeline.io import load_dataset

labels = load_dataset('example/results/run_2/4_Connected_Components__volume.emd')
print(labels.active_scalars.max(), 'spheres')   # 4
```

To apply your own pipeline the same way, save it from the GUI
(`File > Save State`) and point `tomviz-pipeline -s` at it; the `--input`
override works with any state file whose only source is a file reader.
The full set of options, including multi-source pipelines and running in
an external Python environment, is in the
[External Pipelines](https://tomviz.readthedocs.io/en/latest/pipelines.html)
documentation.
