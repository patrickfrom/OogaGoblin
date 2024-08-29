#include "range.c"

const int tile_width = 7;
const float entity_selection_radius = 6.0f;

const int rock_health = 4;

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

float sin_breathe(float time, float rate) {
    return (sin(time * rate) + 1.0) / 2.0;
}

void v2_animate_to_target(Vector2 *value, Vector2 target, float delta, float rate)
{
    f32_animate_to_target(&(value->x), target.x, delta, rate);
    f32_animate_to_target(&(value->y), target.y, delta, rate);
}

typedef struct Sprite
{
    Gfx_Image *image;
} Sprite;
typedef enum SpriteId
{
    SPRITE_NIL,
    SPRITE_PLAYER,
    SPRITE_FURNACE,
    SPRITE_ROCK0,
    SPRITE_ROCK1,
    SPRITE_ROCK2,
    SPRITE_ITEM_ROCK0_ORE,
    SPRITE_ITEM_ROCK1_ORE,
    SPRITE_ITEM_ROCK2_ORE,
    SPRITE_MAX
} SpriteId;
Sprite sprites[SPRITE_MAX];
Sprite *get_sprite(SpriteId id)
{
    if (id >= 0 && id < SPRITE_MAX)
    {
        return &sprites[id];
    }

    return &sprites[0];
}

typedef enum EntityArchetype
{
    ARCH_NIL = 0,
    ARCH_ROCK0 = 1,
    ARCH_TREE = 2,
    ARCH_PLAYER = 3,

    ARCH_ITEM_ROCK0_ORE = 4,
    ARCH_ITEM_ROCK1_ORE = 5,
    ARCH_ITEM_ROCK2_ORE = 6,

    ARCH_MAX
} EntityArchetype;

typedef unsigned char EntityFlag;
enum {
    ENTITY_IS_VALID = 1 << 0,
    ENTITY_RENDER_SPRITE = 1 << 1,
    ENTITY_DESTROYABLE_WORLD_ITEM = 1 << 2,
    ENTITY_IS_ITEM = 1 << 3,
};

typedef struct Entity
{
    EntityFlag entity_flag;
    EntityArchetype archetype;
    Vector2 position;

    SpriteId sprite_id;

    int health;
} Entity;

#define MAX_ENTITY_COUNT 1024
typedef struct World
{
    Entity entities[MAX_ENTITY_COUNT];
} World;
World *world = 0;

typedef struct WorldFrame
{
    Entity *selected_entity;
} WorldFrame;
WorldFrame world_frame;

Entity *create_entity()
{
    Entity *entity_found = 0;
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        Entity *existing_entity = &world->entities[i];
        if (!(existing_entity->entity_flag & ENTITY_IS_VALID))
        {
            entity_found = existing_entity;
            break;
        }
    }

    assert(entity_found, "No more free entities!");
    entity_found->entity_flag = ENTITY_IS_VALID;
    return entity_found;
}

void destroy_entity(Entity *entity)
{
    memset(entity, 0, sizeof(Entity));
}

void setup_rock(Entity *entity)
{
    entity->archetype = ARCH_ROCK0;
    entity->sprite_id = SPRITE_ROCK0;
    entity->health = rock_health;
    entity->entity_flag |= ENTITY_DESTROYABLE_WORLD_ITEM;
}

void setup_player(Entity *entity)
{
    entity->archetype = ARCH_PLAYER;
    entity->sprite_id = SPRITE_PLAYER;
}

void setup_item_rock0_ore(Entity *entity)
{
    entity->archetype = ARCH_ITEM_ROCK0_ORE;
    entity->sprite_id = SPRITE_ITEM_ROCK0_ORE;
    entity->entity_flag |= ENTITY_IS_ITEM;
}

