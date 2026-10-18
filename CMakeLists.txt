cmake_minimum_required(VERSION 3.16)
project(TreeMakerHeadless C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
file(GLOB_RECURSE TM_MODEL_SOURCES ${SRC}/tmModel/*.cpp ${SRC}/tmModel/*.c)

# Headless build of the GUI-free model (TMWX intentionally undefined).
add_library(tmmodel STATIC ${TM_MODEL_SOURCES} ${SRC}/tmPrec.cpp)
target_include_directories(tmmodel PUBLIC
  ${SRC}
  ${SRC}/tmModel
  ${SRC}/tmModel/tmPtrClasses
  ${SRC}/tmModel/tmOptimizers
  ${SRC}/tmModel/tmSolvers
  ${SRC}/tmModel/tmNLCO
  ${SRC}/tmModel/tmTreeClasses
  ${SRC}/tmModel/wnlib/cmp
  ${SRC}/tmModel/wnlib/conjdir
  ${SRC}/tmModel/wnlib/cpy
  ${SRC}/tmModel/wnlib/list
  ${SRC}/tmModel/wnlib/low
  ${SRC}/tmModel/wnlib/mat
  ${SRC}/tmModel/wnlib/mem
  ${SRC}/tmModel/wnlib/random
  ${SRC}/tmModel/wnlib/vect)
# tmNLCO.h enables the ALM engine. libc++ drops std::mem_fun etc. in C++17;
# the model still uses them.
target_compile_definitions(tmmodel PUBLIC _LIBCPP_ENABLE_CXX17_REMOVED_BINDERS)
target_compile_options(tmmodel PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated>)

file(GLOB CLI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cli/*.cpp)
add_executable(treemaker-cli ${CLI_SOURCES})
target_link_libraries(treemaker-cli PRIVATE tmmodel)
