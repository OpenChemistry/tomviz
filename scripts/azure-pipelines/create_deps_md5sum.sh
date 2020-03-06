#!/bin/bash

QMAKE_EXE=qmake
PYTHON_EXE=python3
MD5SUM_EXE=md5sum

if [[ $AGENT_OS == 'Windows_NT' ]]; then
  # Windows just has it labelled "python". Mac requires "python3".
  PYTHON_EXE=python
elif [[ $AGENT_OS == 'Darwin' ]]; then
  QMAKE_EXE=/usr/local/opt/qt/bin/qmake
  # Need '-r' for md5sum-like-output
  MD5SUM_EXE='md5 -r'
fi

versions_file=_versions.txt

paraview_sha1=$(git ls-remote https://github.com/openchemistry/paraview | head -1 | cut -f 1)

# Add more versions here if paraview needs to be re-built when
# these versions change.
echo $paraview_sha1 >> $versions_file
$PYTHON_EXE --version >> $versions_file
$QMAKE_EXE --version >> $versions_file

deps_md5sum=$($MD5SUM_EXE $versions_file | cut -d ' ' -f1)
rm $versions_file

echo "##vso[task.setvariable variable=deps_md5sum]$deps_md5sum"
