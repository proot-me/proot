#!/bin/sh
set -eu

# Test can run using either builder
BUILDER="docker"
if [ -z "$(command -v ${BUILDER})" ] || ! docker run hello-world > /dev/null 2>&1; then
    BUILDER="img"
    if [ -z "$(command -v ${BUILDER})" ]; then
        exit 125
    fi
fi

# Export for use in subshell
export BUILDER
export DOCKER_IMAGE="ghcr.io/proot-me/proot"

TMP=$(mktemp)

# Generate image list
find . -name 'Dockerfile' -exec sh -c '
  TAG=$(echo "${1}" | ../util/parse-docker-tag.awk)
  FLAGS_FILE="$(dirname ${1})/.build-flags"
  FLAGS=$([ -f "${FLAGS_FILE}" ] && cat "${FLAGS_FILE}" || true)
  echo ${BUILDER} build ${FLAGS} -t ${DOCKER_IMAGE}:${TAG} -f ${1} ..
  ' sh {} \; | sort -k1 > "${TMP}"

cat "${TMP}"

# Build all images
sh "${TMP}"

rm "${TMP}"

unset BUILDER

