#!/bin/bash

set -e
set -x

docker build -t et-platform -f docker/Dockerfile .
