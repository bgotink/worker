#!/bin/sh

. ../env.sh

run 'cat {} {0/%1/2} > {0/%.part1/}' '*.part1'
