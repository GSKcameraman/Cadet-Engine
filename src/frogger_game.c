#include "ecs.h"
#include "fs.h"
#include "gpu.h"
#include "heap.h"
#include "render.h"
#include "timer_object.h"
#include "transform.h"
#include "wm.h"
#include "debug.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

typedef struct transform_component_t
{
	transform_t transform;
} transform_component_t;

typedef struct camera_component_t
{
	mat4f_t projection;
	mat4f_t view;
} camera_component_t;

typedef struct model_component_t
{
	gpu_mesh_info_t* mesh_info;
	gpu_shader_info_t* shader_info;
} model_component_t;

typedef struct player_component_t
{
	int index;
} player_component_t;

typedef struct name_component_t
{
	char name[32];
} name_component_t;

typedef struct speed_component_t {
	float speed;
}speed_component_t;

typedef struct refresh_component_t {
	float rate;
}refresh_component_t;

typedef struct row_component_t {
	int row;
}row_component_t;

typedef struct frogger_game_t
{
	heap_t* heap;
	fs_t* fs;
	wm_window_t* window;
	render_t* render;

	timer_object_t* timer;

	ecs_t* ecs;
	int transform_type;
	int camera_type;
	int model_type;
	int player_type;
	int name_type;
	int speed_type;
	int refresh_type;
	int row_type;
	ecs_entity_ref_t player_ent;
	ecs_entity_ref_t camera_ent;

	gpu_mesh_info_t cube_mesh;
	gpu_mesh_info_t rect_mesh;
	gpu_shader_info_t cube_shader;
	gpu_shader_info_t rect_shader;
	fs_work_t* vertex_shader_work;
	fs_work_t* fragment_shader_work;
} frogger_game_t;


float elapsedTime = 1.0f;
float right = 80.0f / 4.0f;
float top = 45.0f / 4.0f;
static void load_resources(frogger_game_t* game);
static void unload_resources(frogger_game_t* game);
static void spawn_player(frogger_game_t* game, int index);
static void spawn_enemy(frogger_game_t* game, int index, int row, int order);
static void spawn_camera(frogger_game_t* game);
static void update_players(frogger_game_t* game);
static void transform_player(transform_component_t* transform_comp, player_component_t* player_comp, float speed, uint32_t key_mask);
static void transform_enemies(transform_component_t* transform_comp, int row,float speed,float dt);
static void draw_models(frogger_game_t* game);
static void collision_detecter(transform_component_t* player_transform, transform_component_t* transform_comp);
static void respawn_player(transform_component_t* player_transform);




frogger_game_t* frogger_game_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render)
{
	frogger_game_t* game = heap_alloc(heap, sizeof(frogger_game_t), 8);
	game->heap = heap;
	game->fs = fs;
	game->window = window;
	game->render = render;

	game->timer = timer_object_create(heap, NULL);

	game->ecs = ecs_create(heap);
	game->transform_type = ecs_register_component_type(game->ecs, "transform", sizeof(transform_component_t), _Alignof(transform_component_t));
	game->camera_type = ecs_register_component_type(game->ecs, "camera", sizeof(camera_component_t), _Alignof(camera_component_t));
	game->model_type = ecs_register_component_type(game->ecs, "model", sizeof(model_component_t), _Alignof(model_component_t));
	game->player_type = ecs_register_component_type(game->ecs, "player", sizeof(player_component_t), _Alignof(player_component_t));
	game->name_type = ecs_register_component_type(game->ecs, "name", sizeof(name_component_t), _Alignof(name_component_t));
	game->speed_type = ecs_register_component_type(game->ecs, "speed", sizeof(speed_component_t), _Alignof(speed_component_t));
	game->refresh_type = ecs_register_component_type(game->ecs, "name", sizeof(refresh_component_t), _Alignof(refresh_component_t));
	game->row_type = ecs_register_component_type(game->ecs, "name", sizeof(row_component_t), _Alignof(row_component_t));


	load_resources(game);
	spawn_player(game, 0);
	spawn_enemy(game, 1, 1, 0);
	spawn_enemy(game, 2, 2, 1);
	spawn_enemy(game, 3, 3, 2);
	spawn_enemy(game, 4, 4, 3);
	spawn_enemy(game, 5, 5, 1);
	spawn_enemy(game, 6, 6, 0);
	spawn_enemy(game, 7, 7, 0);
	spawn_enemy(game, 8, 8, 0);
	spawn_enemy(game, 9, 9, 0);
	spawn_enemy(game, 11, 1, 2);
	spawn_enemy(game, 12, 1, 5);
	spawn_enemy(game, 13, 2, -4);
	spawn_enemy(game, 14, 2, 4);
	spawn_camera(game);

	return game;
}

