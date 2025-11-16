cmake_minimum_required(VERSION 3.6)
project(additionals)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PARENT_DIRECTORY}/include)
include_directories(${PARENT_DIRECTORY}/json)

file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
add_library(additionals ${SOURCES})

get_target_property(ADDITIONALS_INCLUDE_DIRS ${PROJECT_NAME} INCLUDE_DIRECTORIES)
set(ADDITIONALS_INCLUDE_DIRS ${ADDITIONALS_INCLUDE_DIRS} PARENT_SCOPE)
set(ADDITIONALS_LIBRARIES ${PROJECT_NAME} PARENT_SCOPE)
