cmake_minimum_required(VERSION 3.2)
project(shared_ptr)

set(CMAKE_CXX_STANDARD 23)

add_custom_target(static_analysis
        COMMAND ${CMAKE_SOURCE_DIR}/codestyle_check.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running static code analysis..."
)

# For sanitizers
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined,leak -fno-sanitize-recover=all -g -lm")

# For Valgrind
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lm")

add_executable(default_run main.cpp)