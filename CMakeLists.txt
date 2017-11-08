cmake_minimum_required(VERSION 3.2.0)

project(RealmObjectStore)

list(APPEND CMAKE_MODULE_PATH "${RealmObjectStore_SOURCE_DIR}/CMake")

include(CodeCoverage)
include(CompilerFlags)
include(Sanitizers)

# Sync is disabled unless -DREALM_ENABLE_SYNC=1 is specified when invoking CMake.
# FIXME: Flip the default once we can build against prebuilt sync binaries.
set(REALM_ENABLE_SYNC OFF CACHE BOOL "")

if(REALM_ENABLE_SYNC)
  add_definitions(-DREALM_ENABLE_SYNC)
endif()

set(PEGTL_INCLUDE_DIR ${RealmObjectStore_SOURCE_DIR}/external/pegtl)

add_subdirectory(src)
add_subdirectory(tests)
