cmake_minimum_required(VERSION 3.6)
project(experiments)

include_directories(${PARENT_DIRECTORY}/include)
include_directories(${PARENT_DIRECTORY}/algorithms)
include_directories(${PARENT_DIRECTORY}/additionals)
include_directories(${PARENT_DIRECTORY}/json)


file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
add_library(experiments STATIC ${SOURCES})
target_link_libraries(experiments PUBLIC additionals algorithms)
get_target_property(EXPERIMENTS_INCLUDE_DIRS ${PROJECT_NAME} INCLUDE_DIRECTORIES)
set(EXPERIMENTS_INCLUDE_DIRS ${EXPERIMENTS_INCLUDE_DIRS} PARENT_SCOPE)
set(EXPERIMENTS_LIBRARIES ${PROJECT_NAME} PARENT_SCOPE)
