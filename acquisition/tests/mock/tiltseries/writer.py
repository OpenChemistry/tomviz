#
# Simulates an operator writing a series of images to a directory forming
# a tilt series. Used to test passive acquisition by watching a directory.
# It uses a tilt series downloaded from data.kitware.com

from . import TIFFWriter, DM3Writer

import argparse

_writer_map = {
    'tiff': TIFFWriter,
    'dm3': DM3Writer
}


def main():
    parser = argparse.ArgumentParser(
        description='Simulates a tilt series be written to a directory..')
    parser.add_argument('-p', '--path',
                        help='path to write files', required=True)
    parser.add_argument('-d', '--delay',
                        help='the delay between writing images', default=1)

    parser.add_argument('-t', '--type', help='the type of images to use',
                        choices=['tiff', 'dm3'], default='tiff')

    args = parser.parse_args()
    writer = _writer_map[args.type](args.path, args.delay)
    writer.start()
    writer.join()


if __name__ == '__main__':
    main()
