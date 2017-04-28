#!/bin/bash

set -ex

if [[ -z "${DOCKER_USERNAME}" || -z "${DOCKER_PASSWORD}" ]]; then
  echo "Environment variables DOCKER_USERNAME and DOCKER_PASSWORD must be set." >&2
  exit 1
fi

docker login --username="${DOCKER_USERNAME}" --password="${DOCKER_PASSWORD}"
docker push "${DOCKER_USERNAME}/statsd-tg"
