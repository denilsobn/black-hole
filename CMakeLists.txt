cmake_minimum_required(VERSION 3.15)
project(buraco_negro_moderno CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sem isso um build sem -DCMAKE_BUILD_TYPE sai sem otimizacao, o que
# arruina qualquer medicao de FPS para o artigo.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de build" FORCE)
endif()

# Ambos instalados via vcpkg (veja README.md):
#   vcpkg install glfw3 glad
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

add_executable(buraco_negro
    src/main.cpp
    src/shader.h
    src/png_simples.h)

target_link_libraries(buraco_negro PRIVATE glfw glad::glad)

# Caminho da pasta shaders/ do CODIGO-FONTE. O programa prefere esse
# caminho quando ele existe, o que faz o recarregamento a quente (F5)
# pegar o arquivo que voce acabou de salvar -- sem recompilar nada.
# Se a pasta nao existir (build copiado para outra maquina), o programa
# cai automaticamente para a copia que fica ao lado do .exe.
target_compile_definitions(buraco_negro PRIVATE
    SHADER_SOURCE_DIR="${CMAKE_SOURCE_DIR}/shaders")

# Copia a pasta de shaders para perto do .exe apos cada build, para que
# o executavel funcione tambem fora da arvore de codigo.
add_custom_command(TARGET buraco_negro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:buraco_negro>/shaders
    COMMENT "Copiando shaders para o lado do executavel")
