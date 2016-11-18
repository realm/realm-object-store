###########################################################################
#
# Copyright 2016 Realm Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
###########################################################################

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)
add_compile_options("$<$<CONFIG:DEBUG>:-DREALM_DEBUG>")
add_compile_options("$<$<CONFIG:COVERAGE>:-DREALM_DEBUG>")
add_compile_options(
    -DREALM_HAVE_CONFIG
    -Wall
    -Wextra
    -Wno-missing-field-initializers
    -Wempty-body
    -Wparentheses
    -Wunknown-pragmas
    -Wunreachable-code
)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    add_compile_options(
        -Wassign-enum
        -Wbool-conversion
        -Wconditional-uninitialized
        -Wconstant-conversion
        -Wenum-conversion
        -Wint-conversion
        -Wmissing-prototypes
        -Wnewline-eof
        -Wshorten-64-to-32
        -Wimplicit-fallthrough
    )
endif()

if(${CMAKE_GENERATOR} STREQUAL "Ninja")
    if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcolor-diagnostics")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
    elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
    endif()
endif()

if(APPLE)
    find_library(CF_LIBRARY CoreFoundation)
    list(APPEND PLATFORM_LIBRARIES ${CF_LIBRARY})
elseif(REALM_PLATFORM STREQUAL "Android")
    find_library(ANDROID_LIBRARY android)
    find_library(ANDROID_LOG_LIBRARY log)
    list(APPEND PLATFORM_LIBRARIES ${ANDROID_LIBRARY})
    list(APPEND PLATFORM_LIBRARIES ${ANDROID_LOG_LIBRARY})
endif()

if(REALM_PLATFORM STREQUAL "Node")
    find_library(UV_LIBRARY NAMES uv libuv)
    find_path(UV_INCLUDE_DIR uv.h)

    set(PLATFORM_DEFINES "REALM_PLATFORM_NODE=1")
    list(APPEND PLATFORM_LIBRARIES ${UV_LIBRARY})
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    find_library(UV_LIBRARY NAMES uv libuv)
    find_path(UV_INCLUDE_DIR uv.h)

    set(PLATFORM_DEFINES "REALM_PLATFORM_LINUX=1")
    list(APPEND PLATFORM_LIBRARIES ${UV_LIBRARY})
endif()
