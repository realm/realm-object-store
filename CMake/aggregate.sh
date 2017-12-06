#!/bin/bash

set -e
set -x

# we need GNU tar since BSD tar does not support concatenation
type gtar >/dev/null 2>&1 || { echo >&2 "I require gtar but it's not installed.  Please install the gnu-tar package with homebrew.  Aborting."; exit 1; }

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

# remove any tar file
rm -f -- *.tar

# uncompress the tar.gz files and keep the originals
gunzip --keep -- *.tar.gz

# create an empty tar file. This will make the script non-destructive since
# it won't modify the original packages
gtar cvf "${filename}.tar" --files-from /dev/null

# find the tar files that should be concatenated
core_tar=$(echo realm-core*.tar)
sync_tar=$(echo realm-sync*.tar)
os_tar=$(echo realm-object-store*.tar)

# concatenate the tar files
gtar --concatenate --file "${filename}.tar" "${core_tar}" "${sync_tar}" "${os_tar}"

# compress the aggregated tar file
gzip "${filename}.tar"

# clean up
rm -f -- *.tar
