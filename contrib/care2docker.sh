#!/bin/bash
# shellcheck disable=SC1003
# shellcheck disable=SC2016
set -euo pipefail
IFS=$'\n\t'

####
#
# CARE archive to Docker image converter.
#
# There is absolutely **no garanty** this works for every CARE archive.
# This essentially writes the archive's rootfs on top of the base's so there
# might be some cases where this breaks.
# Just here to simplify basic conversions.
#
# Prerequisites:
#  - a folder containing a CARE archive
#  - docker installed
# Usage:
#  - ./care2docker.sh </path/to/archive.tgz.bin>
#
# Copyright Jonathan Passerat-Palmbach, 2016
#
####

archive=$(readlink -f "$1")
basedir=$(dirname "${archive}")

pushd "${basedir}"
"${archive}"
popd

archive_name=$(basename "${archive}" | cut -d '.' -f1)

tar cf "${basedir}/${archive_name}_rootfs.tar" --xform="s,$(echo "${basedir}" | cut -d '/' -f 2-)/${archive_name}/rootfs/,," "${basedir}/${archive_name}"/rootfs/*

cat << EOF > "${basedir}"/Dockerfile
FROM debian:jessie

ADD "${archive_name}_rootfs.tar" /

$(echo -n "ENV " ; grep -e \''.*=.*'\''  \\' "${basedir}/${archive_name}/re-execute.sh" |
sed -e "s/.\(.*\)=/\1='/g" |
sed -e '$ s/  \\//g')

WORKDIR $(grep -e '^-w '\''.*'\''  \\' "${basedir}/${archive_name}/re-execute.sh" |
sed -e 's/^-w '\''\(.*\)'\''  \\/\1/g')

ENTRYPOINT ["$(grep  -e '\[ $nbargs -ne 0 \] || set -- \\' -A 1 "${basedir}/${archive_name}/re-execute.sh" |
tail -n1 |
sed -e 's/'\''\(.*\)'\'' \\/\1/g'
)"]
EOF

cd "${basedir}" || exit 1
docker build -t "${archive_name}" .

