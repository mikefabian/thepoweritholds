#include "range.c"

bool almost_equal(float64 a, float64 b, float64 epsilon)
{
	return fabs(a - b) <= epsilon;
}

bool animate_f32_to_target(float *value, float target, float delta_t, float rate)
{
	*value += (target - *value) * (1.0 - pow(2.0f, -rate * delta_t));
	if (almost_equal(*value, target, 0.001f))
	{
		*value = target;
		return true;
	}
	return false;
}

void animate_v2_to_target(Vector2 *value, Vector2 target, float delta_t, float rate)
{
	animate_f32_to_target(&(value->x), target.x, delta_t, rate);
	animate_f32_to_target(&(value->y), target.y, delta_t, rate);
}

typedef struct Sprite
{
	Gfx_Image *image;
	Vector2 size;
} Sprite;

typedef enum SpriteID
{
	SPRITE_NIL,
	SPRITE_PLAYER,
	SPRITE_ROCK,
	SPRITE_TREE0,
	SPRITE_TREE1,
	SPRITE_ALTAR,
	SPRITE_MAX
} SpriteID;
Sprite sprites[SPRITE_MAX];

Sprite *get_sprite(SpriteID id)
{
	if (id >= 0 && id < SPRITE_MAX)
	{
		return &sprites[id];
	}
	return &sprites[0];
}

typedef enum EntityAcheType
{
	nil = 0,
	arch_rock = 1,
	arch_tree = 2,
	arch_altar = 3,
	arch_player = 4
} EntityAcheType;

typedef struct Entity
{
	bool is_valid;
	bool render_sprite;
	Vector2 size;
	Vector2 pos;
	Gfx_Image *sprite;
	SpriteID sprite_id;
	EntityAcheType arch;
} Entity;

#define MAX_ENTITY_COUNT 1024

typedef struct World
{
	Entity entities[MAX_ENTITY_COUNT];
} World;

World *world = 0;

Entity *entity_create()
{
	Entity *entity_found = 0;
	for (int i = 0; i < MAX_ENTITY_COUNT; i++)
	{
		Entity *existing_entity = &world->entities[i];
		if (!existing_entity->is_valid)
		{
			entity_found = existing_entity;
			break;
		}
	}
	assert(entity_found, "Failed to create entity, no space left");
	entity_found->is_valid = true;
	return entity_found;
}

void entity_destroy(Entity *entity)
{
	memset(entity, 0, sizeof(Entity));
}

void setup_player(Entity *entity)
{
	entity->arch = arch_player;
	entity->pos = v2(0, 0);
	entity->sprite = sprites[SPRITE_PLAYER].image;
	entity->sprite_id = SPRITE_PLAYER;
	// entity->size = v2(6.0, 8.0);
}

void setup_rock(Entity *entity)
{
	entity->arch = arch_rock;
	entity->pos = v2(0, 0);
	entity->sprite_id = SPRITE_ROCK;
	// entity->psize = v2(24, 16);
}

void setup_tree(Entity *entity)
{
	entity->arch = arch_tree;
	entity->pos = v2(0, 0);
	entity->sprite_id = SPRITE_TREE0;
	// entity->size = v2(24, 32);
}


Vector2 screen_to_world() {
	float mouse_x = input_frame.mouse_x;
	float mouse_y = input_frame.mouse_y;
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.camera_xform;
	float window_w = window.width;
	float window_h = window.height;

	// Normalize the mouse coordinates
	float ndc_x = (mouse_x / (window_w * 0.5f)) - 1.0f;
	float ndc_y = (mouse_y / (window_h * 0.5f)) - 1.0f;

	// Transform to world coordinates
	Vector4 world_pos = v4(ndc_x, ndc_y, 0, 1);
	world_pos = m4_transform(m4_inverse(proj), world_pos);
	world_pos = m4_transform(view, world_pos);
	// log("%f, %f", world_pos.x, world_pos.y);

	// Return as 2D vector
	return (Vector2){ world_pos.x, world_pos.y };
}

