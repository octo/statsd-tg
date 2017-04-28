#!/bin/bash

set -ex

declare -r SRCDIR="$(realpath "$(dirname "$0")")"
declare -r TOPSRCDIR="$(realpath "${SRCDIR}/../..")"
declare -r BUILDDIR="$(mktemp -d)"
declare -r DESTDIR="$(mktemp -d)"

export CC="$(which musl-gcc)"
export LDFLAGS='-static'

cd "${BUILDDIR}"

"${TOPSRCDIR}/configure" --prefix='/usr'
make clean
make all
make install DESTDIR="${DESTDIR}"

strip "${DESTDIR}/usr/bin/statsd-tg"

DOCKER_NAME="statsd-tg"
if [[ -n "${DOCKER_USERNAME}" ]]; then
  DOCKER_NAME="${DOCKER_USERNAME}/${DOCKER_NAME}"
fi

# TODO(octo): this doesn't work as intended because Docker tags are more strict
# than Git branch names. I failed to find a reference for allowed tag names, but
# suspect that slashes are an issue.
#if [[ "${TRAVIS_BRANCH:-master}" != "master" ]]; then
#  DOCKER_NAME="${DOCKER_NAME}:${TRAVIS_BRANCH}"
#fi

cp "${SRCDIR}/Dockerfile" "${DESTDIR}/"
docker build -t "${DOCKER_NAME}" "${DESTDIR}"
docker images

rm -rf "${BUILDDIR}" "${DESTDIR}"