void frogger_game_destroy(frogger_game_t* game)
{
	ecs_destroy(game->ecs);
	timer_object_destroy(game->timer);
	unload_resources(game);
	heap_free(game->heap, game);
}

void frogger_game_update(frogger_game_t* game)
{
	timer_object_update(game->timer);
	ecs_update(game->ecs);
	update_players(game);
	draw_models(game);
	render_push_done(game->render);
}

static void load_resources(frogger_game_t* game)
{
	game->vertex_shader_work = fs_read(game->fs, "shaders/triangle.vert.spv", game->heap, false, false);
	game->fragment_shader_work = fs_read(game->fs, "shaders/triangle.frag.spv", game->heap, false, false);
	game->cube_shader = (gpu_shader_info_t)
	{
		.vertex_shader_data = fs_work_get_buffer(game->vertex_shader_work),
		.vertex_shader_size = fs_work_get_size(game->vertex_shader_work),
		.fragment_shader_data = fs_work_get_buffer(game->fragment_shader_work),
		.fragment_shader_size = fs_work_get_size(game->fragment_shader_work),
		.uniform_buffer_count = 1,
	};

	static vec3f_t cube_verts[] =
	{
		{ -0.5f, -0.5f,  0.5f }, { 0.0f, 0.5f,  0.5f },
		{  0.5f, -0.5f,  0.5f }, { 0.5f, 0.0f,  0.5f },
		{  0.5f,  0.5f,  0.5f }, { 0.5f, 0.5f,  0.0f },
		{ -0.5f,  0.5f,  0.5f }, { 0.5f, 0.0f,  0.0f },
		{ -0.5f, -0.5f, -0.5f }, { 0.0f, 0.5f,  0.0f },
		{  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f,  0.5f },
		{  0.5f,  0.5f, -0.5f }, { 0.5f, 0.5f,  0.5f },
		{ -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f,  0.0f },
	};
	static vec3f_t rect_verts[] =
	{
		{ -0.5f, -1.0f,  0.5f }, { 0.0f, 1.0f,  0.5f },
		{  0.5f, -1.0f,  0.5f }, { 0.5f, 0.0f,  0.5f },
		{  0.5f,  1.0f,  0.5f }, { 0.5f, 1.0f,  0.0f },
		{ -0.5f,  1.0f,  0.5f }, { 0.5f, 0.0f,  0.0f },
		{ -0.5f, -1.0f, -0.5f }, { 0.0f, 1.0f,  0.0f },
		{  0.5f, -1.0f, -0.5f }, { 0.0f, 0.0f,  0.5f },
		{  0.5f,  1.0f, -0.5f }, { 0.5f, 1.0f,  0.5f },
		{ -0.5f,  1.0f, -0.5f }, { 0.0f, 0.0f,  0.0f },
	};
	static uint16_t cube_indices[] =
	{
		0, 1, 2,
		2, 3, 0,
		1, 5, 6,
		6, 2, 1,
		7, 6, 5,
		5, 4, 7,
		4, 0, 3,
		3, 7, 4,
		4, 5, 1,
		1, 0, 4,
		3, 2, 6,
		6, 7, 3
	};
	static uint16_t rect_indices[] =
	{
		0, 1, 2,
		2, 3, 0,
		1, 5, 6,
		6, 2, 1,
		7, 6, 5,
		5, 4, 7,
		4, 0, 3,
		3, 7, 4,
		4, 5, 1,
		1, 0, 4,
		3, 2, 6,
		6, 7, 3
	};
	game->cube_mesh = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts,
		.vertex_data_size = sizeof(cube_verts),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};
	game->rect_mesh = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = rect_verts,
		.vertex_data_size = sizeof(rect_verts),
		.index_data = rect_indices,
		.index_data_size = sizeof(rect_indices),

	};
}

