#!/bin/bash

set -e

# get the build type and platform
core_tarball=$(echo realm-core*.tar.gz)
if [[ "${core_tarball}" =~ realm-core-(.+)-v.+-([^-]+)\.tar\.gz ]]; then
    build_type=${BASH_REMATCH[1]}
    platform=${BASH_REMATCH[2]}
else
    { echo >&2 "Cannot find build info.  Aborting."; exit 1; }
fi

sha1=$(git rev-parse --short HEAD)
date=$(date "+%Y-%m-%d")
filename="realm-aggregate-${platform}-${build_type}-${date}-${sha1}"

# find the tar files that should be concatenated
core_tar=$(echo realm-core*.tar.gz)
sync_tar=$(echo realm-sync*.tar.gz)
os_tar=$(echo realm-object-store*.tar.gz)

# extract the tarballs
rm -rf aggregate
mkdir aggregate
tar xzf "${core_tar}" -C aggregate
tar xzf "${sync_tar}" -C aggregate
tar xzf "${os_tar}" -C aggregate

pushd aggregate
rm -rf bin
tar cJf "${filename}".tar.xz -- *
popd
