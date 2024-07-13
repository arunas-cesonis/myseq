#!/bin/sh
set -eu
DESTINATION=$PWD/cpptrace_installed
git clone https://github.com/jeremy-rifkin/cpptrace.git
cd cpptrace
git checkout v0.6.2
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$DESTINATION
make -j
make install
