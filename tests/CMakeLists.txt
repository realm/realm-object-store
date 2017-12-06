add_library(Catch INTERFACE)
target_include_directories(Catch INTERFACE ../external/catch/single_include)

add_executable(Tests EXCLUDE_FROM_ALL
    any.cpp
    collection_change_indices.cpp
    index_set.cpp
    list.cpp
    main.cpp
    migrations.cpp
    object.cpp
    object_store.cpp
    parser.cpp
    primitive_list.cpp
    realm.cpp
    results.cpp
    schema.cpp
    thread_safe_reference.cpp
    transaction_log_parsing.cpp
    uuid.cpp
    util/event_loop.cpp
    util/test_file.cpp
    util/event_loop.hpp
    util/index_helpers.hpp
    util/templated_test_case.hpp
    util/test_file.hpp
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/file.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/metadata.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/partial_sync.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/permission.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/session/session.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/session/progress_notifications.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/session/wait_for_completion.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_manager.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_test_utils.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/user.cpp  >
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_test_utils.hpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/session/session_util.hpp>
)
target_include_directories(Tests PUBLIC "${RealmObjectStore_SOURCE_DIR}/tests")
target_link_libraries(Tests
    PUBLIC
        ObjectStore
        Catch
        $<$<BOOL:${REALM_ENABLE_SYNC}>:RealmSync::SyncServer>
)
set_target_properties(Tests PROPERTIES OUTPUT_NAME tests)

add_custom_command(TARGET Tests POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/sync-1.x.realm $<TARGET_FILE_DIR:Tests>)

create_coverage_target(generate-coverage Tests)

add_custom_target(run-tests USES_TERMINAL DEPENDS Tests COMMAND ./tests)

add_subdirectory(notifications-fuzzer)
