install(TARGETS ${info_binary_id} RUNTIME DESTINATION bin)

if (WITH_SFOS_HARBOUR)
    install(FILES ${sfos_dir}/${info_binary_id}-harbour.desktop DESTINATION share/applications RENAME ${info_binary_id}.desktop)
else()
    install(FILES ${sfos_dir}/${info_binary_id}.desktop DESTINATION share/applications)
    install(FILES ${sfos_dir}/${info_binary_id}-open-url.desktop DESTINATION share/applications)
    install(FILES ${sfos_dir}/sailjail/Jupii.permission DESTINATION /etc/sailjail/permissions)
    install(FILES ${sfos_dir}/sailjail/JupiiGpodder.permission DESTINATION /etc/sailjail/permissions)
endif()

if(BUILD_PYTHON_MODULE)
    install(FILES ${PROJECT_BINARY_DIR}/python.tar.xz DESTINATION share/${info_binary_id}/lib)
endif()

install(FILES ${sfos_dir}/icons/86x86/${info_binary_id}.png DESTINATION share/icons/hicolor/86x86/apps)
install(FILES ${sfos_dir}/icons/108x108/${info_binary_id}.png DESTINATION share/icons/hicolor/108x108/apps)
install(FILES ${sfos_dir}/icons/128x128/${info_binary_id}.png DESTINATION share/icons/hicolor/128x128/apps)
install(FILES ${sfos_dir}/icons/172x172/${info_binary_id}.png DESTINATION share/icons/hicolor/172x172/apps)

install(FILES ${qm_files} DESTINATION share/${info_binary_id}/translations)
install(DIRECTORY ${sfos_dir}/qml DESTINATION share/${info_binary_id})
install(DIRECTORY ${sfos_dir}/images DESTINATION share/${info_binary_id})
