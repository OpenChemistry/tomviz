import argparse
import tomviz
from tomviz.acquisition import server


def main():
    parser = argparse.ArgumentParser(
        description='Tomviz acquisition server.')
    parser.add_argument("-a", "--adapter", help="source adapter to install")
    parser.add_argument("-i", "--interface",
                        help="on what interface the server should run")
    parser.add_argument("-p", "--port",
                        help="on what port the server should run")
    parser.add_argument("-d", "--debug", help="turn on debug mode",
                        action='store_true')

    args = parser.parse_args()

    if args.port:
        server.port = args.port
    if args.interface:
        server.host = args.interface
    if args.adapter:
        server.adapter = args.adapter
    debug = False
    if args.debug:
        debug = args.debug

    tomviz.setupLogger(debug)
    server.start(debug)


if __name__ == '__main__':
    main()
