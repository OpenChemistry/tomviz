#!/usr/bin/env python
"""Build the inputs and the pipeline for the batch-processing example.

    python make_example.py [DIRECTORY] [--count N] [--size S]

writes DIRECTORY/inputs/particles_00.emd ... particles_0N.emd, each a
noisy volume with a different number of spheres in it, and
DIRECTORY/particles.tvsm, a pipeline that blurs, thresholds and labels
the spheres. The pipeline is assembled from the operator scripts shipped
in tomviz/python, so it is the same thing saving it from the GUI gives
you, and the example never drifts from the operators.

Needs the tomviz-pipeline package (pip install tomviz-pipeline).
"""
import argparse
import json
from pathlib import Path

import numpy as np

from tomviz_pipeline import Dataset
from tomviz_pipeline.writers.emd import write_emd

OPERATORS = Path(__file__).resolve().parents[2] / 'tomviz' / 'python'


def make_volume(index, size, rng):
    """A volume with ``2 + index`` non-overlapping spheres in noise.

    The spheres sit on a jittered 2 x 2 x 2 lattice, so they never touch
    and Connected Components finds exactly as many as were placed."""
    spheres = 2 + index
    volume = rng.normal(0.0, 0.15, (size, size, size)).astype(np.float32)
    lattice = [(i, j, k) for i in (0.25, 0.75) for j in (0.25, 0.75)
               for k in (0.25, 0.75)]
    rng.shuffle(lattice)
    x, y, z = np.mgrid[:size, :size, :size]
    for site in lattice[:spheres]:
        center = np.array(site) * size + rng.uniform(-size / 16, size / 16, 3)
        radius = rng.uniform(size / 12, size / 8)
        inside = ((x - center[0])**2 + (y - center[1])**2 +
                  (z - center[2])**2) <= radius**2
        volume[inside] = 1.0
    dataset = Dataset({'Scalars': np.asfortranarray(volume)}, 'Scalars')
    dataset.spacing = (1.0, 1.0, 1.0)
    return dataset, spheres


def operator_node(node_id, name, label, **arguments):
    """A Python operator node the way a state file stores it: the
    description and the script travel inside the file."""
    return {
        'id': node_id,
        'type': 'transform.legacyPython',
        'label': label,
        'description': (OPERATORS / f'{name}.json').read_text(),
        'script': (OPERATORS / f'{name}.py').read_text(),
        'arguments': arguments,
    }


def make_state(first_input):
    nodes = [
        {'id': 1, 'type': 'source.reader', 'label': 'Reader',
         'fileNames': [str(first_input)]},
        operator_node(2, 'GaussianFilter', 'Gaussian Blur', sigma=1.0),
        operator_node(3, 'BinaryThreshold', 'Binary Threshold',
                      lower_threshold=0.5, upper_threshold=10.0),
        operator_node(4, 'ConnectedComponents', 'Connected Components',
                      background_value=0, connectivity=3, min_size=10),
    ]
    # Binary Threshold delivers its label map on a port named after its
    # child dataset, which is where Connected Components reads from.
    links = [
        {'from': {'node': 1, 'port': 'volume'},
         'to': {'node': 2, 'port': 'volume'}},
        {'from': {'node': 2, 'port': 'volume'},
         'to': {'node': 3, 'port': 'volume'}},
        {'from': {'node': 3, 'port': 'thresholded_segmentation'},
         'to': {'node': 4, 'port': 'volume'}},
    ]
    return {
        'schemaVersion': 2,
        'pipeline': {'nextNodeId': 5, 'nodes': nodes, 'links': links},
    }


def build(directory, count=4, size=64, seed=0):
    """Write the inputs and the state file. Returns the state file path,
    the input paths and how many spheres each input holds."""
    directory = Path(directory)
    inputs_dir = directory / 'inputs'
    inputs_dir.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(seed)

    inputs, spheres = [], []
    for index in range(count):
        dataset, n = make_volume(index, size, rng)
        path = inputs_dir / f'particles_{index:02d}.emd'
        write_emd(dataset, path)
        inputs.append(path)
        spheres.append(n)

    state_path = directory / 'particles.tvsm'
    state_path.write_text(json.dumps(make_state(inputs[0]), indent=2))
    return state_path, inputs, spheres


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('directory', nargs='?', default='example')
    parser.add_argument('--count', type=int, default=4,
                        help='how many input volumes to write')
    parser.add_argument('--size', type=int, default=64,
                        help='edge length of each volume in voxels')
    args = parser.parse_args()

    state_path, inputs, spheres = build(args.directory, args.count,
                                        args.size)
    for path, n in zip(inputs, spheres):
        print(f'{path}: {n} spheres')
    print(f'{state_path}: Gaussian Blur -> Binary Threshold -> '
          'Connected Components')


if __name__ == '__main__':
    main()
