cmake_minimum_required(VERSION 3.22)
project(kpp CXX)

set(CMAKE_CXX_STANDARD 26)

add_executable(kpp
  src/function.cpp
  src/interpreter.cpp
  src/lexer.cpp
  src/main.cpp
  src/object.cpp
  src/parser.cpp
  src/token.cpp)

target_include_directories(kpp
  PRIVATE
  include)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFI REQUIRED
  IMPORTED_TARGET libffi)

target_link_libraries(kpp
  PRIVATE
  PkgConfig::FFI)

install(TARGETS kpp
  RUNTIME DESTINATION bin)