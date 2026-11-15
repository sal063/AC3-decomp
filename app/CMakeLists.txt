add_library(ac3_play STATIC play.cpp)
target_include_directories(ac3_play PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ac3_play PUBLIC ac3_warnings ac3_core ac3_sim ac3_machine ac3_platform ac3_render ac3_gamedata ac3_vfs)

add_executable(ac3engine main.cpp)
target_link_libraries(ac3engine PRIVATE ac3_play)
ac3_copy_runtime(ac3engine)
