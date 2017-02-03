import argparse
from tomviz.acquisition import server


def main():
    parser = argparse.ArgumentParser(
        description='Tomviz acquisition server.')
    parser.add_argument("-a", "--adapter", help="source adapter to install")
    parser.add_argument("-i", "--interface",
                        help="on what interface the server should run")
    parser.add_argument("-p", "--port",
                        help="on what port the server should run")
    args = parser.parse_args()

    if args.port:
        server.port = args.port
    if args.interface:
        server.host = args.interface
    if args.adapter:
        server.adapter = args.adapter

    server.start()


if __name__ == '__main__':
    main()
