get_filename_component(ramfs_DIR ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE CACHE)

set(libramfs_rbtree_SRC
    ${ramfs_DIR}/src/ramfs_rbtree.c
    ${ramfs_DIR}/src/rbtree.c
)

set(libramfs_vector_SRC
    ${ramfs_DIR}/src/ramfs_vector.c
)

if(CONFIG_RAMFS_USE_RBTREE STREQUAL "y")
    set(libramfs_SRC ${libramfs_rbtree_SRC})
else()
    set(libramfs_SRC ${libramfs_vector_SRC})
endif()

set(libramfs_INC
    ${ramfs_DIR}/include
)

if(ESP_PLATFORM)
    list(APPEND libramfs_SRC
        ${ramfs_DIR}/src/vfs.c
    )
endif()
