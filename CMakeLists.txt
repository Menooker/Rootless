cmake_minimum_required(VERSION 3.5)
project(Rootless)
set(LIB_NAME Rootless)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -fPIC")
add_subdirectory(${PROJECT_SOURCE_DIR}/3rdparty/PFishHook)
file(GLOB_RECURSE RL_SOURCES
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    )
include_directories(
    ${PROJECT_SOURCE_DIR}/3rdparty/PFishHook
    ${PROJECT_SOURCE_DIR}/
    )

add_library(${LIB_NAME} SHARED src/hooks.cpp)
target_link_libraries(${LIB_NAME} PFishHook Zydis dl)
add_dependencies(${LIB_NAME} PFishHook Zydis)