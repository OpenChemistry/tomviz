FROM tomviz/tomviz-dependencies:latest
MAINTAINER Chris Harris <chris.harris@kitware.com>

RUN echo "#!/bin/bash" > build.sh && \
  echo "set -e" >> build.sh && \
  echo "pip2 install -r ../tomviz/tests/python/requirements-dev.txt" >> build.sh && \
  echo "pip3 install -r ../tomviz/tests/python/requirements-dev.txt" >> build.sh && \
  echo "pip2 install -r ../tomviz/acquisition/requirements-dev.txt" >> build.sh && \
  echo "ctest -VV -S /tomviz/cmake/CircleContinuous.cmake" >> build.sh && \
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
