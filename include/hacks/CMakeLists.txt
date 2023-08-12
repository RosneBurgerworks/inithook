target_include_directories(cathook PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

if(EnableVisuals)
    add_subdirectory(visual)
endif()
