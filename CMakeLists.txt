cmake_minimum_required(VERSION 3.20)
project(FenceInsertion)

find_package(LLVM REQUIRED CONFIG)

if("${LLVM_VERSION_MAJOR}" VERSION_LESS 19)
  message(FATAL_ERROR "Found LLVM ${LLVM_VERSION_MAJOR}, but need LLVM 19 or above")
endif()

list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}")
include(AddLLVM)
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_STANDARD 17)
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

add_library(FenceInsertionPass MODULE FenceInsertionPass.cpp)

target_link_libraries(FenceInsertionPass
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")