cmake_minimum_required(VERSION 3.20)
project(ImageProcessingServer LANGUAGES CXX VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Export compile_commands.json for tooling / clangd.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Dependencies ---------------------------------------------------------
# Drogon: provided by vcpkg manifest (see vcpkg.json) or a system install.
find_package(Drogon CONFIG REQUIRED)

# libvips: dynamically linked via pkg-config (vips-cpp.pc). libvips is
# LGPL-2.1+; dynamic linking keeps this MIT-licensed project compliant.
find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips-cpp)

# --- Target ---------------------------------------------------------------
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(image_server ${SERVER_SOURCES})

target_include_directories(image_server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(image_server PRIVATE Drogon::Drogon PkgConfig::VIPS)

if(NOT MSVC)
  target_compile_options(image_server PRIVATE -Wall -Wextra)
endif()

install(TARGETS image_server RUNTIME DESTINATION bin)
