/*
	Copyright (c) 2017-2020 ByteBit

	This file is part of KyroSpades.

	KyroSpades is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	KyroSpades is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with KyroSpades.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "window.h"
#include "camera.h"
#include "map.h"
#include "matrix.h"
#include "particle.h"
#include "model.h"
#include "weapon.h"
#include "config.h"
#include "tesselator.h"
#include "entitysystem.h"
#include "player.h"

struct entity_system particles;
struct tesselator particle_tesselator;

static float rain_timer = 0.0F;
static float snow_timer = 0.0F;

void particle_init() {
	entitysys_create(&particles, sizeof(struct Particle), 256);
	tesselator_create(&particle_tesselator, VERTEX_FLOAT, 0);
}

static bool particle_update_single(void* obj, void* user) {
	struct Particle* p = (struct Particle*)obj;
	float dt = *(float*)user;
	
	// Determine fade time based on particle type (snow fades slower than rain)
	float fade_time = (p->type == 254) ? 16.0F : 2.6F; // Snow (type 254) takes 16 seconds to fade, rain takes 2.6 (30% longer)
	float size = p->size * (1.0F - ((float)(window_time() - p->fade) / fade_time));

	if(size < 0.01F) {
		return true;
	} else {
		float acc_y = -32.0F * dt;

		// Apply gravity to all particles
		p->vy += acc_y;

		// Cap snow particle velocity to prevent accelerating too fast
		if(p->type == 254) {
			if(p->vy < -6.0F) p->vy = -6.0F;
		}

		float movement_x = p->vx * dt;
		float movement_y = p->vy * dt;
		float movement_z = p->vz * dt;
		bool on_ground = false;

		if(!map_isair(p->x + movement_x, p->y, p->z)) {
			movement_x = 0.0F;
			if(p->type == 254) { p->vx = 0.0F; } else { p->vx = -p->vx * 0.6F; }
			on_ground = true;
		}
		if(!map_isair(p->x + movement_x, p->y + movement_y, p->z)) {
			movement_y = 0.0F;
			if(p->type == 254) { p->vy = 0.0F; } else { p->vy = -p->vy * 0.6F; }
			on_ground = true;
		}
		if(!map_isair(p->x + movement_x, p->y + movement_y, p->z + movement_z)) {
			movement_z = 0.0F;
			if(p->type == 254) { p->vz = 0.0F; } else { p->vz = -p->vz * 0.6F; }
			on_ground = true;
		}

		float pow1_tys = 0.999991F + (2.55114F * dt - 2.30093F) * dt;
		float pow4_tys = 1.0F + (0.413432 * dt - 0.916185F) * dt;

		// air and ground friction
		if(on_ground) {
			p->vx *= pow1_tys; // pow(0.1F, dt);
			p->vy *= pow1_tys; // pow(0.1F, dt);
			p->vz *= pow1_tys; // pow(0.1F, dt);

			if(abs(p->vx) < 0.1F)
				p->vx = 0.0F;
			if(abs(p->vy) < 0.1F)
				p->vy = 0.0F;
			if(abs(p->vz) < 0.1F)
				p->vz = 0.0F;
		} else {
			p->vx *= pow4_tys; // pow(0.4F, dt);
			p->vy *= pow4_tys; // pow(0.4F, dt);
			p->vz *= pow4_tys; // pow(0.4F, dt);
		}

		p->x += movement_x;
		p->y += movement_y;
		p->z += movement_z;

		return false;
	}
}

void particle_update(float dt) {
	entitysys_iterate(&particles, &dt, particle_update_single);
}

static bool particle_render_single(void* obj, void* user) {
	struct Particle* p = (struct Particle*)obj;
	struct tesselator* tess = (struct tesselator*)user;

	if(distance2D(camera_x, camera_z, p->x, p->z) > settings.render_distance * settings.render_distance)
		return false;

	// Determine fade time based on particle type (snow fades slower than rain)
	float fade_time = (p->type == 254) ? 16.0F : 2.6F; // Snow (type 254) takes 16 seconds to fade, rain takes 2.6 (30% longer)
	float size = p->size / 2.0F * (1.0F - ((float)(window_time() - p->fade) / fade_time));

	if(p->type == 255 || p->type == 254) {
		tesselator_set_color(tess, p->color);

		// Render optimized 8-vertex cubes (2 faces) by default, or full 24-vertex cubes (6 faces) if 3D snow is enabled
		if(settings.snow_3d && p->type == 254) {
			// Full 6-face cube for 3D snow mode
			tesselator_addf_cube_face(tess, CUBE_FACE_X_N, p->x - size, p->y - size, p->z - size, size * 2.0F);
			tesselator_addf_cube_face(tess, CUBE_FACE_X_P, p->x - size, p->y - size, p->z - size, size * 2.0F);
			tesselator_addf_cube_face(tess, CUBE_FACE_Y_N, p->x - size, p->y - size, p->z - size, size * 2.0F);
			tesselator_addf_cube_face(tess, CUBE_FACE_Y_P, p->x - size, p->y - size, p->z - size, size * 2.0F);
			tesselator_addf_cube_face(tess, CUBE_FACE_Z_N, p->x - size, p->y - size, p->z - size, size * 2.0F);
			tesselator_addf_cube_face(tess, CUBE_FACE_Z_P, p->x - size, p->y - size, p->z - size, size * 2.0F);
		} else {
			// Optimized 2-face rendering for rain and non-3D snow
			tesselator_addf_cube_face(tess, CUBE_FACE_X_N, p->x - size, p->y - size, p->z - size, size * 2.0F);
			tesselator_addf_cube_face(tess, CUBE_FACE_X_P, p->x - size, p->y - size, p->z - size, size * 2.0F);
		}
	} else {
		struct kv6_t* casing = weapon_casing(p->type);

		if(casing) {
			matrix_push(matrix_model);
			matrix_identity(matrix_model);
			matrix_translate(matrix_model, p->x, p->y, p->z);
			matrix_pointAt(matrix_model, p->ox, p->oy * max(1.0F - (window_time() - p->fade) / 0.5F, 0.0F), p->oz);
			matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);
			matrix_upload();
			kv6_render(casing, TEAM_SPECTATOR);
			matrix_pop(matrix_model);
		}
	}

	return false;
}

void particle_render() {
	tesselator_clear(&particle_tesselator);

	entitysys_iterate(&particles, &particle_tesselator, particle_render_single);

	matrix_upload();
	tesselator_draw(&particle_tesselator, 1);
}

void particle_create_casing(struct Player* p) {
	entitysys_add(&particles,
				  &(struct Particle) {
					  .size = 0.1F,
					  .x = p->gun_pos.x,
					  .y = p->gun_pos.y,
					  .z = p->gun_pos.z,
					  .ox = p->orientation.x,
					  .oy = p->orientation.y,
					  .oz = p->orientation.z,
					  .vx = p->casing_dir.x * 3.5F,
					  .vy = p->casing_dir.y * 3.5F,
					  .vz = p->casing_dir.z * 3.5F,
					  .fade = window_time(),
					  .type = p->weapon,
					  .color = 0x00FFFF,
				  });
}

void particle_create(unsigned int color, float x, float y, float z, float velocity, float velocity_y, int amount,
					 float min_size, float max_size) {
	for(int k = 0; k < amount; k++) {
		float vx = (((float)rand() / (float)RAND_MAX) * 2.0F - 1.0F);
		float vy = (((float)rand() / (float)RAND_MAX) * 2.0F - 1.0F);
		float vz = (((float)rand() / (float)RAND_MAX) * 2.0F - 1.0F);
		float len = len3D(vx, vy, vz);

		vx = (vx / len) * velocity;
		vy = (vy / len) * velocity * velocity_y;
		vz = (vz / len) * velocity;

		entitysys_add(&particles,
					  &(struct Particle) {
						  .size = ((float)rand() / (float)RAND_MAX) * (max_size - min_size) + min_size,
						  .x = x,
						  .y = y,
						  .z = z,
						  .vx = vx,
						  .vy = vy,
						  .vz = vz,
						  .fade = window_time(),
						  .color = color,
						  .type = 255,
					  });
	}
}

void particle_create_rain(void) {
	rain_timer += 0.016F;
	if(rain_timer < 0.05F) {
		return;
	}
	rain_timer = 0.0F;

	struct Player* local = &players[local_player_id];
	if(!local || !local->connected) {
		return;
	}

	float player_x = local->pos.x;
	float player_y = local->pos.y;
	float player_z = local->pos.z;

	float rain_height = player_y + 40.0F; // Spawn twice as high (was 20.0F, now 40.0F)
	float render_dist = sqrtf(settings.render_distance * settings.render_distance);

	int particles_per_frame = 225; // Increased by 50% (was 150)

	for(int i = 0; i < particles_per_frame; i++) {
		float offset_x = (((float)rand() / (float)RAND_MAX) * 2.0F - 1.0F) * render_dist;
		float offset_z = (((float)rand() / (float)RAND_MAX) * 2.0F - 1.0F) * render_dist;

		float spawn_x = player_x + offset_x;
		float spawn_z = player_z + offset_z;

		if(spawn_x < 0 || spawn_x >= map_size_x || spawn_z < 0 || spawn_z >= map_size_z) {
			continue;
		}

		entitysys_add(&particles,
					  &(struct Particle) {
						  .size = 0.15F + ((float)rand() / (float)RAND_MAX) * 0.1F,
						  .x = spawn_x,
						  .y = rain_height,
						  .z = spawn_z,
						  .vx = 0.0F,
						  .vy = -15.0F - ((float)rand() / (float)RAND_MAX) * 5.0F,
						  .vz = 0.0F,
						  .fade = window_time(),
						  .color = rgba(0x00, 0x00, 0xCC, 0xFF),
						  .type = 255,
					  });
	}
}

void particle_create_snow(void) {
	snow_timer += 0.016F;
	if(snow_timer < 0.05F) {
		return;
	}
	snow_timer = 0.0F;

	struct Player* local = &players[local_player_id];
	if(!local || !local->connected) {
		return;
	}

	float player_x = local->pos.x;
	float player_y = local->pos.y;
	float player_z = local->pos.z;

	float snow_height = player_y + 40.0F; // Spawn twice as high (same as rain)
	float render_dist = sqrtf(settings.render_distance * settings.render_distance);

	int particles_per_frame = 225; // Increased by 50% (was 150)

	for(int i = 0; i < particles_per_frame; i++) {
		float offset_x = (((float)rand() / (float)RAND_MAX) * 2.0F - 1.0F) * render_dist;
		float offset_z = (((float)rand() / (float)RAND_MAX) * 2.0F - 1.0F) * render_dist;

		float spawn_x = player_x + offset_x;
		float spawn_z = player_z + offset_z;

		if(spawn_x < 0 || spawn_x >= map_size_x || spawn_z < 0 || spawn_z >= map_size_z) {
			continue;
		}

		entitysys_add(&particles,
					  &(struct Particle) {
						  .size = 0.15F + ((float)rand() / (float)RAND_MAX) * 0.1F,
						  .x = spawn_x,
						  .y = snow_height,
						  .z = spawn_z,
						  .vx = 0.0F,
						  .vy = -3.0F - ((float)rand() / (float)RAND_MAX) * 3.0F, // Tripled speed for snow
						  .vz = 0.0F,
						  .fade = window_time(),
						  .color = rgba(0xFF, 0xFF, 0xFF, 0xFF), // White color for snow
						  .type = 254, // Special type for snow (different fade time)
					  });
	}
}
