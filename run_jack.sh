#!/bin/sh
set -eu
export MYSEQ_LOAD_JSON_FILE=./test.json
export DYLD_LIBRARY_PATH=/usr/local/lib
exec ./bin/MySeq.app/Contents/MacOS/MySeq
