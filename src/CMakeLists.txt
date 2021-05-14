set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(KEKMONITORS_SHARED_LIBS "Build kekmonitors shared or static" OFF)

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
    -DSPDLOG_MASTER_PROJECT=OFF
    -DSPDLOG_BUILD_SHARED=OFF
	INSTALL_COMMAND ""
)

include_directories(${PROJECT_SOURCE_DIR}/deps/include)
include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(Boost 1.66.0 REQUIRED COMPONENTS system filesystem)

set(KEKMONITORS_SOURCE lib/monitormanager.cpp lib/msg.cpp lib/server.cpp lib/utils.cpp lib/config.cpp lib/core.cpp)
set(KEKMONITORS_LIB_DEPS pthread Boost::system Boost::filesystem kekmonitors)

if (KEKMONITORS_SHARED_LIBS)
	add_library(kekmonitors SHARED ${KEKMONITORS_SOURCE})
else()
	add_library(kekmonitors STATIC ${KEKMONITORS_SOURCE})
endif()
add_dependencies(kekmonitors spdlog)

add_executable(moman bin/moman.cpp)
target_link_libraries(moman ${KEKMONITORS_LIB_DEPS})

add_executable(stopmm bin/stopmm.cpp)
target_link_libraries(stopmm ${KEKMONITORS_LIB_DEPS})

add_executable(cli bin/moman_cli.cpp)
target_link_libraries(cli ${KEKMONITORS_LIB_DEPS})
