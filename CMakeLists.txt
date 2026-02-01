cmake_minimum_required(VERSION 3.0)
project(main)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# configure compilation options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# subdirectories to be searched
add_subdirectory(testmuduo)

# configure executable file output directory
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# configure header file search path
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/server)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)
include_directories(${PROJECT_SOURCE_DIR}/include/server/db)
include_directories(${PROJECT_SOURCE_DIR}/include/server/model)
include_directories(${PROJECT_SOURCE_DIR}/include/server/redis)

include_directories(/usr/include/openssl)

# load subdirectories
add_subdirectory(src)

