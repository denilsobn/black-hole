cmake_minimum_required(VERSION 3.15)
project(buraco_negro_moderno CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de build" FORCE)
endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

add_executable(buraco_negro
    src/main.cpp
    src/shader.h
    src/png_simples.h)

target_link_libraries(buraco_negro PRIVATE glfw glad::glad)

target_compile_definitions(buraco_negro PRIVATE
    SHADER_SOURCE_DIR="${CMAKE_SOURCE_DIR}/shaders")

add_custom_command(TARGET buraco_negro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:buraco_negro>/shaders
    COMMENT "Copiando shaders para o lado do executavel")
