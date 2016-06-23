cmake_minimum_required(VERSION 3.2.0)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "")
set(REALM_HAVE_CONFIG false CACHE BOOL "Enable to generate and use config.h from core.")

project(realm-object-store)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

include(CodeCoverage)
include(CompilerFlags)
include(Sanitizers)

include(RealmCore)
set(REALM_CORE_VERSION "1.1.0" CACHE STRING "")
use_realm_core(${REALM_CORE_VERSION})

set(PEGTL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/pegtl)

add_subdirectory(src)
add_subdirectory(tests)
