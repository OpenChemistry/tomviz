FROM tomviz/tomviz-dependencies:latest
MAINTAINER Chris Harris <chris.harris@kitware.com>
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# Install docker client for docker tests
ARG DOCKER_VER="17.03.0-ce"

RUN wget -O /tmp/docker-$DOCKER_VER.tgz https://download.docker.com/linux/static/stable/x86_64/docker-$DOCKER_VER.tgz && \
  tar -xz -C /tmp -f /tmp/docker-$DOCKER_VER.tgz && \
  mv /tmp/docker/* /usr/bin

RUN echo "#!/bin/bash" > build.sh && \
  echo "set -e" >> build.sh && \
  echo "pip2 install -q -r ../tomviz/tests/python/requirements-dev.txt" >> build.sh && \
  echo "pip3 install -q -r ../tomviz/tests/python/requirements-dev.txt" >> build.sh && \
  echo "pip2 install -q -r ../tomviz/acquisition/requirements-dev.txt" >> build.sh  && \
  echo "pip3 install -q ../tomviz/tomviz/python/" >> build.sh && \
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
