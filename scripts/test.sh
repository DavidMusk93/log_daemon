#!/bin/bash

. ./util.sh

cleanup() {
    while read -u $rfd p 2>&$nfd; do kill -0 $p && kill -9 $p; done
    for i in {w,r,n}fd; do closefd ${!i}; done
    rm -f $pidfile
    trap - EXIT
    exit 0
    #never resume the main process
}

initialize() {
    tmpdir=/tmp
    NP=100
    pidfile=$tmpdir/cat.pid
    exec {wfd}>$pidfile
    exec {rfd}<$pidfile
    exec {nfd}>/dev/null
    trap 'cleanup' EXIT SIGINT
}

main() {
    set -x
    initialize
    for ((i = 0; i < NP; i++)); do
        logcat &>/tmp/logcat.$i.log &
        echo $! >&$wfd
    done
    wait
}
main "$@"
