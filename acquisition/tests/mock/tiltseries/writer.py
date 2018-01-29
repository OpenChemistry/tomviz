#
# Simulates an operator writing a series of images to a directory forming
# a tilt series. Used to test passive acquisition by watching a directory.
# It uses a tilt series downloaded from data.kitware.com
# (TiltSeries_NanoParticle_doi_10.1021-nl103400a.tif)
from . import Writer

import argparse
import tomviz


def main():
    parser = argparse.ArgumentParser(
        description='Simulates a tilt series be written to a directory..')
    parser.add_argument('-p', '--path',
                        help='path to write files')
    parser.add_argument('-d', '--delay', help='the delay between writing images',
                        default=1)

    args = parser.parse_args()
    writer = Writre(args.path, args.delay)
    writer.start()
    writer.join()

if __name__ == '__main__':
    main()





