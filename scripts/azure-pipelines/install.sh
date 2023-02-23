#!/bin/bash

if [[ $AGENT_OS == 'Linux' ]]; then
  sudo apt-get update
  sudo apt-get install -y \
    ninja-build \
    qt5-default \
    libqt5x11extras5-dev \
    libqt5svg5-dev \
    qttools5-dev \
    qtxmlpatterns5-dev-tools \
    libtbb-dev \
    libxt-dev \
    libgtest-dev \
    libgl1-mesa-dev

  # We have to build gtest on ubuntu...
  cd /usr/src/gtest
  sudo cmake CMakeLists.txt
  sudo make
  sudo cp lib/*.a /usr/lib

elif [[ $AGENT_OS == 'Darwin' ]]; then
  brew install \
    ninja \
    qt5 \
    tbb

  # Install gtest
  cd $PIPELINE_WORKSPACE
  git clone --branch release-1.8.1 --depth 1 https://github.com/google/googletest
  cd googletest
  cmake .
  sudo make install
elif [[ $AGENT_OS == 'Windows_NT' ]]; then
  choco install wget
  choco install ninja

  # tbb
  wget --no-check-certificate -nv https://data.kitware.com/api/v1/file/5dbb4f19e3566bda4b49e6e9/download -O tbb.zip
  unzip -q tbb.zip
  rm tbb.zip
  mv tbb $PIPELINE_WORKSPACE

  # Qt
  wget --no-check-certificate -nv https://data.kitware.com/api/v1/file/5dbb2210e3566bda4b495afa/download -O Qt5.12.3_msvc2017_64.zip
  unzip -q Qt5.12.3_msvc2017_64.zip
  rm Qt5.12.3_msvc2017_64.zip
  mv Qt5.12.3_msvc2017_64 $PIPELINE_WORKSPACE

  # google-test
  wget --no-check-certificate -nv https://data.kitware.com/api/v1/file/58a3436c8d777f0721a61036/download -O googletest-install.zip
  unzip -q googletest-install.zip
  rm googletest-install.zip
  mv googletest-install $PIPELINE_WORKSPACE
fi
