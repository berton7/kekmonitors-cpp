set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(${PROJECT_SOURCE_DIR}/deps/include)
include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(Boost 1.66.0 REQUIRED COMPONENTS system filesystem)

add_executable(moman moman.cpp comms/msg.cpp comms/server.cpp utils.cpp config.cpp)
target_link_libraries(moman pthread Boost::system Boost::filesystem)

add_executable(stopmm stopmm.cpp comms/msg.cpp utils.cpp config.cpp)
target_link_libraries(stopmm pthread Boost::system Boost::filesystem)

add_executable(cli moman_cli.cpp comms/msg.cpp utils.cpp config.cpp)
target_link_libraries(cli pthread Boost::system Boost::filesystem)

