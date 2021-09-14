#!/bin/sh
# Common test functions
set -eu

# Check for test dependencies
check_dependencies()
{
  for cmd in "${@}"; do
    if ! command -v "${cmd}" > /dev/null; then
      exit 125
    fi
  done
}

# Get TracerPid
get_tracer_pid()
{
  awk '/TracerPid/ { print $2 }' /proc/self/status 
}

