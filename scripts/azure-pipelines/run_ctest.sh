#!/bin/bash

if [[ $AGENT_OS == 'Darwin' ]]; then
  # Make sure we use python3 on the mac
  export TOMVIZ_TEST_PYTHON_EXECUTABLE=$(which python3)
elif [[ $AGENT_OS == 'Windows_NT' ]]; then
  # The azure-pipelines docker on Windows can't run Linux images
  # If we find a way to get it to work, we can include this test
  CTEST_EXTRA_ARGS='-E DockerUtilities'
fi

cd $BUILD_BINARIESDIRECTORY

ctest -VV $CTEST_EXTRA_ARGS
