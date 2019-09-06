Building tomviz-tomopy-pipeline
===============================

In order to build this container the tomviz repository needs to be in the docker
build context. Therefore the build must be performed from the root of the
repository.

    cd <tomviz-repo-root>
    docker build -f docker/tomviz-tomopy-pipeline/Dockerfile .

