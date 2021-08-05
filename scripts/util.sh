#!/bin/bash

closefd() {
    eval "exec $1>&-"
}

rcok() {
    return $?
}
