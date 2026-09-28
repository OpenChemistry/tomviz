Building Tomviz
===============

Building Tomviz can be an advanced process that requires
ensuring that all necessary dependencies are in place. Most users
ought to install Tomviz from the conda-forge packages rather than
building it. These instructions are primarily for developers who wish
to make contributions.

We install most build dependencies from conda-forge. So it is highly
recommended that you make a new conda environment for tomviz, and
activating it, like so:

```bash
conda create -n tomviz -y
conda activate tomviz
```

First, we need to ensure that tomviz is cloned into the current repository.
Run the following bash code to do so:

```bash
if ! [ -e "tomviz" ]; then
  git clone --recursive https://github.com/NSLS2/tomviz
fi
```

The GitHub Actions CI maintains a list of required dependencies from
conda-forge. Thus we will use those lists for installation.

```bash
conda install -y --override-channels -c conda-forge --file tomviz/.github/workflows/build_requirements.txt
```

Now, build Tomviz.

```bash
bash tomviz/.github/workflows/scripts/build_tomviz.sh
```

You can now install all runtime dependencies, as well as the
Tomviz python library itself.

```bash
conda install -y --override-channels -c conda-forge --file tomviz/.github/workflows/runtime_requirements.txt
pip install --no-deps -U "$(python tomviz/tomviz/python/tomviz/_pipeline_requirement.py)"
pip install --no-build-isolation --no-deps -U tomviz/tomviz/python
```

The pipeline engine is the separately released
[tomviz-pipeline](https://pypi.org/project/tomviz-pipeline/) package;
`tomviz/python` is the application-side layer on top of it. The
compatible tomviz-pipeline range is defined once, in
`tomviz/python/tomviz/_pipeline_requirement.py`, which prints it when
run as a script — hence the `$(python ...)` above. Both installs use
`--no-deps` so pip never replaces the conda-managed packages. If your
environment predates that split it still has the old in-tree package
installed under the same name and the two will shadow each other — run
`pip uninstall tomviz-pipeline tomviz-app` and delete
`tomviz/tomviz/python/build/` before the two `pip install` lines above.

Tomviz may then be ran as follows:

```bash
./tomviz-build/bin/tomviz
```

To run the test suite (`ctest` in the build directory), also install the
test requirements:

```bash
conda install -y --override-channels -c conda-forge --file tomviz/tests/python/requirements-dev.txt
```

All of the build instructions above put into a single script are as follows:

```bash
if ! [ -e "tomviz" ]; then
  git clone --recursive https://github.com/NSLS2/tomviz
fi

# Install build requirements
conda install -y --override-channels -c conda-forge --file tomviz/.github/workflows/build_requirements.txt

# Build Tomviz
bash tomviz/.github/workflows/scripts/build_tomviz.sh

# Install runtime dependencies
conda install -y --override-channels -c conda-forge --file tomviz/.github/workflows/runtime_requirements.txt
pip install --no-deps -U "$(python tomviz/tomviz/python/tomviz/_pipeline_requirement.py)"
pip install --no-build-isolation --no-deps -U tomviz/tomviz/python
```
