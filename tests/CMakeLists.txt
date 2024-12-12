
set(TEST_SOURCES 
    ${PROJECT_DIR}/lib/fsm.c
    # ${PROJECT_DIR}/lib/AddClass.cpp 
    # ${PROJECT_DIR}/tests/checker.cpp
    )

set(TEST_HEADERS 
    ${PROJECT_DIR}/lib/fsm.h 
    # ${PROJECT_DIR}/src/MultiplyClass.hpp 
    # ${PROJECT_DIR}/tests/checker.hpp
    )

set(TEST_INCLUDE_DIRS ${PROJECT_DIR}/lib ${PROJECT_DIR}/tests)

message("Tests sources: ${TEST_SOURCES}")
message("Tests headers: ${TEST_HEADERS}")
message("Tests include dirs: ${TEST_INCLUDE_DIRS}")


add_subdirectory(test_fsm)
# add_subdirectory(test_multiply)