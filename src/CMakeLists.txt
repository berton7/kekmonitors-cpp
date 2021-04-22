set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(${PROJECT_SOURCE_DIR}/deps/include)
include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(moman moman.cpp comms/msg.cpp comms/server.cpp ../include/kekmonitors/utils.hpp)
target_link_libraries(moman pthread)
