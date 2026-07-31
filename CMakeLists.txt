cmake_minimum_required(VERSION 3.16)
project(x65 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

function(configure_x65_target target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${target} PRIVATE
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    )
    if(UNIX AND NOT APPLE)
        target_link_libraries(${target} PRIVATE m)
    endif()
    install(TARGETS ${target}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endfunction()

configure_x65_target(x65 x65.cpp)
configure_x65_target(dump_x65 dump_x65/dump_x65.cpp)

option(X65_BUILD_TESTS "Build the bundled regression tests" OFF)
if(X65_BUILD_TESTS)
    enable_testing()
    add_test(NAME x65_smoke COMMAND x65 test/AddrMode_65816.s -cpu=65816 -lst)
endif()