int entry(int argc, char **argv)
{

	window.title = STR("¡The flesh, and the power it holds!");
	window.scaled_width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.scaled_height = 720;
	window.x = 200;
	window.y = 90;
	window.clear_color = hex_to_rgba(0x2a2d3aff);

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

	Gfx_Font *font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf");
	const u32 font_height = 48;

	sprites[SPRITE_PLAYER] = (Sprite){.image = load_image_from_disk(STR("player.png"), get_heap_allocator()), .size = v2(9.0, 11.0)};
	sprites[SPRITE_TREE0] = (Sprite){.image = load_image_from_disk(STR("tree0.png"), get_heap_allocator()), .size = v2(24, 32)};
	sprites[SPRITE_TREE1] = (Sprite){.image = load_image_from_disk(STR("tree1.png"), get_heap_allocator()), .size = v2(39, 25)};
	sprites[SPRITE_ROCK] = (Sprite){.image = load_image_from_disk(STR("rock0.png"), get_heap_allocator()), .size = v2(24, 16)};
	sprites[SPRITE_ALTAR] = (Sprite){.image = load_image_from_disk(STR("altar.png"), get_heap_allocator()), .size = v2(30, 29)};

	Entity *player_entity = entity_create();
	setup_player(player_entity);
	const int position_distance = 200;

	for (int i = 0; i < 10; i++)
	{
		Entity *en = entity_create();
		setup_rock(en);
		en->arch = arch_rock;
		en->pos = v2(get_random_float32_in_range(-position_distance, position_distance), get_random_float32_in_range(-position_distance, position_distance));
	}

	for (int i = 0; i < 10; i++)
	{
		Entity *en = entity_create();
		setup_tree(en);
		en->arch = arch_tree;
		en->pos = v2(get_random_float32_in_range(-position_distance, position_distance), get_random_float32_in_range(-position_distance, position_distance));
	}

	float64 last_time = os_get_elapsed_seconds();
	float64 secconds_counter = 0.0;

	float zoom = 0.1875;
	float player_speed = 40.0;
	Vector2 player_pos = v2(0, 0);
	Vector2 camera_pos = v2(0, 0);

	while (!window.should_close)
	{
		reset_temporary_storage();
		float64 now = os_get_elapsed_seconds();
		if ((int)now != (int)last_time)
			log("%.2f FPS\n%.2fms", 1.0 / (now - last_time), (now - last_time) * 1000);

		float64 delta_t = now - last_time;
		last_time = now;
		os_update();

		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		// camera
		{
			Vector2 target_pos = player_entity->pos;
			// camera_pos = m4_make_scale(v3(1.0,1.0,1));
			animate_v2_to_target(&camera_pos, target_pos, delta_t, 4.00);
			draw_frame.camera_xform = m4_make_scale(v3(1.0, 1.0, 1));
			draw_frame.camera_xform = m4_mul(draw_frame.camera_xform, m4_make_translation(v3(camera_pos.x, camera_pos.y, 0)));
			draw_frame.camera_xform = m4_mul(draw_frame.camera_xform, m4_make_scale(v3(zoom, zoom, 1)));
		}
		
		// mouse position in the world
		{
			Vector2 pos = screen_to_world();
			//draw_text(font, sprint(temp_allocator, STR("%f %f"), pos.x, pos.y), font_height, pos, v2(0.1, 0.1), COLOR_RED); // debug the coordinates of the mouse

			for(int i = 0; i < MAX_ENTITY_COUNT; i++)
			{
				Entity *en = &world->entities[i];
				if (en->is_valid)
				{
					Sprite *sprite = get_sprite(en->sprite_id);
					Range2f bounds = range2f_make_bottom_center(sprite->size);
					bounds = range2f_shift(bounds, en->pos);

					Vector4 col = COLOR_WHITE;
					col.a = 0.4;
					if(range2f_contains(bounds, pos))
					{
						col.a = 1.0;
					}

					draw_rect(bounds.min,range2f_size(bounds), col);
				}
			}
			
		}

		const int tile_width = 8;

		for(int x=0; x < 10; x++)
		{
			for(int y = 0; y < 10; y++)
			{
				if((x + (y % 2 == 0)) %2 == 0)
				{
					float x_pos = x * tile_width;
					float y_pos = y * tile_width;
					draw_rect(v2(x_pos, y_pos), v2(tile_width, tile_width), COLOR_RED);
				}
			}
		}

		if (is_key_just_pressed(KEY_ESCAPE))
		{
			window.should_close = true;
		}

		Vector2 input_axis = v2(0, 0);
		if (is_key_down('W'))
			input_axis.y += 0.03;
		if (is_key_down('S'))
			input_axis.y -= 0.03;
		if (is_key_down('A'))
			input_axis.x -= 0.03;
		if (is_key_down('D'))
			input_axis.x += 0.03;

		input_axis = v2_normalize(input_axis);
		player_entity->pos = v2_add(player_entity->pos, v2_mulf(input_axis, player_speed * delta_t));

		for (int i = 0; i < MAX_ENTITY_COUNT; i++)
		{
			Entity *en = &world->entities[i];
			if (en->is_valid)
			{
				switch (en->arch)
				{

				default:
				{
					Sprite *sprite = get_sprite(en->sprite_id);
					Matrix4 xform = m4_scalar(1.0);
					xform = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
					xform = m4_translate(xform, v3(sprite->size.x * -0.5, 0.5, 0));
					draw_image_xform(sprite->image, xform, sprite->size, COLOR_WHITE);
					//draw_text(font, sprint(temp_allocator, STR("%f %f"), en->pos.x, en->pos.y), font_height, en->pos, v2(0.1, 0.1), COLOR_WHITE);
					break;
				}
				break;
				}
			}
		}
		gfx_update();
	}

	return 0;
}