static void unload_resources(frogger_game_t* game)
{
	fs_work_destroy(game->fragment_shader_work);
	fs_work_destroy(game->vertex_shader_work);
}

static void spawn_player(frogger_game_t* game, int index)
{
	uint64_t k_player_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->player_type) |
		(1ULL << game->name_type);
	game->player_ent = ecs_entity_add(game->ecs, k_player_ent_mask);

	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);
	transform_comp->transform.translation.z = top - 1.0f;
	

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "player");

	player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->player_type, true);
	player_comp->index = index;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = &game->cube_shader;

	speed_component_t* speed_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->speed_type, true);
	speed_comp->speed = 1.0f;

	refresh_component_t* refresh_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->refresh_type, true);
	refresh_comp->rate = 0.25f;

}

static void spawn_enemy(frogger_game_t* game, int index, int row, int order) {
	uint64_t k_player_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->player_type) |
		(1ULL << game->name_type);
	game->player_ent = ecs_entity_add(game->ecs, k_player_ent_mask);

	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);
	transform_comp->transform.translation.z = top - 1.0f - 2.0f * row;
	transform_comp->transform.translation.y = order * 2.0f;

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "enemy");

	player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->player_type, true);
	player_comp->index = index;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->model_type, true);
	model_comp->mesh_info = &game->rect_mesh;
	model_comp->shader_info = &game->cube_shader;

	refresh_component_t* refresh_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->refresh_type, true);
	refresh_comp->rate = 0.25f;


	speed_component_t* speed_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->speed_type, true);
	speed_comp->speed = 3.0f;

	row_component_t* row_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->row_type, true);
	row_comp->row = row;


}


static void spawn_camera(frogger_game_t* game)
{
	uint64_t k_camera_ent_mask =
		(1ULL << game->camera_type) |
		(1ULL << game->name_type);
	game->camera_ent = ecs_entity_add(game->ecs, k_camera_ent_mask);

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "camera");

	camera_component_t* camera_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->camera_type, true);
	//mat4f_make_perspective(&camera_comp->projection, (float)M_PI / 2.0f, 16.0f / 9.0f, 0.1f, 100.0f);
	mat4f_make_perspective_orthographic(&camera_comp->projection, right, - right, top,- top, 0.1f, 100.0f);
	vec3f_t eye_pos = vec3f_scale(vec3f_forward(), -5.0f);
	vec3f_t forward = vec3f_forward();
	vec3f_t up = vec3f_up();
	mat4f_make_lookat(&camera_comp->view, &eye_pos, &forward, &up);
}

static void update_players(frogger_game_t* game)
{
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.001f;

	elapsedTime += dt;

	

	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->player_type);

	transform_component_t* player_transform = NULL;

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{


		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		player_component_t* player_comp = ecs_query_get_component(game->ecs, &query, game->player_type);
		name_component_t* name_comp = ecs_query_get_component(game->ecs, &query, game->name_type);
		speed_component_t* speed_comp = ecs_query_get_component(game->ecs, &query, game->speed_type);

		if (strcmp(name_comp->name, "player") == 0) {
			refresh_component_t* refresh_comp = ecs_query_get_component(game->ecs, &query, game->refresh_type);
			player_transform = transform_comp;
			if (elapsedTime >= refresh_comp->rate) {
				
				float speed = speed_comp->speed;
				uint32_t key_mask = wm_get_key_mask(game->window);
				transform_player(transform_comp, player_comp, speed, key_mask);


			}
		}
		else {
			row_component_t* row_comp = ecs_query_get_component(game->ecs, &query, game->row_type);
			int row = row_comp->row;
			transform_enemies(transform_comp, row, speed_comp->speed,dt);
			collision_detecter(player_transform, transform_comp);
		}
	
	}
}

