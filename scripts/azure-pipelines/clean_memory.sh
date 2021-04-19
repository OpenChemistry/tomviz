#!/bin/bash

# Clean up some memory so we can tar and upload the paraview cache

if [[ $AGENT_OS == 'Linux' ]]; then
  rm -rf $PARAVIEW_SOURCE_FOLDER
  rm -rf $BUILD_BINARIESDIRECTORY
  rm -rf $BUILD_SOURCESDIRECTORY
fi
