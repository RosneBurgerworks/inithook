target_sources(cathook PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/atlas.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/colors.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/drawing.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/drawmgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/EffectGlow.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/picopng.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SDLHooks.cpp")

if(EnableGUI)
    add_subdirectory(menu)
endif()

if (EnableImGuiDrawing)
    add_subdirectory(imgui)
endif()
