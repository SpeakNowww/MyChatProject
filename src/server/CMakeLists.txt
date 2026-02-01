# add all source files in the current directory to SRC_LIST
aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_LIST)
aux_source_directory(./model MODEL_LIST)
aux_source_directory(./redis REDIS_LIST)
# specify the source files for the server
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST} ${REDIS_LIST})

# specify the libraries to be linked to the executable file
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread ssl crypto sqlite3) 


