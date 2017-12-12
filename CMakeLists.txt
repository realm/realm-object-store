cmake_minimum_required(VERSION 3.8.0)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "")

project(RealmObjectStore)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

include(CodeCoverage)
include(CompilerFlags)
include(Sanitizers)
include(Utilities)
include(SpecialtyBuilds)

file(STRINGS "${RealmObjectStore_SOURCE_DIR}/dependencies.list" DEPENDENCIES)
message("Dependencies: ${DEPENDENCIES}")
foreach(LINE IN LISTS DEPENDENCIES)
    string(REGEX MATCHALL "([^=]+)" KEY_VALUE ${LINE})
    list(LENGTH KEY_VALUE matches_count)
    if(matches_count STREQUAL 2)
        list(GET KEY_VALUE 0 KEY)
        list(GET KEY_VALUE 1 VALUE)
        set(DEP_${KEY} ${VALUE})
    endif()
endforeach()

if(NOT CMAKE_RELEASE_POSTFIX)
    set(CMAKE_DEBUG_POSTFIX "-dbg")
    set(CMAKE_MINSIZEDEBUG_POSTFIX "-dbg")
endif()

file(COPY
    "${RealmObjectStore_SOURCE_DIR}/CMake/aggregate.sh"
    DESTINATION "${RealmObjectStore_BINARY_DIR}"
)

find_package(RealmSync QUIET)
if(RealmSync_FOUND)
    message(STATUS "Realm Sync found!")
else()
    get_package_file_name(realm-sync "v${DEP_REALM_SYNC_VERSION}")
    set(REALM_URL "http://static.realm.io/downloads/sync/v${DEP_REALM_SYNC_VERSION}/${realm-sync_FILE_NAME}.tar.gz")

    message(STATUS "Downloading Realm Sync...")
    file(DOWNLOAD "${REALM_URL}" "${RealmObjectStore_BINARY_DIR}/${realm-sync_FILE_NAME}.tar.gz")

    message(STATUS "Uncompressing Realm Sync...")
    file(MAKE_DIRECTORY "${RealmObjectStore_BINARY_DIR}/${realm-sync_FILE_NAME}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xfz "${RealmObjectStore_BINARY_DIR}/${realm-sync_FILE_NAME}.tar.gz"
        WORKING_DIRECTORY "${RealmObjectStore_BINARY_DIR}/${realm-sync_FILE_NAME}"    
    )

    set(RealmSync_DIR "${RealmObjectStore_BINARY_DIR}/${realm-sync_FILE_NAME}/lib/cmake/realmsync")
    find_package(RealmSync REQUIRED)
endif()

option(REALM_ENABLE_SYNC "Enable Sync Support" ON)

add_library(Pegtl INTERFACE)
target_include_directories(Pegtl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/external/pegtl)

add_subdirectory(src)
if(NOT CMAKE_XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS MATCHES "iphoneos" 
    AND NOT CMAKE_XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS MATCHES "watchos" 
    AND NOT CMAKE_XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS MATCHES "tvos" 
)
    add_subdirectory(tests)
endif()

install(FILES LICENSE DESTINATION doc/realmobjectstore)

# Configure the realmobjectstore-config.cmake file
get_package_file_name(realm-sync ${DEP_REALM_SYNC_VERSION})
configure_file(CMake/realmobjectstore-config.cmake.in "${RealmObjectStore_BINARY_DIR}/realmobjectstore-config.cmake" @ONLY)

# Make the project importable from the build directory
export(EXPORT realm-object-store NAMESPACE RealmObjectStore:: FILE realmobjectstore-targets.cmake)

# Make the project importable from the install directory
install(EXPORT realm-object-store FILE realmobjectstore-targets.cmake NAMESPACE RealmObjectStore:: DESTINATION lib/cmake/realmobjectstore)
install(
    FILES
        "${RealmObjectStore_BINARY_DIR}/realmobjectstore-config.cmake"
    DESTINATION lib/cmake/realmobjectstore
)

# CPack
get_package_file_name(realm-object-store "v${DEP_VERSION}")
set(CPACK_GENERATOR TGZ)
set(CPACK_PACKAGE_FILE_NAME ${realm-object-store_FILE_NAME})
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
if(CMAKE_BUILD_TYPE STREQUAL Release)
    set(CPACK_STRIP_FILES TRUE)
endif()
include(CPack)
