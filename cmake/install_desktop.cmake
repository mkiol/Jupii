install(TARGETS ${info_binary_id} RUNTIME DESTINATION bin)

configure_file(${desktop_dir}/${info_id}.desktop.in ${info_app_icon_id}.desktop)
install(FILES ${info_app_icon_id}.desktop DESTINATION share/applications)

configure_file(${desktop_dir}/${info_id}.metainfo.xml.in ${info_app_icon_id}.metainfo.xml)
install(FILES ${info_app_icon_id}.metainfo.xml DESTINATION share/metainfo)

install(FILES "${desktop_dir}/${info_id}.svg" DESTINATION share/icons/hicolor/scalable/apps)
install(FILES "${desktop_dir}/icons/16x16/${info_id}.png" DESTINATION share/icons/hicolor/16x16/apps)
install(FILES "${desktop_dir}/icons/32x32/${info_id}.png" DESTINATION share/icons/hicolor/32x32/apps)
install(FILES "${desktop_dir}/icons/48x48/${info_id}.png" DESTINATION share/icons/hicolor/48x48/apps)
install(FILES "${desktop_dir}/icons/64x64/${info_id}.png" DESTINATION share/icons/hicolor/64x64/apps)
install(FILES "${desktop_dir}/icons/96x96/${info_id}.png" DESTINATION share/icons/hicolor/96x96/apps)
install(FILES "${desktop_dir}/icons/128x128/${info_id}.png" DESTINATION share/icons/hicolor/128x128/apps)
install(FILES "${desktop_dir}/icons/256x256/${info_id}.png" DESTINATION share/icons/hicolor/256x256/apps)
install(FILES "${desktop_dir}/icons/512x512/${info_id}.png" DESTINATION share/icons/hicolor/512x512/apps)