Vector2 screen_to_world()
{
    float mouse_x = input_frame.mouse_x;
    float mouse_y = input_frame.mouse_y;

    Matrix4 projection = draw_frame.projection;
    Matrix4 view = draw_frame.camera_xform;

    float window_width = window.width;
    float window_height = window.height;

    float ndc_x = (mouse_x / (window_width * 0.5f)) - 1.0f;
    float ndc_y = (mouse_y / (window_height * 0.5f)) - 1.0f;

    Vector4 world_position = v4(ndc_x, ndc_y, 0, 1);
    world_position = m4_transform(m4_inverse(projection), world_position);
    world_position = m4_transform(view, world_position);

    return (Vector2){world_position.x, world_position.y};
}

int world_pos_to_tile_pos(float world_position)
{
    return roundf(world_position / (float)tile_width);
}

int tile_pos_to_world_pos(int tile_position)
{
    return (float)tile_position * tile_width;
}

Vector2 v2_round_to_tile(Vector2 world_position)
{
    world_position.x = tile_pos_to_world_pos(world_pos_to_tile_pos(world_position.x));
    world_position.y = tile_pos_to_world_pos(world_pos_to_tile_pos(world_position.y));
    return world_position;
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

    sprites[SPRITE_NIL] = (Sprite){.image    = load_image_from_disk(STR("res/sprites/missing_texture.png"), get_heap_allocator())};
    sprites[SPRITE_PLAYER] = (Sprite){.image = load_image_from_disk(STR("res/sprites/goblin.png"), get_heap_allocator())};
    sprites[SPRITE_ROCK0] = (Sprite){.image  = load_image_from_disk(STR("res/sprites/rock0.png"), get_heap_allocator())};
    sprites[SPRITE_ROCK1] = (Sprite){.image  = load_image_from_disk(STR("res/sprites/rock1.png"), get_heap_allocator())};
    sprites[SPRITE_ITEM_ROCK0_ORE] = (Sprite){.image  = load_image_from_disk(STR("res/sprites/item_rock0_ore.png"), get_heap_allocator())};
    sprites[SPRITE_ITEM_ROCK1_ORE] = (Sprite){.image  = load_image_from_disk(STR("res/sprites/item_rock1_ore.png"), get_heap_allocator())};
    sprites[SPRITE_ITEM_ROCK2_ORE] = (Sprite){.image  = load_image_from_disk(STR("res/sprites/item_rock2_ore.png"), get_heap_allocator())};

    Entity *player_entity = create_entity();
    setup_player(player_entity);

    for (int i = 0; i < 10; i++)
    {
        Entity *entity = create_entity();
        setup_rock(entity);
        entity->position = v2(get_random_float32_in_range(-50, 50), get_random_float32_in_range(-50, 50));
        entity->position = v2_round_to_tile(entity->position);
    }

    Audio_Source goblin_miner_ost_source;
    bool goblin_miner_ost = audio_open_source_load(&goblin_miner_ost_source, STR("res/sounds/goblin_miner_ost.wav"), get_heap_allocator());
    assert(goblin_miner_ost, "Could not load goblin_miner_ost.wav");

    Audio_Player *song_player = audio_player_get_one();

    audio_player_set_source(song_player, goblin_miner_ost_source);
    audio_player_set_state(song_player, AUDIO_PLAYER_STATE_PLAYING);
    audio_player_set_looping(song_player, true);
    song_player->config.volume = 0.15f;

    Gfx_Font *font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
    assert(font, "Failed to load C:/windows/fonts/arial.ttf");

    const u32 font_height = 48;

    float zoom = 18.5;
    Vector2 camera_position = v2(0, 0);

    const float64 fps_limit = 69000;
    const float64 min_frametime = 1.0 / fps_limit;

    float64 last_time = os_get_elapsed_seconds();
    log("Size of EntityFlag: %d\n", sizeof(EntityFlag));
    while (!window.should_close)
    {
        reset_temporary_storage();
        world_frame = (WorldFrame){0};

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
        os_update();

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

        Vector2 mouse_position_world = screen_to_world();
        int mouse_tile_x = world_pos_to_tile_pos(mouse_position_world.x);
        int mouse_tile_y = world_pos_to_tile_pos(mouse_position_world.y);

        // :tile rendering
        {
            int player_tile_x = world_pos_to_tile_pos(player_entity->position.x);
            int player_tile_y = world_pos_to_tile_pos(player_entity->position.y);

            const int tile_radius_x = 13;
            const int tile_radius_y = 10;
            for (int x = player_tile_x - tile_radius_x; x < player_tile_x + tile_radius_x; ++x)
            {
                for (int y = player_tile_y - tile_radius_y; y < player_tile_y + tile_radius_y; ++y)
                {

                    if ((x + (y % 2 == 0)) % 2 == 0)
                    {
                        float x_position = x * tile_width;
                        float y_position = y * tile_width;
                        Vector4 color = v4(0.1, 0.1, 0.1, 0.1);
                        draw_rect(v2(x_position + (float)tile_width * -0.5f, y_position + (float)tile_width * -0.5f), v2(tile_width, tile_width), color);
                    }
                }
            }
        }

        // :hover world pos
        {
            float smallest_distance = INFINITY;

            for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
            {
                Entity *entity = &world->entities[i];
                if (!(entity->entity_flag & ENTITY_IS_VALID) || !(entity->entity_flag & ENTITY_DESTROYABLE_WORLD_ITEM))
                    continue;

                int entity_tile_x = world_pos_to_tile_pos(entity->position.x);
                int entity_tile_y = world_pos_to_tile_pos(entity->position.y);

                float distance = fabs(v2_dist(entity->position, mouse_position_world));
                if (distance < entity_selection_radius)
                {
                    if (!world_frame.selected_entity || distance < smallest_distance)
                    {
                        world_frame.selected_entity = entity;
                        smallest_distance = distance;
                    }
                }
            }
        }

        // :click
        {
            Entity *selected_entity = world_frame.selected_entity;
            if (is_key_just_pressed(MOUSE_BUTTON_LEFT))
            {
                consume_key_just_pressed(MOUSE_BUTTON_LEFT);
                if (selected_entity)
                {
                    selected_entity->health--;
                    if (selected_entity->health <= 0)
                    {
                        switch (selected_entity->archetype) {
                            case ARCH_ROCK0: {
                                Entity *entity = create_entity();
                                setup_item_rock0_ore(entity);
                                entity->position = selected_entity->position;
                            } break;
                            default: {}
                        }
                        destroy_entity(selected_entity);
                    }
                }
            }
        }

        // :render
        for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
        {
            Entity *entity = &world->entities[i];
            if (!(entity->entity_flag & ENTITY_IS_VALID))
                continue;

            switch (entity->archetype)
            {
            default:
            {
                Sprite *sprite = get_sprite(entity->sprite_id);
                Vector2 size = v2(sprite->image->width, sprite->image->height);
                Matrix4 xform = m4_scalar(1.0);
                if (entity->entity_flag & ENTITY_IS_ITEM) {
                    xform = m4_translate(xform, v3(0.0, 2.0 * sin_breathe(now, 5.0f), 0.0));
                }
                xform = m4_translate(xform, v3(0.0, tile_width * -0.5, 0.0));
                xform = m4_translate(xform, v3(entity->position.x, entity->position.y, 0.0));
                xform = m4_translate(xform, v3(size.x * -0.5, 0.0, 0.0));

                Vector4 color = COLOR_WHITE;
                if (world_frame.selected_entity == entity)
                {
                    color = COLOR_GREEN;
                }

                draw_image_xform(sprite->image, xform, size, color);
                // debug pos
                // draw_text(font, tprint(STR("%f, %f"), entity->position.x, entity->position.y), font_height, v2(entity->position.x, entity->position.y - 1.5f), v2(0.05f, 0.05f), COLOR_BLACK);
                break;
            }
            }
        }

        gfx_update();
    }

    return 0;
}
