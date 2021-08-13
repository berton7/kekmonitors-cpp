#!/bin/bash

mongoc_version="1.18.0"
mongocxx_version="r3.6.5"

if [[ -z "${CMAKE_GENERATOR}" ]]; then
	LOCAL_CMAKE_GENERATOR=make
else
	LOCAL_CMAKE_GENERATOR="${CMAKE_GENERATOR}"
fi

if [[ -z "${CMAKE_INSTALL_PREFIX}" ]]; then
	LOCAL_CMAKE_INSTALL_PREFIX=/usr/local
else
	LOCAL_CMAKE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}"
fi

cd deps

[ ! -d "mongo-c-driver" ] && git clone https://github.com/mongodb/mongo-c-driver.git --depth 1 -b $mongoc_version
[ ! -d "mongo-cxx-driver" ] && git clone https://github.com/mongodb/mongo-cxx-driver.git --depth 1 -b $mongocxx_version

cd mongo-c-driver/build
cmake .. -DCMAKE_INSTALL_PREFIX=${LOCAL_CMAKE_INSTALL_PREFIX} -DCMAKE_BUILD_TYPE=Release -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF -DENABLE_STATIC=ON
cmake --build . -j `nproc`
sudo ${LOCAL_CMAKE_GENERATOR,,} install

cd ../../mongo-cxx-driver/build
cmake .. -DCMAKE_INSTALL_PREFIX=${LOCAL_CMAKE_INSTALL_PREFIX} -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DBUILD_SHARED_AND_STATIC_LIBS=On -DBUILD_SHARED_LIBS_WITH_STATIC_MONGOC=On
cmake --build . -j `nproc`
sudo ${LOCAL_CMAKE_GENERATOR,,} install
