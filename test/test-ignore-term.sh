#!/bin/sh
# Test for ignoring terminating signals
set -eu

# Import common functions
source ./lib/common.sh

# Check for test dependencies
check_dependencies cut grep kill

# Kill tracees on abnormal termination via TERM signal
${PROOT} sh -c "kill -15 $(get_tracer_pid)"

