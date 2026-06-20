if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

cmake_minimum_required(VERSION 3.12)
project(navigator)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

find_package(glfw3 CONFIG QUIET)
if(NOT TARGET glfw)
    find_package(glfw3 QUIET)
endif()
if(NOT TARGET glfw)
    find_package(GLFW3 QUIET)
endif()
if(NOT TARGET glfw)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    FetchContent_MakeAvailable(glfw)
endif()
if(NOT TARGET glfw)
    message(FATAL_ERROR "GLFW3 was not found and the bundled FetchContent fallback did not produce the glfw target.")
endif()

find_package(magic_enum CONFIG QUIET)
find_path(MAGIC_ENUM_INCLUDE_DIR magic_enum/magic_enum.hpp)
if(NOT TARGET magic_enum::magic_enum AND NOT MAGIC_ENUM_INCLUDE_DIR)
    FetchContent_Declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
        GIT_TAG v0.9.7
    )
    FetchContent_MakeAvailable(magic_enum)
    if(EXISTS "${magic_enum_SOURCE_DIR}/include/magic_enum/magic_enum.hpp")
        set(MAGIC_ENUM_INCLUDE_DIR "${magic_enum_SOURCE_DIR}/include")
    endif()
endif()
if(NOT TARGET magic_enum::magic_enum AND NOT MAGIC_ENUM_INCLUDE_DIR)
    message(FATAL_ERROR "magic_enum was not found and the bundled FetchContent fallback did not provide headers.")
endif()

add_subdirectory(lib)

file(GLOB NAVIGATOR_SRC
    # NaviGator
    "src/*.cpp"
    "include/*.hpp"
    "include/*.h"
    
    "src/application/*.cpp"
    "include/application/*.hpp"
    
    "src/ui/*.cpp"
    "include/ui/*.hpp"

    "src/util/*.cpp"
    "include/util/*.hpp"

    "src/ubo/*.cpp"
    "include/ubo/*.hpp"

    "src/tracks/*.cpp"
    "include/tracks/*.hpp"

    "include/primitives/*.hpp"
    
    # glad
    "lib/glad/src/*.c"

    # ImGuiFileDialog
    "lib/ImGuiFileDialog/ImGuiFileDialog.cpp"
)

add_executable(navigator ${NAVIGATOR_SRC})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/asset DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(navigator PUBLIC include lib/glad/include lib/glm lib/ImGuiFileDialog lib/librdr3/include lib/Recast/include lib/pugixml/src lib/tracy/public/tracy)
if(MAGIC_ENUM_INCLUDE_DIR)
    target_include_directories(navigator PUBLIC ${MAGIC_ENUM_INCLUDE_DIR})
endif()

target_link_libraries(navigator PUBLIC glm imgui glfw librdr3 Recast pugixml)
if(TARGET magic_enum::magic_enum)
    target_link_libraries(navigator PUBLIC magic_enum::magic_enum)
endif()
