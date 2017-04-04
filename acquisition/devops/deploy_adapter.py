import requests
import json
import argparse
import uuid
import pprint


def main():
    parser = argparse.ArgumentParser(
        description='Deploy adapter to Tomviz acquisition server ( the server '
        'must be running with the --dev option.')
    parser.add_argument('-u', '--url', help='the base url to the server',
                        default='http://localhost:8080')
    parser.add_argument('-m', '--module', help='the module name',
                        default='tomviz_dev_adapter')
    parser.add_argument('-a', '--adapter', help='the adapter class name',
                        required=True)
    parser.add_argument('-p', '--path', type=argparse.FileType('r'),
                        help='the path to the adapter source', required=True)

    args = parser.parse_args()

    request = {
        'id': uuid.uuid4().int,
        'jsonrpc': '2.0',
        'method': 'deploy_adapter',
        'params': [args.module, args.adapter, args.path.read()]
    }

    url = '%s/dev/' % args.url.rstrip('/')
    r = requests.post(url, json=request)
    if r.status_code != 200:
        print('JSON response:')
        pp = pprint.PrettyPrinter(indent=2)
        pp.pprint(r.json())
        print('Data:')
        print(json.loads(r.json()['error']['data']))
    r.raise_for_status()


if __name__ == '__main__':
    main()
