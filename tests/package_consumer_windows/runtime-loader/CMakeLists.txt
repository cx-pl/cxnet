cmake_minimum_required(VERSION 3.31)
project(CxPackageRuntimeLoader C)

add_executable(CxPackageRuntimeLoader load.c)
target_compile_features(CxPackageRuntimeLoader PRIVATE c_std_11)
