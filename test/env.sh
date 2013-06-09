#!/bin/bash

VERBOSE=0;
if [[ $1 =~ -v|--verbose ]]; then
    VERBOSE=1;
    shift;
fi

run() {
    if [ $VERBOSE -eq 1 ]; then
        ../../bin/worker -v "$@"
    else
        ../../bin/worker "$@"
    fi
}