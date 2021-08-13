#!/bin/bash

DIR=$(readlink -f $0)
DIR=$(dirname $0)

cppcheck -f --enable=all --platform=unix32 --std=c++17 --inconclusive --library=posix "$DIR/usbmodem/src"
