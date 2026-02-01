aux_source_directory(. SRC_LIST)
set(SERVER_COMMON_SRC ${PROJECT_SOURCE_DIR}/src/server/encrypt.cpp 
${PROJECT_SOURCE_DIR}/src/server/model/chathistory.cpp)
# 指定生成可执行文件
add_executable(ChatClient ${SRC_LIST} ${SERVER_COMMON_SRC})
# 指定可执行文件链接时需要依赖的库文件
target_link_libraries(ChatClient pthread ssl crypto sqlite3)