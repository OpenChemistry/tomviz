FROM tomviz/tomviz-dependencies:latest
MAINTAINER Chris Harris <chris.harris@kitware.com>

RUN echo "#!/bin/bash" > build.sh && \
  echo "set -e" >> build.sh && \
  echo "mkdir tomviz-build" >> build.sh && \
  echo "cd tomviz-build" >> build.sh && \
  echo "cmake -G Ninja -DCMAKE_BUILD_TYPE:STRING=Release \\" >> build.sh && \
  echo "  -DENABLE_TESTING:BOOL=ON \\" >> build.sh && \
  echo "  -DParaView_DIR:PATH=$PARAVIEW_DIR \\" >> build.sh && \
  echo "  -DITK_DIR:PATH=$ITK_DIR \\" >> build.sh && \
  echo "  -DQt5_DIR:PATH=$QT5_DIR \\" >> build.sh && \
  echo "  -DGTEST_LIBRARY:PATH=$GTEST_LIBRARY \\" >> build.sh && \
  echo "  -DGTEST_MAIN_LIBRARY:PATH=$GTEST_MAIN_LIBRARY \\" >> build.sh && \
  echo "  -DGTEST_INCLUDE_DIR:PATH=$GTEST_INCLUDE_DIR \\" >> build.sh && \
  echo "  ../tomviz" >> build.sh && \
  echo "ninja -j \$(nproc)" >> build.sh && \
  echo "pip install -r ../tomviz/tests/python/requirements-dev.txt" >> build.sh && \
  echo "ctest -V" >> build.sh && \
  chmod u+x build.sh

ENTRYPOINT ["/build.sh"]

ARG BUILD_DATE
ARG IMAGE=tomviz-builder
ARG VCS_REF
ARG VCS_URL
LABEL org.label-schema.build-date=$BUILD_DATE \
      org.label-schema.name=$IMAGE \
      org.label-schema.description="Image containing tomviz environment to build and test tomviz" \
      org.label-schema.url="https://github.com/OpenChemistry/tomviz" \
      org.label-schema.vcs-ref=$VCS_REF \
      org.label-schema.vcs-url=$VCS_URL \
      org.label-schema.schema-version="1.0"
