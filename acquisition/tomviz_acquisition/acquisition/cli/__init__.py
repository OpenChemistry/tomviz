import argparse
from tomviz_acquisition.utility import logging


def main():
    parser = argparse.ArgumentParser(
        description='Tomviz acquisition server.')
    parser.add_argument('-a', '--adapter', help='source adapter to install')
    parser.add_argument('-i', '--host', default='localhost',
                        help='on what interface the server should run')
    parser.add_argument('-p', '--port',
                        help='on what port the server should run', default=8080)
    parser.add_argument('-d', '--debug', help='turn on debug mode',
                        action='store_true')
    parser.add_argument('-e', '--dev', help='turn on dev mode',
                        action='store_true')
    parser.add_argument('-r', '--redirect',
                        help='redirect stdout/stderr to log',
                        action='store_true')

    args = parser.parse_args()

    if args.redirect:
        logging.setup_std_loggers()

    from tomviz_acquisition.acquisition import server

    logging.setup_loggers(args.debug, args.redirect)
    server_params = vars(args)
    del server_params['redirect']
    server.start(**server_params)
