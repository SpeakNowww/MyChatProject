cmake_minimum_required(VERSION 3.0)
# project(main)
# configure compilation options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# configure header file search path
# include_directories()

# configure library file search path
# link_directories()

# set sources files to be compiled
set(SRC_LIST ./testmuduoserver.cpp)

# set executable file output directory
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# or use the following command to add all source files in the current directory
# aux_source_directory(. SRC_LIST)

# generate executable file from source files in SRC_LIST
add_executable(server ${SRC_LIST})

# link libraries to the executable file
target_link_libraries(server muduo_net muduo_base pthread)