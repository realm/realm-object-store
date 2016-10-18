#!/bin/sh

set -e
rm -rf CMakeCache.txt
find . -name CMakeFiles | xargs rm -rf

docker build -t ci/realm-object-server:build .
docker run -it --rm -v $(pwd):/source -w /source ci/realm-object-server:build \
  "cmake -DCMAKE_BUILD_TYPE=Coverage . && make VERBOSE=1 -j2 generate-coverage-cobertura"

