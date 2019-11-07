#!/bin/bash

# See: https://github.com/Microsoft/azure-pipelines-image-generation/issues/738
# If azure-pipelines adds docker to MacOS, we can skip this.

# We need to use a specific version of docker, the latest version
# fails to start.
brew cask install https://raw.githubusercontent.com/Homebrew/homebrew-cask/8ce4e89d10716666743b28c5a46cd54af59a9cc2/Casks/docker.rb
sudo /Applications/Docker.app/Contents/MacOS/Docker --quit-after-install --unattended
/Applications/Docker.app/Contents/MacOS/Docker --unattended &

attempts=0
while ! docker info 2>/dev/null ; do
    sleep 5
    attempts=`expr $attempts + 1`
    if pgrep -xq -- "Docker"; then
        echo 'docker still running, waiting...'
    else
        echo 'docker not running, restarting...'
        /Applications/Docker.app/Contents/MacOS/Docker --unattended &
    fi
    if [ $attempts -gt 30 ]; then
        >&2 echo 'Failed to run docker'
        exit 1
    fi;

    echo 'Waiting for docker service to be in the running state'
done
