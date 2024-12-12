add_executable(test_fsm 
    main.c
    ${TEST_SOURCES}
    ${TEST_HEADERS})
target_include_directories(test_fsm PUBLIC ${TEST_INCLUDE_DIRS})

add_test(test_fsm, test_fsm)