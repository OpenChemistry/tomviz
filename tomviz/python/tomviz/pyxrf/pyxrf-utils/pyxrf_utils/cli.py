#!/usr/bin/env python3
"""PyXRF Utilities — tools for downloading and processing XRF scan data."""
from __future__ import annotations

import argparse
import json
import sys

from pyxrf_utils.process_projections import process_projections
from pyxrf_utils.scan_range import expand_scan_range


def make_hdf5_cmd(args: argparse.Namespace) -> None:
    # Downloading needs the beamline stack (hxntools -> databroker ->
    # pims). Import it here so process-projections works without it.
    from pyxrf_utils.make_hdf5 import make_hdf5

    skip_ids = json.loads(args.skip) if args.skip else []
    scan_ids = expand_scan_range(args.range, skip_ids)
    if not scan_ids:
        print('No scan IDs to download after applying range and skip list.',
              flush=True)
        return

    make_hdf5(
        scan_ids=scan_ids,
        working_directory=args.output_directory,
        force=args.force,
    )


def process_projections_cmd(args: argparse.Namespace) -> None:
    skip_ids = json.loads(args.skip) if args.skip else []
    scan_ids = expand_scan_range(args.range, skip_ids)

    process_projections(
        scan_ids=scan_ids,
        working_directory=args.working_directory,
        parameters_file_name=args.parameters_file,
        ic_name=args.ic_name,
        output_directory=args.output_directory,
        skip_processed=args.skip_processed,
        csv_output=args.csv_output or '',
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        prog='pyxrf-utils',
        description='PyXRF Utilities — download and process XRF scan data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  pyxrf-utils make-hdf5 /path/to/data --range 157391:157637
  pyxrf-utils make-hdf5 /path/to/data --range 157391:157637:2 --skip '[157400,157410]'
  pyxrf-utils process-projections /working --range 157391:157637 -p params.json -i sclr1_ch4 -o /output
        """,
    )
    parser.add_argument('--version', action='version', version='%(prog)s 2.0.0')

    subparsers = parser.add_subparsers(
        title='subcommands',
        dest='command',
        required=True,
    )

    # ========== make-hdf5 ==========
    p_hdf5 = subparsers.add_parser(
        'make-hdf5',
        help='Download scan data as PyXRF HDF5 files',
    )
    p_hdf5.add_argument(
        'output_directory',
        help='Directory where HDF5 files will be saved',
    )
    p_hdf5.add_argument(
        '--range', required=True,
        help='Scan range (inclusive stop): start:stop or start:stop:stride',
    )
    p_hdf5.add_argument(
        '--skip', default='',
        help='JSON list of scan IDs to skip, e.g. \'[100,105]\'',
    )
    p_hdf5.add_argument(
        '--force', action='store_true', default=False,
        help='Re-download even if HDF5 file already exists',
    )
    p_hdf5.set_defaults(func=make_hdf5_cmd)

    # ========== process-projections ==========
    p_proc = subparsers.add_parser(
        'process-projections',
        help='Process XRF projections and assemble tomo.h5',
    )
    p_proc.add_argument(
        'working_directory',
        help='Directory containing scan HDF5 files',
    )
    p_proc.add_argument(
        '--range', required=True,
        help='Scan range (inclusive stop): start:stop or start:stop:stride',
    )
    p_proc.add_argument(
        '--skip', default='',
        help='JSON list of scan IDs to skip, e.g. \'[100,105]\'',
    )
    p_proc.add_argument(
        '-p', '--parameters-file', required=True,
        help='Path to PyXRF parameters file (.json)',
    )
    p_proc.add_argument(
        '-i', '--ic-name', required=True,
        help='Ion chamber name for normalization',
    )
    p_proc.add_argument(
        '-o', '--output-directory', required=True,
        help='Directory for tomo.h5 output',
    )
    p_proc.add_argument(
        '-s', '--skip-processed', action='store_true', default=False,
        help='Skip scans that have already been processed',
    )
    p_proc.add_argument(
        '--csv-output', default='',
        help='Optional path to save the generated CSV log file',
    )
    p_proc.set_defaults(func=process_projections_cmd)

    args = parser.parse_args()
    try:
        args.func(args)
    except Exception as e:
        print(f'Error: {e}', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
