# -----
# libuv
# -----
if(NOT REALM_PLATFORM OR REALM_PLATFORM STREQUAL "Node" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
    find_library(UV_LIBRARY NAMES uv libuv)
    if(UV_LIBRARY)
        find_path(UV_INCLUDE_DIR uv.h)
        message(STATUS "Using libuv")
    elseif(REALM_PLATFORM STREQUAL "Node")
        message(FATAL_ERROR "Platform set to Node but libuv was not found!") 
    endif()
endif()

# -----
# epoll
# -----
check_symbol_exists(epoll_create sys/epoll.h REALM_HAVE_EPOLL)
    
# -----------

if(APPLE)
    set(IMPL_DIR apple)
elseif(REALM_HAVE_EPOLL)
    set(IMPL_DIR epoll)
elseif(WIN32)
    set(IMPL_DIR windows)
else()
    set(IMPL_DIR generic)
endif()

add_library(ObjectStore
    binding_callback_thread_observer.cpp
    binding_callback_thread_observer.hpp
    collection_notifications.cpp
    collection_notifications.hpp
    descriptor_ordering.hpp
    execution_context_id.hpp
    feature_checks.hpp
    impl/${IMPL_DIR}/external_commit_helper.cpp
    impl/apple/external_commit_helper.hpp
    $<$<BOOL:${APPLE}>:impl/apple/keychain_helper.cpp>
    impl/apple/keychain_helper.hpp
    impl/collection_change_builder.cpp
    impl/collection_change_builder.hpp
    impl/collection_notifier.cpp
    impl/collection_notifier.hpp
    impl/epoll/external_commit_helper.hpp
    impl/external_commit_helper.hpp
    $<$<AND:$<NOT:$<BOOL:${APPLE}>>,$<NOT:$<BOOL:${ANDROID}>>>:util/generic/event_loop_signal.cpp>
    impl/generic/external_commit_helper.hpp
    impl/list_notifier.cpp
    impl/list_notifier.hpp
    impl/notification_wrapper.hpp
    impl/object_accessor_impl.hpp
    impl/object_notifier.cpp
    impl/object_notifier.hpp
    impl/primitive_list_notifier.cpp
    impl/primitive_list_notifier.hpp
    impl/realm_coordinator.cpp
    impl/realm_coordinator.hpp
    impl/results_notifier.cpp
    impl/results_notifier.hpp
    impl/transact_log_handler.cpp
    impl/transact_log_handler.hpp
    impl/weak_realm_notifier.cpp
    impl/weak_realm_notifier.hpp
    index_set.cpp
    index_set.hpp
    list.cpp
    list.hpp
    object.cpp
    object.hpp
    object_accessor.hpp
    object_schema.cpp
    object_schema.hpp
    object_store.cpp
    object_store.hpp
    parser/parser.cpp
    parser/parser.hpp
    parser/query_builder.cpp
    parser/query_builder.hpp
    property.hpp
    results.cpp
    results.hpp
    schema.cpp
    schema.hpp
    shared_realm.cpp
    shared_realm.hpp
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/partial_sync.hpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_config.hpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_manager.hpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_permission.hpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_session.hpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_user.hpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/impl/sync_client.hpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/impl/sync_file.hpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/impl/sync_metadata.hpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/partial_sync.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_config.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_manager.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_permission.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_session.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/sync_user.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/impl/sync_file.cpp>
    $<$<BOOL:${REALM_ENABLE_SYNC}>:sync/impl/sync_metadata.cpp>
    $<$<AND:$<BOOL:${REALM_ENABLE_SYNC}>,$<BOOL:${APPLE}>>:sync/impl/apple/network_reachability_observer.cpp>
    $<$<AND:$<BOOL:${REALM_ENABLE_SYNC}>,$<BOOL:${APPLE}>>:sync/impl/apple/system_configuration.cpp>
    thread_safe_reference.cpp
    thread_safe_reference.hpp
    util/aligned_union.hpp
    util/android/event_loop_signal.hpp
    util/any.hpp
    util/apple/event_loop_signal.hpp
    util/atomic_shared_ptr.hpp
    util/compiler.hpp
    util/event_loop_signal.hpp
    util/format.cpp
    util/format.hpp
    util/generic/event_loop_signal.hpp
    util/tagged_bool.hpp
    util/time.hpp
    util/uuid.cpp
    util/uuid.hpp
    util/uv/event_loop_signal.hpp
)
set_target_properties(ObjectStore
    PROPERTIES
        OUTPUT_NAME realmobjectstore
        POSITION_INDEPENDENT_CODE 1
)
target_compile_definitions(ObjectStore
    PUBLIC
        $<$<BOOL:${ANDROID}>:__STDC_CONSTANT_MACROS=1>
        $<$<BOOL:${ANDROID}>:ANDROID>
        $<$<BOOL:${REALM_HAVE_EPOLL}>:REALM_HAVE_EPOLL>
        $<$<BOOL:${UV_LIBRARY}>:REALM_HAVE_UV>
        $<$<STREQUAL:${REALM_PLATFORM},Node>:REALM_PLATFORM_NODE=1>
        $<$<BOOL:${REALM_ENABLE_SYNC}>:REALM_ENABLE_SYNC>
)
target_include_directories(ObjectStore
    PUBLIC
       $<$<BOOL:${UV_INCLUDE_DIR}>:${UV_INCLUDE_DIR}>
       $<BUILD_INTERFACE:${RealmObjectStore_SOURCE_DIR}/src>
       $<INSTALL_INTERFACE:include> 
)
target_include_directories(ObjectStore
    PRIVATE
        ${RealmObjectStore_SOURCE_DIR}/external/pegtl
)
target_link_libraries(ObjectStore
    PUBLIC
        $<IF:$<BOOL:${REALM_ENABLE_SYNC}>,RealmSync::Sync,Realm::Core>
        $<$<BOOL:${UV_LIBRARY}>:${UV_LIBRARY}>
)

add_library(RealmObjectStore::ObjectStore ALIAS ObjectStore)

install(TARGETS ObjectStore EXPORT realm-object-store
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(
    DIRECTORY ./
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)
