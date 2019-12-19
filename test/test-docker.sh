#!/bin/sh
set -eu

# Test can run using either builder
BUILDER="docker"
if [ -z "$(command -v ${BUILDER})" ]; then
BUILDER="img"
else
  if [ -z "$(command -v ${BUILDER})" ]; then
    exit 125
  fi
fi

# Export for use in subshell
export BUILDER
export DOCKER_IMAGE="proot-me/proot"

TMP=$(mcookie)
TMP="/tmp/${TMP}"

# Generate image list
find "${SRC_DIR}" -name 'Dockerfile' -exec sh -c '
  TAG=$(echo "${1}" | ${SRC_DIR}/util/parse-docker-tag.awk)
  echo ${BUILDER} build -t ${DOCKER_IMAGE}:${TAG} -f ${1} ${SRC_DIR}
  ' sh {} \; | sort -k1 > "${TMP}"

# Build all images
sh "${TMP}"

rm "${TMP}"

unset BUILDER

