add_executable(tpc_test_frame test_frame.cpp)
target_link_libraries(tpc_test_frame PRIVATE TPC::TPC)

add_test(NAME tpc.test_frame COMMAND tpc_test_frame)
