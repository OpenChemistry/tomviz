FROM library/ubuntu:16.04
MAINTAINER Chris Harris <chris.harris@kitware.com>

RUN apt-get update && apt-get -y install \
  git \
  wget \
  cmake \
  python \
  python-pip \
  python3 \
  python3-pip \
  python3-dev \
  g++ \
  libtbb-dev \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  libxt-dev

RUN pip3 install -U pip && \
  pip3 install numpy

# Build and install ninja.
ARG NINJA_VERSION=v1.7.2
RUN git clone -b $NINJA_VERSION https://github.com/martine/ninja.git && \
  cd ninja && \
  ./configure.py --bootstrap && \
  mv ninja /usr/bin/ && \
  cd .. && \
  rm -rf ninja

# Build and install googletest
ARG GTEST_VERSION=release-1.8.0
RUN git clone -b $GTEST_VERSION https://github.com/google/googletest.git && \
  mkdir googletest-build && \
  cd googletest-build && \
  cmake -DCMAKE_INSTALL_PREFIX:PATH=/opt/googletest ../googletest && \
  cmake --build . --target install && \
  rm -rf googletest

# Build and install Qt
ARG QT_VERSION=5.8.0
ARG QT_BASE_DOWNLOAD_URL=http://download.qt.io/official_releases/qt/5.8/5.8.0/single/
ENV QT_DIR qt-everywhere-opensource-src-$QT_VERSION
ENV QT_TARBALL $QT_DIR.tar.gz
ENV QT_DOWNLOAD_URL $QT_BASE_DOWNLOAD_URL/$QT_TARBALL
RUN wget $QT_DOWNLOAD_URL && \
  md5=$(md5sum ./$QT_TARBALL | awk '{ print $1 }') && \
  [ $md5 = "a9f2494f75f966e2f22358ec367d8f41" ] && \
  tar -xzvf $QT_TARBALL && \
  rm $QT_TARBALL && \
  cd $QT_DIR && \
  ./configure -opensource \
    -release \
    -confirm-license \
    -nomake examples \
    -skip qtconnectivity \
    -skip qtlocation \
    -skip qtmultimedia \
    -skip qtsensors \
    -skip qtserialport \
    -skip qtsvg \
    -skip qtwayland \
    -skip qtwebchannel \
    -skip qtwebengine \
    -skip qtwebsockets \
    -no-dbus \
    -no-openssl \
    -qt-libjpeg \
    -qt-pcre && \
  make -j$(grep -c processor /proc/cpuinfo) && \
  make install && \
  cd .. && \
  rm -rf $QT_DIR

# Build and install ITK
ARG ITK_VERSION=v4.11.0
RUN git clone -b $ITK_VERSION git://itk.org/ITK.git && \
  mkdir itk-build && \
  cd itk-build && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE:STRING=Release \
    -DITK_LEGACY_REMOVE:BOOL=ON \
    -DITK_LEGACY_SILENT:BOOL=ON \
    -DModule_BridgeNumPy:BOOL=ON \
    -DBUILD_TESTING:BOOL=OFF \
    -DITK_WRAP_unsigned_short:BOOL=ON \
    -DITK_BUILD_DEFAULT_MODULES:BOOL=OFF \
    -DITKGroup_Core:BOOL=ON \
    -DITKGroup_Filtering:BOOL=ON \
    -DITKGroup_Segmentation:BOOL=ON \
    -DITKGroup_Registration:BOOL=ON \
    -DITKGroup_Nonunit:BOOL=ON \
    -DITK_WRAP_PYTHON:BOOL=ON \
    -DBUILD_EXAMPLES:BOOL=OFF \
    -DBUILD_SHARED_LIBS:BOOL=ON \
    -DCMAKE_INSTALL_PREFIX:PATH=/opt/itk \
    -DPYTHON_EXECUTABLE:PATH=/usr/bin/python3 \
    ../ITK && \
  cmake --build . --target install && \
  cd .. && \
  rm -rf ITK && \
  rm -rf itk-build

# Build and install ParaView
ARG PARAVIEW_VERSION=f700a4d7f32ccb7e31f1da6e6fd1c6188dac2feb
RUN git clone --recursive git://github.com/kitware/paraview.git && \
  cd paraview && \
  git checkout $PARAVIEW_VERSION && \
  git submodule update && \
  cd .. && \
  mkdir paraview-build && \
  cd paraview-build && \
  cmake -G Ninja -DCMAKE_BUILD_TYPE:STRING=Release \
    -DBUILD_TESTING:BOOL=OFF \
    -DPARAVIEW_ENABLE_CATALYST:BOOL=OFF \
    -DPARAVIEW_ENABLE_PYTHON:BOOL=ON \
    -DPARAVIEW_QT_VERSION:STRING=5 \
    -DPARAVIEW_ENABLE_WEB:BOOL=OFF \
    -DPARAVIEW_ENABLE_EMBEDDED_DOCUMENTATION:BOOL=OFF\
    -DPARAVIEW_USE_QTHELP:BOOL=OFF \
    -DVTK_RENDERING_BACKEND:STRING=OpenGL2 \
    -DVTK_SMP_IMPLEMENTATION_TYPE:STRING=TBB \
    -DVTK_PYTHON_FULL_THREADSAFE:BOOL=ON \
    -DVTK_NO_PYTHON_THREADS:BOOL=OFF \
    -DQt5_DIR:PATH=/usr/local/Qt-5.8.0/lib/cmake/Qt5 \
    -DVTK_PYTHON_VERSION:STRING=3 \
    ../paraview && \
  cmake --build . && \
  cd .. && \
  rm -rf paraview/.git && \
  find paraview-build -name '*.o' -delete && \
  find paraview-build -name '*.cxx' -delete

# Provide paths to downstream builds
ONBUILD ENV QT5_DIR /usr/local/Qt-5.8.0/lib/cmake/Qt5
ONBUILD ENV PARAVIEW_DIR /paraview-build
ONBUILD ENV ITK_DIR /opt/itk/lib/cmake/ITK-4.11
ONBUILD ENV GTEST_LIBRARY /opt/googletest/lib/libgtest.a
ONBUILD ENV GTEST_MAIN_LIBRARY /opt/googletest/lib/libgtest_main.a
ONBUILD ENV GTEST_INCLUDE_DIR /opt/googletest/include

ARG BUILD_DATE
ARG IMAGE=tomviz-dependencies
ARG VCS_REF
ARG VCS_URL
LABEL org.label-schema.build-date=BUILD_DATE \
      org.label-schema.name=$IMAGE \
      org.label-schema.description="Base image containing tomviz dependencies" \
      org.label-schema.url="https://github.com/OpenChemistry/tomviz" \
      org.label-schema.vcs-ref=$VCS_REF \
      org.label-schema.vcs-url=$VCS_URL \
      org.label-schema.schema-version="1.0"