static void transform_player(transform_component_t* transform_comp, player_component_t* player_comp, float speed, uint32_t key_mask) {
	transform_t move;
	transform_identity(&move);
	
	if (key_mask & k_key_up)
	{
		move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), -speed));
		elapsedTime = 0;
	}
	if (key_mask & k_key_down)
	{
		move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), speed));
		elapsedTime = 0;
	}
	if (key_mask & k_key_left)
	{
		move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -speed));
		elapsedTime = 0;
	}
	if (key_mask & k_key_right)
	{
		move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), speed));
		elapsedTime = 0;
	}
	transform_multiply(&transform_comp->transform, &move);
	if (transform_comp->transform.translation.y > right) {
		transform_comp->transform.translation.y = -right;
	}
	if (transform_comp->transform.translation.y < -right) {
		transform_comp->transform.translation.y = right;
	}
	if (transform_comp->transform.translation.z > top) {
		debug_print(k_print_info, "Reached other side of the road!\n");
		respawn_player(transform_comp);
	}
	if (transform_comp->transform.translation.z < -top) {
		transform_comp->transform.translation.z = top - 1.0f;
	}
}

static void transform_enemies(transform_component_t* transform_comp, int row, float speed, float dt) {
	
	
	transform_t move;
	transform_identity(&move);
	if (row % 2 == 0) {
		move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), dt*speed));
	}
	else {
		move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -dt*speed));
	}
	transform_multiply(&transform_comp->transform, &move);
	if (transform_comp->transform.translation.y > right) {
		transform_comp->transform.translation.y = -right;
	}
	if (transform_comp->transform.translation.y < -right) {
		transform_comp->transform.translation.y = right;
	}

}

static void collision_detecter(transform_component_t* player_transform, transform_component_t* transform_comp) {
	if (player_transform->transform.translation.z == transform_comp->transform.translation.z && 
		fabsf(player_transform->transform.translation.y - transform_comp->transform.translation.y) < 0.75f) {
		debug_print(k_print_info, "Collision!\n");
		respawn_player(player_transform);
	}
}

static void respawn_player(transform_component_t* player_transform) {
	transform_identity(&(player_transform->transform));
	player_transform->transform.translation.z = top - 1.0f;
	
}

static void draw_models(frogger_game_t* game)
{
	uint64_t k_camera_query_mask = (1ULL << game->camera_type);
	for (ecs_query_t camera_query = ecs_query_create(game->ecs, k_camera_query_mask);
		ecs_query_is_valid(game->ecs, &camera_query);
		ecs_query_next(game->ecs, &camera_query))
	{
		camera_component_t* camera_comp = ecs_query_get_component(game->ecs, &camera_query, game->camera_type);

		uint64_t k_model_query_mask = (1ULL << game->transform_type) | (1ULL << game->model_type);
		for (ecs_query_t query = ecs_query_create(game->ecs, k_model_query_mask);
			ecs_query_is_valid(game->ecs, &query);
			ecs_query_next(game->ecs, &query))
		{
			transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
			model_component_t* model_comp = ecs_query_get_component(game->ecs, &query, game->model_type);
			ecs_entity_ref_t entity_ref = ecs_query_get_entity(game->ecs, &query);

			struct
			{
				mat4f_t projection;
				mat4f_t model;
				mat4f_t view;
			} uniform_data;
			uniform_data.projection = camera_comp->projection;
			uniform_data.view = camera_comp->view;
			transform_to_matrix(&transform_comp->transform, &uniform_data.model);
			gpu_uniform_buffer_info_t uniform_info = { .data = &uniform_data, sizeof(uniform_data) };

			render_push_model(game->render, &entity_ref, model_comp->mesh_info, model_comp->shader_info, &uniform_info);
		}
	}
}
