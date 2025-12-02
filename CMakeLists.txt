cmake_minimum_required(VERSION 3.22)

project(SchedulingProblem VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)

# if you install boost in specific directory then specify path to boost
# set(BOOST_ROOT "path to boost")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
  # set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -pg -Wno-reorder")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Wno-reorder")

set(PARENT_DIRECTORY ${PROJECT_SOURCE_DIR})

find_package(Boost 1.70 REQUIRED COMPONENTS program_options)
file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/algorithms)
include_directories(${PROJECT_SOURCE_DIR}/additionals)
include_directories(${PROJECT_SOURCE_DIR}/experiments)
#include_directories(${PROJECT_SOURCE_DIR}/algorithms/spg_schedule)

add_subdirectory(algorithms)
add_subdirectory(additionals)
add_subdirectory(experiments)
#include_directories(algorithms/spg_schedule)

include_directories(${ALGORITHMS_INCLUDE_DIRS}
                    ${ADDITIONALS_INCLUDE_DIRS}
                    ${EXPERIMENTS_INCLUDE_DIRS})
add_executable(application main.cpp ${SOURCES})
target_link_libraries(application
                      ${ALGORITHMS_LIBRARIES}
                      ${ADDITIONALS_LIBRARIES}
                      ${EXPERIMENTS_LIBRARIES}
                      Boost::program_options
                      pthread)
