#!/usr/bin/env python3
"""
PyXRF Utilities

This script provides utilities for working with PyXRF data including
downloading the data and writing to PyXRF HDF5 files,
CSV log file creation, and projection processing.
"""
import argparse
import sys

from pyxrf_utils.create_log_file import create_log_file
from pyxrf_utils.make_hdf5 import make_hdf5
from pyxrf_utils.process_projections import process_projections


def make_csv_cmd(args):
    """
    Read PyXRF HDF5 files and write a CSV log file.

    Args:
        args: Parsed arguments from argparse
    """
    create_log_file(
        log_file_name=args.output_csv,
        working_directory=args.working_directory,
        sid_selection=args.sid_selection,
        hdf5_glob_pattern=args.hdf5_glob_pattern,
        skip_invalid=args.skip_invalid,
    )


def make_hdf5_cmd(args):
    """
    Download and save scan data in PyXRF HDF5 format.

    Args:
        args: Parsed arguments from argparse
    """
    make_hdf5(
        start_scan=args.start_scan,
        stop_scan=args.end_scan,
        working_directory=args.output_directory,
        successful_scans_only=args.successful_scans_only,
        log_file_name=args.log_file,
    )


def process_projections_cmd(args):
    """
    Process XRF projection data using PyXRF.

    Args:
        args: Parsed arguments from argparse
    """
    process_projections(
        working_directory=args.working_directory,
        parameters_file_name=args.parameters_file,
        log_file_name=args.log_file,
        ic_name=args.ic_name,
        skip_processed=args.skip_processed,
        output_directory=args.output_directory,
    )


def main():
    """Main entry point for the pyxrf-utils command-line tool."""

    # Create the top-level parser
    parser = argparse.ArgumentParser(
        prog='pyxrf-utils',
        description='PyXRF Utilities - Tools for XRF data processing',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  pyxrf-utils make-hdf5 -s 157391 -e 157637 -b -l log.csv /path/to/output/directory
  pyxrf-utils make-csv -w /path/to/working/directory -s "157391:157637" log.csv
  pyxrf-utils process-projections -p params.json -l log.csv -i sclr1_ch4 -s -o /output /working
        """  # noqa
    )

    # Add version argument
    parser.add_argument(
        '--version',
        action='version',
        version='%(prog)s 1.0.0'
    )

    # Create subparsers
    subparsers = parser.add_subparsers(
        title='subcommands',
        description='Available commands',
        dest='command',
        help='Use "pyxrf-utils <command> --help" for more information',
        required=True
    )

    # ========== make-csv subcommand ==========
    parser_csv = subparsers.add_parser(
        'make-csv',
        help='Create log CSV file',
        description='Create log CSV file from PyXRF HDF5 files'
    )
    parser_csv.add_argument(
        'output_csv',
        help='Output CSV file path (e.g., log.csv)'
    )
    parser_csv.add_argument(
        '-w', '--working-directory',
        required=True,
        help='Path to working directory containing input data'
    )
    parser_csv.add_argument(
        '-s', '--sid-selection',
        required=False,
        default=None,
        help='SID selection string (comma-separated scan IDs or pattern)'
    )
    parser_csv.add_argument(
        '-p', '--hdf5-glob-pattern',
        required=False,
        default='scan2D_*.h5',
        help='Glob pattern for identifying which HDF5 files to read.',
    )
    parser_csv.add_argument(
        '-i', '--skip-invalid',
        action='store_true',
        required=False,
        default=False,
        help='Skip invalid scans whose properties could not be read.',
    )
    parser_csv.set_defaults(func=make_csv_cmd)

    # ========== make-hdf5 subcommand ==========
    parser_hdf5 = subparsers.add_parser(
        'make-hdf5',
        help='Download and save scans in PyXRF HDF5 format',
        description='Download and save scans in PyXRF HDF5 format'
    )
    parser_hdf5.add_argument(
        'output_directory',
        help='Path to output directory where HDF5 files will be saved'
    )
    parser_hdf5.add_argument(
        '-s', '--start-scan',
        type=int,
        required=True,
        help='Starting scan number'
    )
    parser_hdf5.add_argument(
        '-e', '--end-scan',
        type=int,
        required=True,
        help='Ending scan number'
    )
    parser_hdf5.add_argument(
        '-b', '--successful-scans-only',
        action='store_true',
        required=False,
        default=False,
        help='Process only successful scans'
    )
    parser_hdf5.add_argument(
        '-l', '--log-file',
        required=True,
        help='Log file name for recording conversion process'
    )
    parser_hdf5.set_defaults(func=make_hdf5_cmd)

    # ========== process-projections subcommand ==========
    parser_proj = subparsers.add_parser(
        'process-projections',
        help='Process XRF projection data',
        description='Process XRF projection data using PyXRF'
    )
    parser_proj.add_argument(
        'working_directory',
        help='Path to working directory containing projection data'
    )
    parser_proj.add_argument(
        '-p', '--parameters-file',
        required=True,
        help='Path to parameters file (e.g., JSON or config file)'
    )
    parser_proj.add_argument(
        '-l', '--log-file',
        required=True,
        help='Path to log file for recording processing steps'
    )
    parser_proj.add_argument(
        '-i', '--ic-name',
        required=True,
        help='Ion chamber name for normalization'
    )
    parser_proj.add_argument(
        '-s', '--skip-processed',
        required=False,
        default=False,
        action='store_true',
        help='Skip already processed data'
    )
    parser_proj.add_argument(
        '-o', '--output-directory',
        required=True,
        help='Path to output directory for reconstructed data'
    )
    parser_proj.set_defaults(func=process_projections_cmd)

    # Parse arguments
    args = parser.parse_args()

    # Call the appropriate function
    try:
        args.func(args)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
