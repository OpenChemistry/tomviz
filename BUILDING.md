Building Tomviz
===============

Building Tomviz is an advanced process that requires compiling ParaView,
as well as ensuring all necessary dependencies are in place. Most users
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

First, we need to ensure that ParaView and tomviz are cloned into the
current repository. Run the following bash code to do so:

```bash
if ! [ -e "paraview" ]; then
  git clone -b v6.0.1 --recursive https://gitlab.kitware.com/paraview/paraview
fi

if ! [ -e "tomviz" ]; then
  git clone --recursive https://github.com/NSLS2/tomviz
fi
```

The GitHub Actions CI maintains a list of required dependencies from
conda-forge. Thus we will use those lists for installation.

```bash
conda install -y --override-channels -c conda-forge --file tomviz/.github/workflows/build_requirements.txt
```

Next, build ParaView. This may take some time.

```bash
bash tomviz/.github/workflows/scripts/build_paraview.sh
```

Now, build Tomviz.

```bash
bash tomviz/.github/workflows/scripts/build_tomviz.sh
```

You can now install all runtime dependencies, as well as the
Tomviz python library itself.

```bash
conda install -y --override-channels -c conda-forge --file tomviz/.github/workflows/runtime_requirements.txt
pip install --no-build-isolation --no-deps -U tomviz/tomviz/python
```

Tomviz may then be ran as follows:

```bash
./tomviz-build/bin/tomviz
```

All of the build instructions above put into a single script are as follows:

```bash
if ! [ -e "paraview" ]; then
  git clone -b v6.0.1 --recursive https://gitlab.kitware.com/paraview/paraview
fi

if ! [ -e "tomviz" ]; then
  git clone --recursive https://github.com/NSLS2/tomviz
fi

# Install build requirements
conda install -y --override-channels -c conda-forge --file tomviz/.github/workflows/build_requirements.txt

# Build ParaView and Tomviz
bash tomviz/.github/workflows/scripts/build_paraview.sh
bash tomviz/.github/workflows/scripts/build_tomviz.sh

# Install runtime dependencies
conda install -y --override-channels -c conda-forge --file tomviz/.github/workflows/runtime_requirements.txt
pip install --no-build-isolation --no-deps -U tomviz/tomviz/python
```
