
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.18.1)

project("kernelsu")

add_library(kernelsu
        SHARED
        jni.cc
        ksu.cc
        )

find_library(log-lib log)

target_link_libraries(kernelsu ${log-lib})