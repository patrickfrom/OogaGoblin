bool almost_equals(float a, float b, float epsilon)
{
    return fabs(a - b) <= epsilon;
}

bool f32_animate_to_target(float *value, float target, float delta, float rate)
{
    *value += (target - *value) * (1.0 - pow(2.0f, -rate * delta));
    if (almost_equals(*value, target, 0.001f))
    {
        *value = target;
        return true;
    }

    return false;
}

void v2_animate_to_target(Vector2 *value, Vector2 target, float delta, float rate)
{
    f32_animate_to_target(&(value->x), target.x, delta, rate);
    f32_animate_to_target(&(value->y), target.y, delta, rate);
}

typedef struct Sprite
{
    Gfx_Image *image;
    Vector2 size;
} Sprite;
typedef enum SpriteId
{
    SPRITE_NIL,
    SPRITE_PLAYER,
    SPRITE_ROCK0,
    SPRITE_ROCK1,
    SPRITE_ROCK2,
    SPRITE_MAX
} SpriteId;

typedef enum EntityArchetype
{
    ARCH_NIL = 0,
    ARCH_ROCK = 1,
    ARCH_TREE = 2,
    ARCH_PLAYER = 3
} EntityArchetype;

typedef struct Entity
{
    bool is_valid;
    EntityArchetype archetype;
    Vector2 position;

    bool render_sprite;
    SpriteId sprite_id;
} Entity;

#define MAX_ENTITY_COUNT 1024
typedef struct World
{
    Entity entities[MAX_ENTITY_COUNT];
} World;
World *world = 0;

Entity *create_entity()
{
    Entity *entity_found = 0;
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        Entity *existing_entity = &world->entities[i];
        if (!existing_entity->is_valid)
        {
            entity_found = existing_entity;
            break;
        }
    }

    assert(entity_found, "No more free entities!");
    entity_found->is_valid = true;
    return entity_found;
}

void destroy_entity(Entity *entity)
{
    memset(entity, 0, sizeof(Entity));
}

void setup_rock(Entity *entity)
{
    entity->archetype = ARCH_ROCK;
    entity->sprite_id = SPRITE_ROCK0;
}

void setup_player(Entity *entity)
{
    entity->archetype = ARCH_PLAYER;
    entity->sprite_id = SPRITE_PLAYER;
}

Sprite sprites[SPRITE_MAX];
Sprite *get_sprite(SpriteId id)
{
    if (id >= 0 && id < SPRITE_MAX)
    {
        return &sprites[id];
    }

    return &sprites[0];
}

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

    world = alloc(get_heap_allocator(), sizeof(World));

    sprites[SPRITE_NIL] = (Sprite){.image = load_image_from_disk(STR("assets/missing_texture.png"), get_heap_allocator()), .size = v2(5.0, 5.0)};
    sprites[SPRITE_PLAYER] = (Sprite){.image = load_image_from_disk(STR("assets/goblin.png"), get_heap_allocator()), .size = v2(5.0, 7.0)};
    sprites[SPRITE_ROCK0] = (Sprite){.image = load_image_from_disk(STR("assets/rock0.png"), get_heap_allocator()), .size = v2(5.0, 3.0)};
    sprites[SPRITE_ROCK1] = (Sprite){.image = load_image_from_disk(STR("assets/rock1.png"), get_heap_allocator()), .size = v2(5.0, 3.0)};
    sprites[SPRITE_ROCK2] = (Sprite){.image = load_image_from_disk(STR("assets/rock2.png"), get_heap_allocator()), .size = v2(5.0, 3.0)};

    Entity *player_entity = create_entity();
    setup_player(player_entity);

    for (int i = 0; i < 10; i++)
    {
        Entity *entity = create_entity();
        setup_rock(entity);
        entity->position = v2(get_random_float32_in_range(-25, 25), get_random_float32_in_range(-25, 25));
    }

    Audio_Source goblin_miner_ost_source;
    bool goblin_miner_ost = audio_open_source_load(&goblin_miner_ost_source, STR("assets/sounds/goblin_miner_ost.wav"), get_heap_allocator());
    assert(goblin_miner_ost, "Could not load goblin_miner_ost.wav");

    Audio_Player *song_player = audio_player_get_one();

    audio_player_set_source(song_player, goblin_miner_ost_source);
    audio_player_set_state(song_player, AUDIO_PLAYER_STATE_PLAYING);
    audio_player_set_looping(song_player, true);
    song_player->config.volume = 0.15f;

    float zoom = 18.5;
    Vector2 camera_position = v2(0, 0);

    const float64 fps_limit = 69000;
    const float64 min_frametime = 1.0 / fps_limit;

    float64 last_time = os_get_elapsed_seconds();
    while (!window.should_close)
    {
        reset_temporary_storage();

        // :delta
        float64 now = os_get_elapsed_seconds();
        float64 delta = now - last_time;
        if (delta < min_frametime)
        {
            os_high_precision_sleep((min_frametime - delta) * 1000.0);
            now = os_get_elapsed_seconds();
            delta = now - last_time;
        }
        last_time = now;

        draw_frame.projection = m4_make_orthographic_projection(
            window.width * -0.5f, window.width * 0.5f,
            window.height * -0.5f, window.height * 0.5f,
            -1, 10);

        // :camera
        {
            Vector2 target_position = player_entity->position;
            v2_animate_to_target(&camera_position, target_position, delta, 15.0f);
            draw_frame.camera_xform = m4_make_scale(v3(1.0, 1.0, 1.0));
            draw_frame.camera_xform = m4_mul(draw_frame.camera_xform, m4_make_translation(v3(camera_position.x, camera_position.y, 0)));
            draw_frame.camera_xform = m4_mul(draw_frame.camera_xform, m4_make_scale(v3(1.0 / zoom, 1.0 / zoom, 1.0)));
        }

        if (is_key_just_pressed(KEY_ESCAPE))
        {
            window.should_close = true;
        }

        if (is_key_just_pressed(KEY_F11))
        {
            window.fullscreen = !window.fullscreen;
        }

        os_update();

        // :player movement
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

        player_entity->position = v2_add(player_entity->position, v2_mulf(input_axis, 25.0 * delta));

        // :render
        for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
        {
            Entity *entity = &world->entities[i];
            if (!entity->is_valid)
                continue;

            switch (entity->archetype)
            {
            default:
            {
                Sprite *sprite = get_sprite(entity->sprite_id);
                Vector2 size = sprite->size;
                Matrix4 xform = m4_scalar(1.0);
                xform = m4_translate(xform, v3(entity->position.x, entity->position.y, 0.0));
                xform = m4_translate(xform, v3(size.x * -0.5, 0.0, 0.0));
                draw_image_xform(sprite->image, xform, size, COLOR_WHITE);
                break;
            }
            }
        }

        gfx_update();
    }

    return 0;
}
