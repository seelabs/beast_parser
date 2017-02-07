cmake_minimum_required(VERSION 3.5.0)

if (DEFINED ENV{NIH})
  set(NIH $ENV{NIH})
else()
  set(NIH $ENV{HOME}/projs/nih)
endif()

set(CMAKE_MODULE_PATH "${NIH}/cmake/module")
include(CMakeFuncs)

set(CMAKE_CXX_STANDARD 14)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

# server
append_boost_libs(BOOST_LINK_LIBS filesystem program_options system)
add_executable(server src/server/parse_bug.cpp)
target_link_libraries(server ${BOOST_LINK_LIBS} Threads::Threads)
nih(server boost beast)
target_include_directories(server PUBLIC src ${NIH}/beast/extras)
