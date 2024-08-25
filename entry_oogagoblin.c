
int entry(int argc, char **argv)
{
    window.title = STR("Ooga Goblin");
    window.scaled_width = 1280;
    window.scaled_height = 720;
    window.x = 200;
    window.y = 90;
    window.clear_color = hex_to_rgba(0x12173DFF);
    window.allow_resize = true;
    window.fullscreen = false;

    Gfx_Image *goblin_player = load_image_from_disk(fixed_string("assets/goblin.png"), get_heap_allocator());
    assert(goblin_player, "failed to load goblin.png");

    const float64 fps_limit = 69000;
    const float64 min_frametime = 1.0 / fps_limit;

    Vector2 player_pos = v2(0, 0);

    float64 last_time = os_get_elapsed_seconds();
    while (!window.should_close)
    {
        reset_temporary_storage();

        os_update();

        if (is_key_just_pressed(KEY_ESCAPE))
        {
            window.should_close = true;
        }

        float64 now = os_get_elapsed_seconds();
        float64 delta = now - last_time;
        if (delta < min_frametime)
        {
            os_high_precision_sleep((min_frametime - delta) * 1000.0);
            now = os_get_elapsed_seconds();
            delta = now - last_time;
        }
        last_time = now;

        if (is_key_just_pressed(KEY_F11))
        {
            window.fullscreen = !window.fullscreen;
        }

        Vector2 input_axis = v2(0, 0);
        if (is_key_down('A'))
        {
            input_axis.x -= 1.0;
        }

        if (is_key_down('D'))
        {
            input_axis.x += 1.0;
        }

        if (is_key_down('S'))
        {
            input_axis.y -= 1.0;
        }

        if (is_key_down('W'))
        {
            input_axis.y += 1.0;
        }

        input_axis = v2_normalize(input_axis);

        player_pos = v2_add(player_pos, v2_mulf(input_axis, 1.0 * delta));

        Matrix4 xform = m4_scalar(1.0);
        xform = m4_translate(xform, v3(player_pos.x, player_pos.y, 0.0));
        draw_image_xform(goblin_player, xform, v2(.5f, .5f), COLOR_WHITE);

        gfx_update();
    }

    return 0;
}
