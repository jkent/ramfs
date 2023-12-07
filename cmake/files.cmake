get_filename_component(ramfs_DIR ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE CACHE)

set(libramfs_SRC
    ${ramfs_DIR}/src/ramfs.c
)

set(libramfs_INC
    ${ramfs_DIR}/include
)

if(ESP_PLATFORM)
    list(APPEND libramfs_SRC
        ${ramfs_DIR}/src/vfs.c
    )
endif()
