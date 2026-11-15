    bool port_call_exe_d(u32 fn, const u32* r, u32& out);
    bool port_phase_exe_d(u32 fn, bool run);

    bool player_gun(u32 fn);
    bool player_special(u32 fn, u32 racks);

    void player_mode_second_stage();
    void player_mode_level();
    void player_mode_idle();
    void player_mode_first_stage();
    void view_projection(u32 view, u32 h);
    bool view_fog(u32 view, s32 near, s32 far, s32 h, u32 table);
    void view_fixed();
    void view_follow_player_by_option();
    u32 recorder_attach(u32 allocation);
    u32 trails_draw_weapons(u32 ot, u32 packets);
    u32 trails_draw_aircraft(u32 ot, u32 packets);
    u32 trails_draw(u32 ot, u32 packets);
    u32 trails_age_weapons();
    u32 trails_age_aircraft();

    u32 record_weapon_kind(u32 kind, u32 slot_i, u32 a3, u32 a4, u32 a5);
    u32 record_tile_change(u32 x, u32 z, u32 tile, u32 old);
    u32 record_gun_flash(u32 kind, u32 t);

    u32 effects_draw(u32 ot, u32 packets);
    u32 lights_draw(u32 bank, u32 v0);
    u32 light_glow(u32 light, u32 work);
    u32 light_ring(u32 light, u32 work);
    u32 light_flash(u32 light, u32 work);
    u32 sun_disc(u32 rec, u32 colour);
    u32 sun_flare(u32 rec, s32 kind, s32 floor);
    u32 sun_flare_original(u32 rec, s32 kind, s32 floor);
    void sun_enhance(s32 kind);
    bool sun_on_screen_ = false;
    u32 flare_sprites(u32 work, u32 scratch);
