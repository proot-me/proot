#!/bin/sh
# Test for ignoring terminating signals
set -eu

# Import common functions
. ./lib/common.sh

# Check for test dependencies
check_dependencies cut grep kill

# Kill tracees on abnormal termination via TERM signal
# shellcheck disable=SC2016
${PROOT} sh -c 'kill -15 $(. ./lib/common.sh; get_tracer_pid)'

