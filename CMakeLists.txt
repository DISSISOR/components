cmake_minimum_required(VERSION 3.20)
project(componentsProbe CXX)

set(CMAKE_CXX_STANDARD 20)

set( srcs
    src/main.cc
    src/PodArray.cc
    src/World.cc
)
include_directories(inc)

set(SDL3_VERSION 3.0)
find_package(SDL3 ${SDL3_VERSION} REQUIRED)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Werror -Wno-deprecated-declarations)
endif()

add_executable(main ${srcs})
target_link_libraries(main SDL3::SDL3 m)

