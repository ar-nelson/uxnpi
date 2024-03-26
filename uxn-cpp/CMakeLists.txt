cmake_minimum_required(VERSION 3.5)

project(uxncpp VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

# Export metadata for clangd, even in a nix-shell.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

find_package(SDL2 REQUIRED)

add_library(uxn uxn.cpp)
target_compile_options(uxn PRIVATE -fno-exceptions)
target_link_options(uxn PRIVATE -nostdlib)

add_executable(uxn_sdl stdlib_filesystem.cpp sdl_varvara.cpp)
target_compile_options(uxn_sdl PUBLIC -fno-omit-frame-pointer -fno-exceptions -fsanitize=address,undefined)
target_link_options(uxn_sdl PUBLIC -fsanitize=address,undefined)
target_link_libraries(uxn_sdl PUBLIC uxn SDL2::SDL2-static)

