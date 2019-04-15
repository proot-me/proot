#!/bin/sh

# $1: instance id,
# $2: waiting time before server binding
# $3: waiting time before client connecting
# $4: client waiting time before sending message
start_ips_program() {
    ../../src/proot -v 1 -p $1 python tcpsockets.py $2 $3 $4 $1
}

#  Instance 1:  bind                         connect send&close
#  Instance 2:       bind connect send&close

start_ips_program 5432:54320 1 3 1 &
start_ips_program 5432:54321 2 3 1
#start_ips_program 10 0 1 0

# If PRoot allows these two processes to proceed without errors, the test passes.
# Without the -p option, they cannot be run at the same time because they use the same port.
