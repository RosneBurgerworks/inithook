target_sources(cathook PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/CreateMove.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/DispatchUserMessage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/GetFriendPersonaName.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/GetUserCmd.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/LevelInit.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/LevelShutdown.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/nographics.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Paint.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SendNetMsg.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Shutdown.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/IsPlayingTimeDemo.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/HookTools.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SendDatagram.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/RunCommand.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Think.cpp")

if(EnableVisuals)
    add_subdirectory(visual)
else()
    add_subdirectory(textmode)
endif()
