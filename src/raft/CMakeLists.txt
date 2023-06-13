option(RAFT_BETA "RAFT_BETA" ON)
option(RAFT_EXAMPLE "RAFT_EXAMPLE" OFF)

set(RAFT_DIR ${PROJECT_SOURCE_DIR}/include/raft/)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/raft RAFT_SRC)

add_custom_command(
        OUTPUT raw_header
        COMMAND jrpcstub
        ARGS -o -i ${CMAKE_CURRENT_SOURCE_DIR}/spec.json
        MAIN_DEPENDENCY spec.json
        DEPENDS jrpcstub
        COMMENT "Generating Server/Client Stub..."
        VERBATIM
)

set(stub_dir ${PROJECT_BINARY_DIR}/src/raft)

add_custom_command(
        OUTPUT RaftServiceStub.h RaftClientStub.h
        COMMAND ${CMAKE_STUB_FORMATTER}
        ARGS -i ${stub_dir}/RaftServiceStub.h ${stub_dir}/RaftClientStub.h
        DEPENDS raw_header
        COMMENT "clang format Stub..."
        VERBATIM
)

IF(RAFT_BETA)
    add_library(RAFT_BETA STATIC ${RAFT_SRC} ${RAFT_DIR})
    target_include_directories(RAFT_BETA PUBLIC ${RAFT_DIR} ${STORAGE_DIR} ${INCLUDE_DIR})
    # add_executable(raftTest ${PROJECT_SOURCE_DIR}/raftTest.cpp)
    target_link_libraries(RAFT_BETA KV_STORAGE_BETA jrpc)
ENDIF()

IF(RAFT_EXAMPLE)
    add_library(RAFT_EXAMPLE STATIC ${RAFT_SRC} ${RAFT_DIR})
    add_dependencies(RAFT_EXAMPLE raw_header)
    target_include_directories(RAFT_EXAMPLE PUBLIC ${RAFT_DIR} ${STORAGE_DIR} ${INCLUDE_DIR})
    set_target_properties((RAFT_EXAMPLE PROPERTIES LINKER_LANGUAGE CXX))
    # add_executable(raftTest ${PROJECT_SOURCE_DIR}/raftTest.cpp)
    target_link_libraries(RAFT_EXAMPLE KV_STORAGE_BETA jrpc)
ENDIF()
