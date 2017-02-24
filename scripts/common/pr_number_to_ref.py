#!/usr/bin/env python

import sys
import requests

url = 'https://api.github.com/repos/OpenChemistry/tomviz/pulls/%s' % sys.argv[1]
request = requests.get(url)
request.raise_for_status()
print(request.json()['head']['ref'])
