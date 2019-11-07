#!/bin/bash

if [[ $AGENT_OS == 'Linux' ]]; then
  sudo apt-get update
  sudo apt-get install -y \
    ninja-build \
    qt5-default \
    libqt5x11extras5-dev \
    qttools5-dev \
    qtxmlpatterns5-dev-tools \
    libtbb-dev \
    libxt-dev \
    libgtest-dev

  # We have to build gtest on ubuntu...
  cd /usr/src/gtest
  sudo cmake CMakeLists.txt
  sudo make
  sudo cp *.a /usr/lib

elif [[ $AGENT_OS == 'Darwin' ]]; then
  brew install \
    ninja \
    qt5 \
    tbb

  # Downgrade cmake to 3.14.6
  # TODO: Remove this when our VTK includes this commit:
  # https://gitlab.kitware.com/vtk/vtk/commit/eebd1042c88d9ae047f3620590587f81e5d44503
  brew unlink cmake
  brew install https://raw.githubusercontent.com/Homebrew/homebrew-core/74d54460d0af8e21deacee8dcced325cfc191141/Formula/cmake.rb
  brew switch cmake 3.14.6

  # Install gtest
  cd $PIPELINE_WORKSPACE
  git clone https://github.com/google/googletest
  cd googletest
  cmake .
  sudo make install

  # For some reason, not all standard library header files are on
  # Mojave by default. This fixes the issue.
  sudo installer -pkg /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pkg -target /

  # Install docker
  # If azure-pipelines adds docker to MacOS, we can skip this.
  cd $BUILD_SOURCESDIRECTORY
  scripts/azure-pipelines/mac_install_docker.sh

elif [[ $AGENT_OS == 'Windows_NT' ]]; then
  choco install wget
  choco install ninja

  # tbb
  wget -nv https://data.kitware.com/api/v1/file/5dbb4f19e3566bda4b49e6e9/download -O tbb.zip
  unzip -q tbb.zip
  rm tbb.zip
  mv tbb $PIPELINE_WORKSPACE

  # Qt
  wget -nv https://data.kitware.com/api/v1/file/5dbb2210e3566bda4b495afa/download -O Qt5.12.3_msvc2017_64.zip
  unzip -q Qt5.12.3_msvc2017_64.zip
  rm Qt5.12.3_msvc2017_64.zip
  mv Qt5.12.3_msvc2017_64 $PIPELINE_WORKSPACE

  # google-test
  wget -nv https://data.kitware.com/api/v1/file/58a3436c8d777f0721a61036/download -O googletest-install.zip
  unzip -q googletest-install.zip
  rm googletest-install.zip
  mv googletest-install $PIPELINE_WORKSPACE
fi
