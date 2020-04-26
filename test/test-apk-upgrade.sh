#!/bin/sh
# Test package upgrade triggers for Alpine Linux
set -eu

# Import common functions
. ./lib/common.sh

# Check for test dependencies
check_dependencies apk

# Check if running in PRoot
get_tracer_pid

apk update

apk upgrade
