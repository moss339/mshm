add_executable(server_test server_test.c)
target_link_libraries(server_test mshm)

add_executable(client_test client_test.c)
target_link_libraries(client_test mshm)

add_executable(multi_client_test multi_client_test.c)
target_link_libraries(multi_client_test mshm)

add_executable(unit_test unit_test.c)
target_link_libraries(unit_test mshm)

add_test(NAME producer_client_test
    COMMAND bash -c "./server_test & ./client_test; wait")

add_test(NAME unit_test
    COMMAND ./unit_test)
