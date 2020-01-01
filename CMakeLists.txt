cmake_minimum_required(VERSION 3.5)

project(REDICON VERSION 0.4.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)

# Set default build type to release.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

# Initialize CXXFLAGS. 
set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE        "-O4 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

# Build all examples.
option(BUILD_EXAMPLES "Build all examples." ON)

# Build all tests.
option(BUILD_TESTS "Build all tests." ON)

# Compiler-specific C++11 activation.
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
else ()
  message(FATAL_ERROR "Your C++ compiler does not support C++11.")
endif ()


# Boost is required.
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED COMPONENTS system filesystem date_time regex)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
  message(STATUS "Boost libs: " ${Boost_LIBRARIES})
endif()

# OpenSSL is required.
set(OPENSSL_USE_STATIC_LIBS OFF)
set(OPENSSL_MSVC_STATIC_RT ON)
find_package(OpenSSL)
if(OPENSSL_FOUND)
  include_directories(${OPENSSL_INCLUDE_DIR})
  message(STATUS "OpenSSL libs: " ${OPENSSL_LIBRARIES})
else()
  set(OPENSSL_USE_STATIC_LIBS ON)
  set(OPENSSL_MSVC_STATIC_RT OFF)
  find_package(OpenSSL)
  if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "OpenSSL libs: " ${OPENSSL_LIBRARIES})
  endif()
endif()

# Threads is required.
find_package(Threads REQUIRED)


# Include header files.
include_directories(include)

# Create redicon static library.
add_library(redicon src/redicon.cc) # add_library(redicon SHARED src/redicon.cc)
target_link_libraries(redicon ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

# Define redicon library's compile time log level.
target_compile_definitions(redicon PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO)

# Set third_party directory.
set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)

# Include pugixml header files.
set(PUGIXML_DIR ${THIRD_PARTY_DIR}/pugixml/src)
include_directories(${PUGIXML_DIR})

# Create pugixml static library
set(PUGI_HEADERS ${PUGIXML_DIR}/pugixml.hpp ${PUGIXML_DIR}/pugiconfig.hpp)
set(PUGI_SOURCES ${PUGI_HEADERS} ${PUGIXML_DIR}/pugixml.cpp)
add_library(pugixml STATIC ${PUGI_SOURCES})

# Include spdlog header files.
include_directories(${THIRD_PARTY_DIR}/spdlog/include)

# Include googletest header files.
set(GTEST_DIR ${THIRD_PARTY_DIR}/googletest/googletest)
include_directories(${GTEST_DIR}/include)

# Create googletest shared library
add_library(gtest SHARED ${GTEST_DIR}/src/gtest-all.cc)
target_include_directories(gtest PRIVATE ${GTEST_DIR})
target_link_libraries(gtest ${CMAKE_THREAD_LIBS_INIT})

# Build examples.
if (BUILD_EXAMPLES)
  add_executable(hello examples/hello.cc)
  target_link_libraries(hello redicon)

  add_executable(hello_sync examples/hello_sync.cc)
  target_link_libraries(hello_sync redicon)

  add_executable(benchmark_async_multi examples/benchmark_async_multi.cc)
  target_link_libraries(benchmark_async_multi redicon)

  add_executable(benchmark_async examples/benchmark_async.cc)
  target_link_libraries(benchmark_async redicon)

  add_executable(benchmark_sync examples/benchmark_sync.cc)
  target_link_libraries(benchmark_sync redicon)

  add_executable(benchmark_sync_multi examples/benchmark_sync_multi.cc)
  target_link_libraries(benchmark_sync_multi redicon)
endif()

# Build tests.
if (BUILD_TESTS)
  add_executable(test_async tests/test_async.cc)
  target_link_libraries(test_async redicon gtest)

  add_executable(test_sync tests/test_sync.cc)
  target_link_libraries(test_sync redicon gtest)
endif()
