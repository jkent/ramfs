cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/files.cmake)

add_library(ramfs
    ${libramfs_SRC}
)

target_include_directories(ramfs
PUBLIC
    ${libramfs_INC}
)

get_cmake_property(_vars VARIABLES)
list(SORT _vars)
foreach(_var ${_vars})
    unset(MATCHED)
    string(REGEX MATCH "^CONFIG_" MATCHED ${_var})
    if(NOT MATCHED)
        continue()
    endif()
    if("${${_var}}" STREQUAL "y")
        target_compile_definitions(ramfs PUBLIC "${_var}=1")
    else()
        target_compile_definitions(ramfs PUBLIC "${_var}=${${_var}}")
    endif()
endforeach()
