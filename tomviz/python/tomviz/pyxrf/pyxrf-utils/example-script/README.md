This is an example `run-pyxrf-utils` script which could be used
to execute pyxrf-utils with a `pixi run` command.

It either needs to be located somewhere in the user's $PATH, or
an absolute path to this file can be specified.

Note this example file needs to be modified to point the
pyxrf-utils path to a real path.

It runs `pixi run --as-is`, so it will never re-solve the manifest or
install packages. The environment must already have been built with
`pixi install`; see MAINTENANCE.md in the parent directory.
