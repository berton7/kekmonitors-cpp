set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(ExternalProject)

ExternalProject_Add(spdlog
    PREFIX spdlog
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.8.5
    GIT_SHALLOW True
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DCMAKE_INSTALL_PREFIX=${STAGING_DIR}
    -DSPDLOG_BUILD_SHARED=OFF
	INSTALL_COMMAND ""
)

include_directories(${PROJECT_SOURCE_DIR}/deps/include)
include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(Boost 1.66.0 REQUIRED COMPONENTS system filesystem)

add_library(kekmonitors STATIC monitormanager.cpp comms/msg.cpp comms/server.cpp utils.cpp config.cpp)
add_dependencies(kekmonitors spdlog)

add_executable(moman moman.cpp)
target_link_libraries(moman pthread Boost::system Boost::filesystem kekmonitors)

add_executable(stopmm stopmm.cpp)
target_link_libraries(stopmm pthread Boost::system Boost::filesystem kekmonitors)

add_executable(cli moman_cli.cpp)
target_link_libraries(cli pthread Boost::system Boost::filesystem kekmonitors)
