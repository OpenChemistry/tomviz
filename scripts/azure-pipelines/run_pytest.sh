#!/bin/bash

if [[ $AGENT_OS == 'Windows_NT' ]]; then
  # Windows just has it labelled "python"
  PYTHON_EXE=python
else
  # On mac, "python" is python2
  PYTHON_EXE=python3
fi

cd acquisition/tests
$PYTHON_EXE -m pytest -s -k "not fei"
