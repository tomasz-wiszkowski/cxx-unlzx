cmake_minimum_required(VERSION 4.00)

# Set the project name and version
project(UnLzx VERSION 1.0 LANGUAGES C CXX)

option(BUILD_TESTS "Whether to compile unittests" TRUE)
option(BUILD_EXECUTABLE "Whether to compile the executable binary" TRUE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(IS_DEBUG TRUE)
else()
    set(IS_DEBUG FALSE)
endif()

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
    string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    # 4201: unnamed bitfields
    # 4530: exception semantics used with disabled exceptions.
    add_compile_options(/EHs- /EHc- /W4 /wd4201 /wd4530)
else()
    add_compile_options(-fno-exceptions -fno-rtti -Wall -Wextra)
endif()

set(SOURCES
    src/crc.cc
    src/error.cc
    src/huffman_decoder.cc
    src/huffman_table.cc
    src/lzx_block.cc
    src/lzx_entry.cc
    src/lzx_entry_builder.cc
    src/lzx_handle.cc
    src/mmap_buffer.cc
    src/unlzx.cc
)

set(HEADERS
    src/crc.hh
    src/error.hh
    src/huffman_decoder.hh
    src/lzx_block.hh
    src/lzx_entry.hh
    src/lzx_entry_builder.hh
    src/lzx_handle.hh
    src/mmap_buffer.hh
    src/types.hh
    src/unlzx.hh
)

add_library(unlzx_lib ${SOURCES} ${HEADERS})

if (BUILD_EXECUTABLE) 
    # Add the main program source file
    add_executable(unlzx src/unlzx_main.cc)
    target_link_libraries(unlzx PRIVATE unlzx_lib)
endif()

if (BUILD_TESTS AND IS_DEBUG) 
    # Enable testing
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.14.0
      GIT_SHALLOW    TRUE
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    # Ensure gtest is compiled as a static library
    set(BUILD_SHARED_LIBS OFF)
    add_compile_definitions(GTEST_LINKED_AS_SHARED_LIBRARY=0)
    add_compile_definitions(GTEST_CREATE_SHARED_LIBRARY=0)
    FetchContent_MakeAvailable(googletest)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(gtest PRIVATE "-Wno-character-conversion")
        target_compile_options(gtest_main PRIVATE "-Wno-character-conversion")
    endif()

    add_executable(unlzx_test
        src/crc_test.cc
        src/mmap_buffer_test.cc
        src/unlzx_test.cc
    )
    target_link_libraries(unlzx_test PUBLIC gtest_main PRIVATE unlzx_lib)
    
    include(GoogleTest)
    gtest_discover_tests(unlzx_test)
endif()