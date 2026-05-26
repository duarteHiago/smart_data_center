cmake_minimum_required(VERSION 3.20)
project(smart_data_center C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --------------------------------------------------------------------------
# Configurações de compilador (MSYS2 MinGW64)
# --------------------------------------------------------------------------
if(MINGW)
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -O2
        -ffast-math
        -msse2
    )
    # Subsistema Windows para evitar janela de console no release
    # Mantenha comentado durante debug para ver saída de stderr
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
endif()

# --------------------------------------------------------------------------
# Raylib — encontra a instalação local do MSYS2 MinGW64
# Caminho padrão: C:/msys64/mingw64
# --------------------------------------------------------------------------
set(RAYLIB_ROOT "C:/msys64/mingw64" CACHE PATH "Raiz do MinGW64 com Raylib")

find_library(RAYLIB_LIB
    NAMES raylib
    PATHS "${RAYLIB_ROOT}/lib"
    NO_DEFAULT_PATH
    REQUIRED
)

find_path(RAYLIB_INCLUDE
    NAMES raylib.h
    PATHS "${RAYLIB_ROOT}/include"
    NO_DEFAULT_PATH
    REQUIRED
)

# raygui é header-only; precisamos do diretório
find_path(RAYGUI_INCLUDE
    NAMES raygui.h
    PATHS
        "${RAYLIB_ROOT}/include"
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/include"
    NO_DEFAULT_PATH
)

if(NOT RAYGUI_INCLUDE)
    # Fallback: usa o mesmo diretório do raylib.h
    set(RAYGUI_INCLUDE "${RAYLIB_INCLUDE}")
    message(STATUS "raygui.h não encontrado separadamente — usando ${RAYGUI_INCLUDE}")
endif()

# --------------------------------------------------------------------------
# Fontes do projeto
# --------------------------------------------------------------------------
set(SOURCES
    main.c
    fuzzy.c
    sensors.c
    arduino.c
    scene.c
    rack.c
    airflow.c
    signals.c
    renderer.c
    hud.c
    inspector.c
    mlp.c
    compare.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${RAYLIB_INCLUDE}"
    "${RAYGUI_INCLUDE}"
    "${CMAKE_SOURCE_DIR}"
)

# --------------------------------------------------------------------------
# Link com Raylib e dependências do Windows/MinGW
# --------------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
    "${RAYLIB_LIB}"
    opengl32
    gdi32
    winmm
    m
)

# Copia o executável para a raiz do projeto após build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PROJECT_NAME}>
        "${CMAKE_SOURCE_DIR}/${PROJECT_NAME}.exe"
    COMMENT "Copiando executavel para a raiz do projeto"
)

message(STATUS "Raylib: ${RAYLIB_LIB}")
message(STATUS "Include: ${RAYLIB_INCLUDE}")
message(STATUS "raygui:  ${RAYGUI_INCLUDE}")
