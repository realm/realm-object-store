#!/bin/sh

set -e

cmake -DCMAKE_BUILD_TYPE=Coverage -DREALM_SYNC_PREFIX=/source/realm-sync/ .
sh realm-sync/build.sh config
make VERBOSE=1 -j2 generate-coverage-cobertura

