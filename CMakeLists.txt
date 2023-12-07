cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/files.cmake)

idf_component_register(
SRCS
    ${libramfs_SRC}
INCLUDE_DIRS
    ${libramfs_INC}
PRIV_REQUIRES
    vfs
)
