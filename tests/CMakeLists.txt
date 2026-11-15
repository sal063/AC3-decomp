add_executable(ac3engine_tests
    test_main.cpp
    test_wrap.cpp
    test_rng.cpp
    test_vfs.cpp
    test_sticks.cpp)
target_include_directories(ac3engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ac3engine_tests PRIVATE ac3_warnings ac3_core ac3_sim ac3_vfs ac3_assets)

add_test(NAME unit COMMAND ac3engine_tests)

set(AC3_DISC1 "" CACHE FILEPATH "Disc 1 image, for the tests that read the game's data")
if(EXISTS "${AC3_DISC1}")
    add_test(NAME disc_verify COMMAND ac3cat --disc "${AC3_DISC1}" verify)
    set_tests_properties(disc_verify PROPERTIES LABELS disc)
else()
    message(STATUS "disc tests skipped: no image at ${AC3_DISC1}")
endif()
