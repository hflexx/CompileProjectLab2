#!/bin/bash

# Change directory to build
cd /coursegrader/build

# Run make clean
make clean

# Run make
make

# Change directory to test
cd /coursegrader/test

# Run python script
python3 run.py s2

# python3 score.py s2