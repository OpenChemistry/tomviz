#!/bin/bash

# Clean up some memory so we can tar and upload the paraview cache

if [[ $AGENT_OS == 'Linux' ]]; then
  rm -rf $PARAVIEW_SOURCE_FOLDER
  rm -rf $PARAVIEW_BUILD_FOLDER
  rm -rf $BUILD_SOURCESDIRECTORY
  rm -rf $BUILD_BINARIESDIRECTORY
fi
