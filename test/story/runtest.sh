#!/bin/sh

../../bin/worker --verbose -- 'cat {} {0/%1/2} > {0/%.part1/}' '*.part1'
