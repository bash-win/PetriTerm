cmake_minimum_required(VERSION 3.20)
project(petriterm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
find_library(PANELW_LIBRARY NAMES panelw REQUIRED)

add_library(petriterm_core STATIC
    src/engine/TerminalWindow.cpp
    src/engine/ColorPalette.cpp
    src/engine/Renderer.cpp
    src/engine/InputManager.cpp
    src/engine/RandomNumberGenerator.cpp
    src/engine/SceneManager.cpp
    src/engine/GameLoop.cpp
    src/world/NoiseGenerator.cpp
    src/world/Biome.cpp
    src/world/WorldGrid.cpp
    src/world/WorldGenerator.cpp
    src/game/Viewport.cpp
)
target_include_directories(petriterm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(petriterm_core SYSTEM PUBLIC
    ${CURSES_INCLUDE_DIRS}
)
target_link_libraries(petriterm_core PUBLIC
    ${CURSES_LIBRARIES}
    ${PANELW_LIBRARY}
)

add_executable(petriterm src/main.cpp)
target_link_libraries(petriterm PRIVATE petriterm_core)

set(PETRITERM_SPECIES_FILE ${CMAKE_CURRENT_SOURCE_DIR}/data/species.txt)
if(EXISTS ${PETRITERM_SPECIES_FILE})
    add_custom_command(TARGET petriterm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PETRITERM_SPECIES_FILE}
            $<TARGET_FILE_DIR:petriterm>/species.txt
        COMMENT "Copying species.txt next to the petriterm binary")
endif()

file(GLOB PETRITERM_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
if(PETRITERM_TEST_SOURCES)
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.2
        )
        FetchContent_MakeAvailable(Catch2)
    endif()
    add_executable(petriterm_tests ${PETRITERM_TEST_SOURCES})
    target_link_libraries(petriterm_tests PRIVATE petriterm_core Catch2::Catch2WithMain)
    enable_testing()
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(petriterm_tests)
endif()
