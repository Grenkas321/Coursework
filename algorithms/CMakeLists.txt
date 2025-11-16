cmake_minimum_required(VERSION 3.6)
project(algorithms)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PARENT_DIRECTORY}/include)
include_directories(${PARENT_DIRECTORY}/json)
include_directories(${PARENT_DIRECTORY}/additionals)

file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
add_library(algorithms ${SOURCES})

get_target_property(ALGORITHMS_INCLUDE_DIRS ${PROJECT_NAME} INCLUDE_DIRECTORIES)
set(ALGORITHMS_INCLUDE_DIRS ${ALGORITHMS_INCLUDE_DIRS} PARENT_SCOPE)
set(ALGORITHMS_LIBRARIES ${PROJECT_NAME} PARENT_SCOPE)
