# Consumer of an installed hf-se050-driver: find_package() and the exported hf::se050 target only.
cmake_minimum_required(VERSION 3.16)
project(hf_se050_install_consumer LANGUAGES CXX)
find_package(hf_se050 0.3 REQUIRED CONFIG)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE hf::se050)
