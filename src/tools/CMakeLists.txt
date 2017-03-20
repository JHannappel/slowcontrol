add_executable(requestValueSetting requestValueSetting.cpp)
target_link_libraries(requestValueSetting slowcontrol_static ${OptionParser_LIBRARIES})

set(usr_local_bin "restart_daemons" "purge_videos")

foreach (SCRIPT ${usr_local_bin})
       configure_file(${SCRIPT}.in ${SCRIPT} @ONLY)
       install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${SCRIPT} DESTINATION /usr/local/bin PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endforeach(SCRIPT)
