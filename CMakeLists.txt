cmake_minimum_required(VERSION 3.13)

set(CMAKE_DEBUG_POSTFIX _d)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED true)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

project(Orxata VERSION 0.0.1)

add_library(orxata OBJECT)
set_target_properties(orxata PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

target_sources(orxata PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/orxata.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene.c
)

target_include_directories(orxata PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/llulu/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/spine/spine-c/include
)

target_compile_options(orxata PRIVATE
    -Wall
)

add_compile_definitions(
    $<$<CONFIG:Debug>:ORX_DEBUG>
    $<$<CONFIG:Debug>:ORX_VERBOSE>
)

if (WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(extern)
add_subdirectory(test)
