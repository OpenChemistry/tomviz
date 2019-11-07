#!/bin/bash

if [[ $AGENT_OS == 'Windows_NT' ]]; then
  # TBB
  echo "##vso[task.prependpath]$PIPELINE_WORKSPACE\\tbb"
  echo "##vso[task.prependpath]$PIPELINE_WORKSPACE\\tbb\\bin\\intel64\\vc14"
  echo "##vso[task.prependpath]$PIPELINE_WORKSPACE\\tbb\\lib\\intel64\\vc14"

  # Qt
  echo "##vso[task.prependpath]$PIPELINE_WORKSPACE\\Qt5.12.3_msvc2017_64\\bin"

  # ParaView
  echo "##vso[task.prependpath]$PARAVIEW_BUILD_FOLDER\\lib"
  echo "##vso[task.prependpath]$PARAVIEW_BUILD_FOLDER\\bin"
fi
