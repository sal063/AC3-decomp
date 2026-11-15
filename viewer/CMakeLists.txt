add_executable(ac3viewer main.cpp scripts.cpp ui.cpp)
target_link_libraries(ac3viewer PRIVATE ac3_play)
ac3_copy_runtime(ac3viewer)
