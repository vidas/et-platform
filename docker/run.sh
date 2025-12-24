#!/bin/bash

set -e

USER_ID=$(id -u)
GROUP_ID=$(id -g)
USER_NAME=$(id -un)

set -x

docker run -it \
  -e USER_ID=$USER_ID \
  -e GROUP_ID=$GROUP_ID \
  -e USER_NAME=$USER_NAME \
  -e ARGS="$*" \
  --volume $HOME:$HOME \
  --volume $PWD:$PWD \
  --entrypoint $PWD/docker/entrypoint.sh \
  --workdir $PWD \
  et-platform:latest
