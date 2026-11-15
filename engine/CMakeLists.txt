add_library(ac3_core STATIC
    core/log.cpp
    core/file.cpp
    core/png.cpp
    core/settings.cpp)
target_include_directories(ac3_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ac3_core PRIVATE ac3_warnings)

add_library(ac3_assets STATIC
    assets/tim.cpp
    assets/vram.cpp)
target_link_libraries(ac3_assets PUBLIC ac3_core PRIVATE ac3_warnings)

add_library(ac3_vfs STATIC
    vfs/disc_image.cpp
    vfs/iso9660.cpp
    vfs/pack_index.cpp
    vfs/ace_discs.cpp
    vfs/ulz.cpp
    vfs/container.cpp
    vfs/xdelta.cpp
    vfs/translation.cpp)
target_link_libraries(ac3_vfs PUBLIC ac3_core ac3_assets PRIVATE ac3_warnings)

add_library(ac3_gamedata STATIC
    assets/mission.cpp)
target_link_libraries(ac3_gamedata PUBLIC ac3_vfs ac3_assets PRIVATE ac3_warnings)

file(GLOB AC3_PORT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/sim/game/ports/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sim/game/ports/*/*.cpp)
add_library(ac3_sim STATIC
    sim/memory.cpp
    sim/gte.cpp
    sim/precise.cpp
    sim/state_file.cpp
    sim/game/psxlib.cpp
    sim/game/tick.cpp
    sim/game/common.cpp
    sim/game/step2.cpp
    sim/game/step5.cpp
    sim/game/weapons.cpp
    sim/game/bullets.cpp
    sim/game/impacts.cpp
    sim/game/ground.cpp
    sim/game/ground_fx.cpp
    sim/game/mission.cpp
    sim/game/player.cpp
    sim/game/player_weapons.cpp
    sim/game/flight.cpp
    sim/game/ai.cpp
    sim/game/ai_states.cpp
    sim/game/ai_sound.cpp
    sim/game/frame.cpp
    sim/game/smooth.cpp
    sim/game/replay.cpp
    sim/game/loader.cpp
    sim/game/mission_init.cpp
    sim/game/hud_templates.cpp
    sim/game/gauges.cpp
    sim/game/loading.cpp
    sim/game/mission_load.cpp
    sim/game/title.cpp
    sim/game/title_accounts.cpp
    sim/game/title_new.cpp
    sim/game/gmv.cpp
    sim/game/o2_screens.cpp
    sim/game/o2_load.cpp
    sim/game/o2_text.cpp
    sim/game/o2_hangar.cpp
    sim/game/o2_menu.cpp
    sim/game/address_table.cpp
    sim/game/replay_shots.cpp
    sim/game/camera.cpp
    sim/game/mission_flow.cpp
    sim/game/hud.cpp
    sim/game/draw_logic.cpp
    sim/game/boot.cpp
    sim/game/main_loop.cpp
    sim/game/sound_driver.cpp
    sim/game/kcd.cpp
    sim/game/step7.cpp
    sim/game/step8.cpp
    sim/game/raycast.cpp
    sim/game/targeting.cpp
    ${AC3_PORT_SOURCES})
target_link_libraries(ac3_sim PUBLIC ac3_core PRIVATE ac3_warnings)
target_compile_options(ac3_sim PUBLIC -fwrapv)

add_library(ac3_machine STATIC
    machine/spu.cpp
    machine/machine.cpp
    machine/cd.cpp
    machine/card.cpp
    machine/mdec.cpp
    machine/gpu.cpp)
target_link_libraries(ac3_machine PUBLIC ac3_sim ac3_vfs PRIVATE ac3_warnings)

add_library(ac3_platform STATIC
    platform/window.cpp)
target_link_libraries(ac3_platform PUBLIC ac3_core SDL3::SDL3 PRIVATE ac3_warnings)

add_library(ac3_render STATIC
    render/vk.cpp
    render/renderer.cpp
    render/terrain.cpp
    render/hd.cpp
    render/hd_textures.cpp
    render/ocean.cpp)
add_dependencies(ac3_render ac3_shaders)
target_include_directories(ac3_render PRIVATE ${AC3_GENERATED})
target_link_libraries(ac3_render PUBLIC ac3_core ac3_gamedata ac3_machine Vulkan::Vulkan PRIVATE ac3_warnings SDL3::SDL3)
