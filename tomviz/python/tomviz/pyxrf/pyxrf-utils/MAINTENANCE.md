# Maintaining the pyxrf-utils environment

This document describes how the prebuilt `pyxrf-utils` environment is
built, validated, deployed, and discovered by Tomviz. It is aimed at
whoever maintains a beamline deployment (e.g. the shared environment at
NSLS-II), not at end users.

Tomviz never imports the PyXRF workflow dependencies (pyxrf, tomopy,
xraylib, hxntools, h5py used for processing, etc.) into its own Python
process. It invokes `pyxrf-utils` as a command-line tool in a
subprocess, and all of those dependencies live in the pixi-managed
environment described here. Keeping the two environments separate is
deliberate: the compiled packages in this environment only need to be
consistent with each other, not with anything inside Tomviz.

## Where the source configuration lives

The `pyxrf-utils` project is versioned inside the Tomviz repository at:

    tomviz/python/tomviz/pyxrf/pyxrf-utils/

The relevant files are:

- `pixi.toml` - the environment definition (conda-forge and PyPI
  dependencies, tasks). This is the single source of truth for what the
  environment contains.
- `pixi.lock` - the fully resolved, reproducible set of package
  versions. Committed to the repository so that a rebuild produces the
  same environment.
- `pyproject.toml` and `pyxrf_utils/` - the CLI package itself
  (`make-hdf5`, `make-csv`, `process-projections` subcommands).
- `example-script/run-pyxrf-utils` - a template wrapper script used to
  launch the CLI through `pixi run` from a deployed location.

## How to rebuild the environment

1. Install [pixi](https://pixi.sh) if it is not already available.
2. Copy or check out this directory to the deployment location
   (anywhere outside a user home directory is fine; the environment is
   self-contained).
3. From that directory, run:

       pixi install

   This creates `.pixi/envs/default/` using the exact versions pinned
   in `pixi.lock`.

To *update* dependencies (rather than reproduce the pinned ones), run
`pixi update`, verify the result (next section), and commit the
regenerated `pixi.lock` back to the Tomviz repository.

For a shared multi-user deployment, make the environment world-readable
after building, e.g. `chmod -R a+rX .pixi`, and build it with a umask
that keeps group access intact (e.g. `umask 002`).

## How to validate a rebuilt environment

Run the CLI's help through the environment:

    pixi run --manifest-path /path/to/pyxrf-utils/pixi.toml pyxrf-utils --help

Then confirm the compiled scientific stack imports cleanly and is
internally consistent:

    pixi run --manifest-path /path/to/pyxrf-utils/pixi.toml \
        python -c "import h5py, tomopy, xraylib, pyxrf, hxntools, xrf_tomo; \
                   print('h5py', h5py.__version__, 'HDF5', h5py.version.hdf5_version)"

If h5py imports without an HDF5 version warning and all of the above
packages import, the environment is good. For a fuller check, run
`pyxrf-utils make-hdf5` against a known scan range and compare with a
previous output.

## How to deploy the path known by Tomviz

Tomviz resolves the `pyxrf-utils` command in this order (see
`findPyxrfUtilsCommand` in `tomviz/PyXRFWidget.cxx`):

1. The command saved in the PyXRF dialog (persisted in the pipeline
   state as `pyxrf_utils_command`). This may be a bare command name on
   `$PATH` or an absolute path.
2. `run-pyxrf-utils`, then `pyxrf-utils`, found on `$PATH`.
3. A hard-coded NSLS-II fallback path, the HXN shared deployment at
   `/nsls2/data/hxn/legacy/users/data_analysis/tomviz_utils/bin/run-pyxrf-utils`.

The recommended deployment is the wrapper script:

1. Copy `example-script/run-pyxrf-utils` to a directory on the users'
   `$PATH` (or any stable absolute path). At HXN, that path is the
   one hard-coded above, so no user configuration is needed.
2. Edit its `PIXI_MANIFEST` variable to point at the deployed
   `pixi.toml` from the previous sections. Add `umask 002` near the
   top as well: an env update run under a restrictive umask has
   previously left `conda-meta/*.json` unreadable to other users.
3. Make it executable (`chmod a+rx run-pyxrf-utils`).

The wrapper deliberately runs `pixi run --as-is`, which is `--frozen`
plus `--no-install`: it never re-solves the manifest and never installs
anything. Without it, a `pixi.toml` that no longer matches `pixi.lock`
would send whoever runs it next into a full dependency solve, and a
missing or incomplete environment into a multi-gigabyte download, in
both cases from inside the Tomviz GUI with no visible progress. With
it, those situations fail immediately instead. The tradeoff is that the
environment must already have been built with `pixi install` before the
wrapper is used, so run that after any change to `pixi.toml` or
`pixi.lock`.

Note that `--as-is` does not fail on its own when the environment is
absent; pixi falls back to running the task with the system `python`,
which then dies with a confusing `ModuleNotFoundError`. That is why the
wrapper checks for the environment's interpreter itself and exits with
a message pointing at `pixi install`.

Do not adopt `--as-is` expecting it to be faster. It skips the
manifest and environment consistency checks, but those cost roughly
15 ms on a warm cache, against a CLI startup of about 1.5 s and jobs
that run for minutes. The reason to use it is the failure mode, not
the latency.

Because the wrapper goes through `pixi run`, the CLI always executes
with the environment's own Python and libraries, regardless of the
caller's environment. Users' `~/.local` site-packages are not involved
on either side: the Tomviz GUI excludes them at startup, and the
wrapper uses the pixi environment's interpreter.

To update an existing deployment after a rebuild, nothing in Tomviz
needs to change as long as the wrapper path stays the same.

## How to confirm Tomviz is using the intended executable

- In the Tomviz PyXRF dialog, check the pyxrf-utils command field; that
  exact string is what gets executed.
- If it is a bare name, run `which run-pyxrf-utils` (or `pyxrf-utils`)
  in the same shell environment Tomviz was launched from to see which
  file wins on `$PATH`.
- The wrapper invokes `pixi run -v`, so the manifest path it resolved
  appears in the process output; check the working directory's
  `pyxrf-utils` log output when a job runs.
- As a definitive check, add `echo "using manifest: $PIXI_MANIFEST" >&2`
  to the wrapper temporarily and trigger a small `make-hdf5` job from
  Tomviz.
