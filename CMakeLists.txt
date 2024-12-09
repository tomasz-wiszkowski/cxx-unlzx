cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(UnLzx VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fexceptions")

set(SOURCES
    src/circular_buffer.cc
    src/crc.cc
    src/huffman_decoder.cc
    src/huffman_table.cc
    src/lzx_handle.cc
    src/mmap_buffer.cc
    src/unlzx.cc
)

set(HEADERS
    src/circular_buffer.hh
    src/crc.hh
    src/huffman_decoder.hh
    src/lzx_handle.hh
    src/mmap_buffer.hh
    src/types.hh
    src/unlzx.hh
)

add_library(unlzx_lib ${SOURCES} ${HEADERS})

# Add the main program source file
add_executable(unlzx src/unlzx_main.cc)
target_link_libraries(unlzx PRIVATE unlzx_lib)

# Enable testing
enable_testing()

find_package(GTest REQUIRED)
add_executable(unlzx_test
    src/circular_buffer_test.cc
    src/crc_test.cc
    src/mmap_buffer_test.cc
    src/unlzx_test.cc
)
target_include_directories(unlzx_test PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(unlzx_test PUBLIC GTest::GTest GTest::Main PRIVATE unlzx_lib)
add_test(NAME unlzx_test COMMAND unlzx_test)
