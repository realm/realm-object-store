include_directories(../external/catch/single_include .)

set(HEADERS
    util/event_loop.hpp
    util/index_helpers.hpp
    util/test_file.hpp
)

set(SOURCES
    collection_change_indices.cpp
    handover.cpp
    index_set.cpp
    list.cpp
    main.cpp
    migrations.cpp
    object_store.cpp
    parser.cpp
    realm.cpp
    results.cpp
    schema.cpp
    transaction_log_parsing.cpp
    util/event_loop.cpp
    util/test_file.cpp
)


find_package(Threads)
find_package(PkgConfig REQUIRED)
pkg_search_module(OPENSSL REQUIRED openssl)

if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
endif()

add_executable(tests ${SOURCES} ${HEADERS})
target_compile_definitions(tests PRIVATE ${PLATFORM_DEFINES})
target_link_libraries(tests realm-object-store ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${PLATFORM_LIBRARIES})

if(REALM_ENABLE_SYNC)
    target_link_libraries(tests realm-sync-server)
endif()

create_coverage_target(generate-coverage tests)

add_custom_target(run-tests USES_TERMINAL DEPENDS tests COMMAND ./tests)

add_subdirectory(notifications-fuzzer)
