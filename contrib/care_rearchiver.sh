#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

####
#
# CARE re-archiver. Updates a CARE archive in .tgz.bin format.
# Prerequisites:
#  - a folder containing the extraction of the CARE archive
# Usage:
#  - add / remove / modify files in the folder
#  - run care_rearchiver.sh archive.tgz.bin archive
#
# Copyright Jonathan Passerat-Palmbach, 2016
#
####

archive="$1"
mode="$2"

archive_size=$(printf "%d" "0x$(tail -c 8 "${archive}" | hexdump -e '16/1 "%02X"' | cut -d ' ' -f1)")

file_size=$(stat -c %s "${archive}")
footer_size=21
extracting_program_size=$((file_size - archive_size - footer_size))
archive_no_bin=$(basename "${archive}" .bin)


function unarchive() {
  dd if="${archive}" of="${archive_no_bin}" bs=1 skip="${extracting_program_size}" count="${archive_size}"
  tar zxf "${archive_no_bin}"
}

function archive() {
  folder_name=$(basename "${archive_no_bin}" .tgz)
  new_archive="${folder_name}_new.tgz.bin"

  tar zcf "${archive_no_bin}" "${folder_name}"

  new_archive_size=$(stat -c %s "${archive_no_bin}")

  # add extracter
  dd if="${archive}" of="${new_archive}" bs=1 skip=0 count=${extracting_program_size}
  # add new archive
  dd if="${archive_no_bin}" of="${new_archive}" bs=1 skip=0 seek=${extracting_program_size} count="${new_archive_size}"
  ## add footer
  # I_LOVE_PIZZA
  echo -en \\x49\\x5f\\x4c\\x4f\\x56\\x45\\x5f\\x50\\x49\\x5a\\x5a\\x41\\x00 >> "${new_archive}"
  # size
  echo -en "$(printf "%016X" "${new_archive_size}" | sed -e 's/\([0-9A-F]\{2\}\)/\\\x\1/g')" >> "${new_archive}"

  chmod +x "${new_archive}"
  rm "${archive_no_bin}"
}


if [ "${mode}" == "archive" ]; then
  archive
else
  unarchive
fi

