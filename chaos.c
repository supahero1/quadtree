/* Randomized test of the whole quadtree API against a brute-force mirror.
 *
 * Usage: ./chaos [seed] [ticks] [paranoid]
 * Runs until it fails or until `ticks` ticks pass (0 or missing means forever).
 * Paranoid mode verifies the whole tree after every operation and logs them.
 * CHAOS_QUERIES_IN_CALLBACKS=0/1 and CHAOS_CHANGES_IN_CALLBACKS=0/1 force
 * those features off or on instead of picking them per run.
 * Everything is derived from the seed it prints, so a failing run can be
 * reproduced by passing that seed back.
 *
 * Positions only change inside update callbacks. Query and collide callbacks
 * may insert, remove and run more queries, but never move anything.
 */

#include "extent.c"


typedef struct chaos_data
{
	uint32_t id;
	uint32_t gen;
	float vx;
	float vy;
}
chaos_data_t;

#define quadtree_entity_data chaos_data_t


#include "alloc/src/arena.c"
#include "alloc/src/base.c"
#include "alloc/src/bootstrap.c"
#include "alloc/src/btree.c"
#include "alloc/src/consts.c"
#include "alloc/src/debug.c"
#include "alloc/src/huge.c"
#include "alloc/src/log.c"
#include "alloc/src/magic.c"
#include "alloc/src/platform.c"
#include "alloc/src/red_zone.c"
#include "alloc/src/report.c"
#include "alloc/src/sync.c"
#include "alloc/src/tcb.c"
#include "alloc/src/threads.c"
#include "heap.c"


/* Randomly takes the SIMD or the scalar path at every dispatch */
static int chaos_cpu_supports(const char* feature);
#define __builtin_cpu_supports(feature) chaos_cpu_supports(feature)
#include "quadtree.c"
#undef __builtin_cpu_supports


#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHAOS_MAX_ENTITIES 2048
#define CHAOS_MAX_DEPTH 2


static uint64_t rng_state;


static uint32_t
rng_u32(
	void
	)
{
	uint64_t z = (rng_state += UINT64_C(0x9E3779B97F4A7C15));
	z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
	return (z ^ (z >> 31)) >> 32;
}


static float
rng_f32(
	void
	)
{
	return (rng_u32() >> 8) * (1.0f / 16777216.0f);
}


static float
rng_range(
	float min,
	float max
	)
{
	return min + rng_f32() * (max - min);
}


static bool
rng_chance(
	float probability
	)
{
	return rng_f32() < probability;
}


static uint32_t
rng_below(
	uint32_t n
	)
{
	return rng_u32() % n;
}


static int
chaos_cpu_supports(
	const char* feature
	)
{
	if(rng_chance(0.5f))
	{
		return 0;
	}

	if(!strcmp(feature, "avx"))
	{
		return __builtin_cpu_supports("avx");
	}

	if(!strcmp(feature, "sse"))
	{
		return __builtin_cpu_supports("sse");
	}

	return 0;
}


typedef enum chaos_state : uint8_t
{
	CHAOS_FREE,
	CHAOS_PENDING_INSERT,
	CHAOS_LIVE,
	CHAOS_PENDING_REMOVE
}
chaos_state_t;


static quadtree_t qt;
static uint32_t max_entities;
static uint64_t tick;

/* Per-run feature switches, so a failure narrows down to what was enabled */
static bool queries_in_callbacks;
static bool changes_in_callbacks;

static chaos_state_t states[CHAOS_MAX_ENTITIES];
static uint32_t gens[CHAOS_MAX_ENTITIES];
static rect_extent_t extents[CHAOS_MAX_ENTITIES];
static chaos_data_t* pointers[CHAOS_MAX_ENTITIES];
static uint32_t pointer_epochs[CHAOS_MAX_ENTITIES];
static uint32_t free_ids[CHAOS_MAX_ENTITIES];
static uint32_t free_count;
static uint32_t entities_count;
static bool pending;

/* Bumped whenever data pointers handed out by the tree may have moved */
static uint32_t epoch = 1;
static uint32_t update_epoch;

/* One row per level of nested queries, so inner ones don't clobber outer ones */
static uint32_t stamps[CHAOS_MAX_DEPTH + 2][CHAOS_MAX_ENTITIES];
static uint32_t stamp;
static uint32_t query_level;
static uint32_t updated[CHAOS_MAX_ENTITIES];
static uint32_t update_stamp;
static uint8_t pairs[CHAOS_MAX_ENTITIES][CHAOS_MAX_ENTITIES];

static int depth;
static int in_update;
static bool paranoid;


static bool
chaos_visible(
	uint32_t id
	)
{
	return states[id] == CHAOS_LIVE || states[id] == CHAOS_PENDING_REMOVE;
}


static bool
chaos_extent_eq(
	rect_extent_t a,
	rect_extent_t b
	)
{
	return a.min_x == b.min_x && a.min_y == b.min_y && a.max_x == b.max_x && a.max_y == b.max_y;
}


static void
chaos_check_data(
	const rect_extent_t* extent,
	const chaos_data_t* data
	)
{
	hard_assert_lt(data->id, CHAOS_MAX_ENTITIES);
	hard_assert_true(chaos_visible(data->id));
	hard_assert_eq(data->gen, gens[data->id]);
	hard_assert_true(chaos_extent_eq(*extent, extents[data->id]));
}


/* The tree applies queued insertions and removals before any call that reads
 * it, except while it is calling back into us.
 */
static void
chaos_sync(
	void
	)
{
	if(!pending || depth || in_update)
	{
		return;
	}

	pending = false;
	++epoch;

	for(uint32_t id = 0; id < CHAOS_MAX_ENTITIES; ++id)
	{
		if(states[id] == CHAOS_PENDING_INSERT)
		{
			states[id] = CHAOS_LIVE;
		}
		else if(states[id] == CHAOS_PENDING_REMOVE)
		{
			states[id] = CHAOS_FREE;
			++gens[id];
			free_ids[free_count++] = id;
			--entities_count;
		}
	}
}


static rect_extent_t
chaos_random_size(
	void
	)
{
	float arena_w = qt.rect_extent.max_x - qt.rect_extent.min_x;
	float arena_h = qt.rect_extent.max_y - qt.rect_extent.min_y;
	float max_size = MACRO_MIN(arena_w, arena_h);
	float w, h;

	switch(rng_below(5))
	{

	case 0:
	{
		w = 0.0f;
		h = 0.0f;
		break;
	}

	case 1:
	{
		w = rng_range(0.0f, 2.0f);
		h = rng_range(0.0f, 2.0f);
		break;
	}

	case 2:
	case 3:
	{
		w = rng_range(0.0f, max_size / 50.0f);
		h = rng_range(0.0f, max_size / 50.0f);
		break;
	}

	default:
	{
		w = rng_range(max_size / 50.0f, max_size / 3.0f);
		h = rng_range(max_size / 50.0f, max_size / 3.0f);
		break;
	}

	}

	return (rect_extent_t){ .max_x = w, .max_y = h };
}


static rect_extent_t
chaos_place(
	float w,
	float h
	)
{
	rect_extent_t extent;
	extent.min_x = qt.rect_extent.min_x + rng_f32() * (qt.rect_extent.max_x - qt.rect_extent.min_x - w);
	extent.min_y = qt.rect_extent.min_y + rng_f32() * (qt.rect_extent.max_y - qt.rect_extent.min_y - h);
	extent.max_x = MACRO_MIN(extent.min_x + w, qt.rect_extent.max_x);
	extent.max_y = MACRO_MIN(extent.min_y + h, qt.rect_extent.max_y);
	return extent;
}


static void
chaos_insert(
	void
	)
{
	if(!free_count || entities_count >= max_entities)
	{
		return;
	}

	uint32_t id = free_ids[--free_count];
	hard_assert_eq(states[id], CHAOS_FREE);

	rect_extent_t size = chaos_random_size();
	float speed = (qt.rect_extent.max_x - qt.rect_extent.min_x) * rng_range(0.0f, 0.02f);

	states[id] = CHAOS_PENDING_INSERT;
	extents[id] = chaos_place(size.max_x, size.max_y);
	++entities_count;
	pending = true;

	if(paranoid && depth)
	{
		printf("  insert id %u depth %d update %d\n", id, depth, in_update);
	}

	quadtree_insert(&qt, extents[id], &(chaos_data_t){
		.id = id,
		.gen = gens[id],
		.vx = rng_range(-speed, speed),
		.vy = rng_range(-speed, speed)
		});
}


static void
chaos_remove(
	uint32_t id,
	chaos_data_t* data
	)
{
	hard_assert_eq(states[id], CHAOS_LIVE);
	hard_assert_eq(data->id, id);
	hard_assert_eq(data->gen, gens[id]);

	states[id] = CHAOS_PENDING_REMOVE;
	pending = true;

	if(paranoid)
	{
		uint32_t slot = data - qt.data;
		printf("  remove id %u slot %u in_nodes %u depth %d update %d\n", id, slot, qt.entities[slot].in_nodes, depth, in_update);
	}

	quadtree_remove(&qt, data);
}


static void chaos_query(void);


/* Insertions, removals and queries from inside a callback */
static void
chaos_nested(
	chaos_data_t* data,
	bool allow_query
	)
{
	if(depth >= CHAOS_MAX_DEPTH)
	{
		return;
	}

	++depth;

	if(changes_in_callbacks && rng_chance(0.05f))
	{
		chaos_insert();
	}

	if(changes_in_callbacks && data && rng_chance(0.03f) && states[data->id] == CHAOS_LIVE)
	{
		chaos_remove(data->id, data);
	}

	if(queries_in_callbacks && allow_query && rng_chance(0.02f))
	{
		chaos_query();
	}

	--depth;
}


typedef struct chaos_query_state
{
	uint32_t* stamps;
	uint32_t stamp;
	uint32_t found;
	uint32_t stop_after;
	bool stopped;
	bool complete;
	float last_value;
	float x;
	float y;
	bool nearest;
}
chaos_query_state_t;


static quadtree_status_t
chaos_query_fn(
	quadtree_t* tree,
	rect_extent_t* extent,
	chaos_data_t* data,
	void* user_data
	)
{
	(void) tree;

	chaos_query_state_t* query = user_data;
	uint32_t id = data->id;

	hard_assert_lt(id, CHAOS_MAX_ENTITIES);
	hard_assert_true(chaos_visible(id));
	hard_assert_eq(data->gen, gens[id]);
	hard_assert_true(chaos_extent_eq(*extent, extents[id]));
	hard_assert_neq(query->stamps[id], query->stamp);

	query->stamps[id] = query->stamp;
	++query->found;

	if(!in_update)
	{
		pointers[id] = data;
		pointer_epochs[id] = epoch;
	}

	if(query->nearest && !in_update)
	{
		float value = quadtree_point_to_extent_distance_sq(query->x, query->y, *extent);
		hard_assert_ge(value, query->last_value);
		query->last_value = value;
	}

	/* Queries share one tick to skip entities they already reported, so one
	 * nested in another would report entities twice.
	 */
	chaos_nested(data, false);

	if(query->found == query->stop_after)
	{
		query->stopped = true;
		return QUADTREE_STATUS_CHANGED;
	}

	return QUADTREE_STATUS_NOT_CHANGED;
}


static quadtree_status_t
chaos_node_query_fn(
	quadtree_t* tree,
	const quadtree_node_info_t* info,
	void* user_data
	)
{
	(void) tree;

	rect_extent_t* extent = user_data;
	hard_assert_le(info->extent.min_x, info->extent.max_x);
	hard_assert_le(info->extent.min_y, info->extent.max_y);
	hard_assert_true(rect_extent_intersects(info->extent, *extent));

	return QUADTREE_STATUS_NOT_CHANGED;
}


static void
chaos_begin_query(
	chaos_query_state_t* query
	)
{
	chaos_sync();

	hard_assert_lt(query_level, CHAOS_MAX_DEPTH + 2);

	*query =
	(chaos_query_state_t)
	{
		.stamps = stamps[query_level++],
		.stamp = ++stamp,
		.stop_after = rng_chance(0.1f) ? 1 + rng_below(8) : UINT32_MAX,
		.last_value = -INFINITY
	};
}


/* In an update the tree's nodes lag behind the moves, so only what was found is checked */
static void
chaos_end_query(
	chaos_query_state_t* query,
	uint32_t expected
	)
{
	--query_level;

	if(in_update || query->stopped)
	{
		hard_assert_le(query->found, expected);
	}
	else
	{
		hard_assert_eq(query->found, expected);
	}
}


static void
chaos_query(
	void
	)
{
	chaos_query_state_t query;
	chaos_begin_query(&query);

	uint32_t expected = 0;

	switch(rng_below(6))
	{

	case 0:
	{
		rect_extent_t size = chaos_random_size();
		rect_extent_t extent = chaos_place(size.max_x, size.max_y);

		if(rng_chance(0.1f))
		{
			extent = qt.rect_extent;
		}

		quadtree_query_rect(&qt, extent, chaos_query_fn, &query);

		for(uint32_t id = 0; id < CHAOS_MAX_ENTITIES; ++id)
		{
			if(chaos_visible(id) && rect_extent_intersects(extents[id], extent))
			{
				++expected;
			}
			else
			{
				hard_assert_neq(query.stamps[id], query.stamp);
			}
		}

		break;
	}

	case 1:
	{
		rect_extent_t point = chaos_place(0.0f, 0.0f);
		float radius = rng_range(0.0f, (qt.rect_extent.max_x - qt.rect_extent.min_x) * 0.3f);

		quadtree_query_circle(&qt, point.min_x, point.min_y, radius, chaos_query_fn, &query);

		for(uint32_t id = 0; id < CHAOS_MAX_ENTITIES; ++id)
		{
			if(chaos_visible(id) &&
				quadtree_point_to_extent_distance_sq(point.min_x, point.min_y, extents[id]) <= radius * radius)
			{
				++expected;
			}
			else
			{
				hard_assert_neq(query.stamps[id], query.stamp);
			}
		}

		break;
	}

	case 2:
	case 3:
	{
		uint32_t max_results = rng_below(16);
		float max_distance = -1.0f;
		rect_extent_t extent;

		if(rng_chance(0.5f))
		{
			rect_extent_t size = chaos_random_size();
			extent = chaos_place(size.max_x, size.max_y);
		}
		else
		{
			extent = chaos_place(0.0f, 0.0f);
			if(rng_chance(0.5f))
			{
				max_distance = rng_range(0.0f, (qt.rect_extent.max_x - qt.rect_extent.min_x) * 0.3f);
			}
		}

		query.nearest = true;
		query.x = (extent.min_x + extent.max_x) * 0.5f;
		query.y = (extent.min_y + extent.max_y) * 0.5f;

		if(max_distance != -1.0f || rng_chance(0.5f))
		{
			quadtree_nearest_circle(&qt, query.x, query.y, max_distance, max_results, chaos_query_fn, &query);
			extent = (rect_extent_t){ .min_x = -INFINITY, .min_y = -INFINITY, .max_x = INFINITY, .max_y = INFINITY };
		}
		else
		{
			quadtree_nearest_rect(&qt, extent, max_results, chaos_query_fn, &query);
		}

		float max_distance_sq = max_distance < 0.0f ? INFINITY : max_distance * max_distance;
		uint32_t candidates = 0;

		for(uint32_t id = 0; id < CHAOS_MAX_ENTITIES; ++id)
		{
			if(!chaos_visible(id) || !rect_extent_intersects(extents[id], extent))
			{
				hard_assert_neq(query.stamps[id], query.stamp);
				continue;
			}

			float value = quadtree_point_to_extent_distance_sq(query.x, query.y, extents[id]);
			if(value > max_distance_sq)
			{
				hard_assert_neq(query.stamps[id], query.stamp);
				continue;
			}

			++candidates;

			if(!in_update && query.stamps[id] != query.stamp && !query.stopped)
			{
				hard_assert_ge(value, query.last_value);
			}
		}

		expected = MACRO_MIN(candidates, max_results);
		break;
	}

	case 4:
	{
		rect_extent_t start = chaos_place(0.0f, 0.0f);
		rect_extent_t end = chaos_place(0.0f, 0.0f);
		float dx = end.min_x - start.min_x;
		float dy = end.min_y - start.min_y;

		if(rng_chance(0.2f))
		{
			dx = 0.0f;
		}
		else if(rng_chance(0.2f))
		{
			dy = 0.0f;
		}

		quadtree_raycast(&qt, start.min_x, start.min_y, dx, dy, chaos_query_fn, &query);

		if(!dx && !dy)
		{
			expected = query.found;
			break;
		}

		float inv_dx = 1.0f / dx;
		float inv_dy = 1.0f / dy;

		for(uint32_t id = 0; id < CHAOS_MAX_ENTITIES; ++id)
		{
			float t_min;
			if(chaos_visible(id) &&
				quadtree_ray_hits(start.min_x, start.min_y, inv_dx, inv_dy, extents[id], &t_min))
			{
				++expected;
			}
			else
			{
				hard_assert_neq(query.stamps[id], query.stamp);
			}
		}

		break;
	}

	default:
	{
		rect_extent_t size = chaos_random_size();
		rect_extent_t extent = chaos_place(size.max_x, size.max_y);

		if(rng_chance(0.5f))
		{
			quadtree_query_nodes_rect(&qt, extent, chaos_node_query_fn, &extent);
		}
		else
		{
			float radius = (size.max_x + size.max_y) * 0.5f;
			float x = extent.min_x;
			float y = extent.min_y;

			extent =
			(rect_extent_t)
			{
				.min_x = x - radius,
				.min_y = y - radius,
				.max_x = x + radius,
				.max_y = y + radius
			};

			quadtree_query_nodes_circle(&qt, x, y, radius, chaos_node_query_fn, &extent);
		}

		expected = query.found;
		break;
	}

	}

	chaos_end_query(&query, expected);
}


static void
chaos_verify_all(
	void
	)
{
	chaos_query_state_t query;
	chaos_begin_query(&query);
	query.stop_after = UINT32_MAX;

	int saved_depth = depth;
	depth = CHAOS_MAX_DEPTH;
	quadtree_query_rect(&qt, qt.rect_extent, chaos_query_fn, &query);
	depth = saved_depth;

	uint32_t expected = 0;

	for(uint32_t id = 0; id < CHAOS_MAX_ENTITIES; ++id)
	{
		if(chaos_visible(id))
		{
			++expected;
			hard_assert_eq(query.stamps[id], query.stamp);
		}
	}

	chaos_end_query(&query, expected);
	hard_assert_eq(qt.live_entities, expected);
}


static void
chaos_collide_fn(
	const quadtree_t* tree,
	rect_extent_t* extent_a,
	chaos_data_t* data_a,
	rect_extent_t* extent_b,
	chaos_data_t* data_b,
	void* user_data
	)
{
	(void) tree;
	(void) user_data;

	chaos_check_data(extent_a, data_a);
	chaos_check_data(extent_b, data_b);
	hard_assert_neq(data_a->id, data_b->id);

	uint32_t a = MACRO_MIN(data_a->id, data_b->id);
	uint32_t b = MACRO_MAX(data_a->id, data_b->id);
	hard_assert_eq(pairs[a][b], 0);
	pairs[a][b] = 1;

	chaos_nested(rng_chance(0.5f) ? data_a : data_b, true);
}


static void
chaos_collide(
	void
	)
{
	chaos_sync();
	memset(pairs, 0, sizeof(pairs));

	quadtree_collide(&qt, chaos_collide_fn, NULL);

	for(uint32_t a = 0; a < CHAOS_MAX_ENTITIES; ++a)
	{
		if(!chaos_visible(a))
		{
			continue;
		}

		for(uint32_t b = a + 1; b < CHAOS_MAX_ENTITIES; ++b)
		{
			bool expected = chaos_visible(b) && rect_extent_intersects(extents[a], extents[b]);
			hard_assert_eq(pairs[a][b], expected);
		}
	}
}


static void
chaos_bounce(
	rect_extent_t* extent,
	chaos_data_t* data
	)
{
	if(extent->min_x < qt.rect_extent.min_x)
	{
		float shift = qt.rect_extent.min_x - extent->min_x;
		extent->min_x += shift;
		extent->max_x += shift;
		data->vx = fabsf(data->vx);
	}
	else if(extent->max_x > qt.rect_extent.max_x)
	{
		float shift = extent->max_x - qt.rect_extent.max_x;
		extent->min_x -= shift;
		extent->max_x -= shift;
		data->vx = -fabsf(data->vx);
	}

	if(extent->min_y < qt.rect_extent.min_y)
	{
		float shift = qt.rect_extent.min_y - extent->min_y;
		extent->min_y += shift;
		extent->max_y += shift;
		data->vy = fabsf(data->vy);
	}
	else if(extent->max_y > qt.rect_extent.max_y)
	{
		float shift = extent->max_y - qt.rect_extent.max_y;
		extent->min_y -= shift;
		extent->max_y -= shift;
		data->vy = -fabsf(data->vy);
	}

	extent->min_x = MACRO_MAX(extent->min_x, qt.rect_extent.min_x);
	extent->min_y = MACRO_MAX(extent->min_y, qt.rect_extent.min_y);
	extent->max_x = MACRO_MIN(extent->max_x, qt.rect_extent.max_x);
	extent->max_y = MACRO_MIN(extent->max_y, qt.rect_extent.max_y);
}


static quadtree_status_t
chaos_update_fn(
	quadtree_t* tree,
	rect_extent_t* extent,
	chaos_data_t* data,
	void* user_data
	)
{
	(void) tree;
	(void) user_data;

	chaos_check_data(extent, data);
	hard_assert_neq(updated[data->id], update_stamp);
	updated[data->id] = update_stamp;

	if(changes_in_callbacks && rng_chance(0.01f))
	{
		uint32_t other = rng_below(CHAOS_MAX_ENTITIES);
		if(states[other] == CHAOS_LIVE && pointer_epochs[other] == update_epoch)
		{
			chaos_remove(other, pointers[other]);
		}
	}

	chaos_nested(data, true);

	float choice = rng_f32();

	if(choice < 0.15f)
	{
		return QUADTREE_STATUS_NOT_CHANGED;
	}

	if(choice < 0.2f)
	{
		rect_extent_t size = chaos_random_size();
		*extent = chaos_place(size.max_x, size.max_y);
	}
	else if(choice < 0.25f)
	{
		rect_extent_t size = chaos_random_size();
		extent->max_x = extent->min_x + size.max_x;
		extent->max_y = extent->min_y + size.max_y;
	}
	else
	{
		extent->min_x += data->vx;
		extent->max_x += data->vx;
		extent->min_y += data->vy;
		extent->max_y += data->vy;
	}

	chaos_bounce(extent, data);
	extents[data->id] = *extent;

	return QUADTREE_STATUS_CHANGED;
}


static void
chaos_update(
	void
	)
{
	update_epoch = pending ? UINT32_MAX : epoch;
	chaos_sync();

	++update_stamp;
	++in_update;

	quadtree_update(&qt, chaos_update_fn, NULL);

	--in_update;
	++epoch;

	for(uint32_t id = 0; id < CHAOS_MAX_ENTITIES; ++id)
	{
		if(states[id] == CHAOS_PENDING_INSERT)
		{
			hard_assert_neq(updated[id], update_stamp);
		}
		else if(chaos_visible(id))
		{
			hard_assert_eq(updated[id], update_stamp);
		}
	}
}


static void
chaos_reset(
	void
	)
{
	if(qt.nodes)
	{
		quadtree_free(&qt);
	}

	float half_w = rng_chance(0.5f) ? rng_range(10.0f, 200000.0f) : (float)(1 << rng_below(18));
	float half_h = rng_chance(0.2f) ? rng_range(10.0f, 200000.0f) : half_w;
	float x = rng_chance(0.5f) ? 0.0f : rng_range(-half_w, half_w);
	float y = rng_chance(0.5f) ? 0.0f : rng_range(-half_h, half_h);

	qt = (quadtree_t){0};
	qt.rect_extent = (rect_extent_t){ .min_x = x - half_w, .min_y = y - half_h, .max_x = x + half_w, .max_y = y + half_h };
	qt.split_threshold = 1 + rng_below(64);
	/* Up to 4^8 leaves, all of which a big entity can be in when split_threshold is 1 */
	qt.max_depth = 1 + rng_below(9);
	qt.min_size = rng_chance(0.5f) ? 0.0f : rng_range(0.1f, MACRO_MIN(half_w, half_h) * 0.1f);

	if(rng_chance(0.3f))
	{
		qt.merge_threshold = rng_below(qt.split_threshold);
		qt.merge_threshold_set = true;
	}

	quadtree_init(&qt);

	max_entities = 1 + rng_below(CHAOS_MAX_ENTITIES);
	queries_in_callbacks = rng_chance(0.5f);
	changes_in_callbacks = rng_chance(0.5f);

	const char* force = getenv("CHAOS_QUERIES_IN_CALLBACKS");
	if(force)
	{
		queries_in_callbacks = atoi(force);
	}

	force = getenv("CHAOS_CHANGES_IN_CALLBACKS");
	if(force)
	{
		changes_in_callbacks = atoi(force);
	}

	memset(states, 0, sizeof(states));
	free_count = 0;
	for(uint32_t id = CHAOS_MAX_ENTITIES; id > 0; --id)
	{
		free_ids[free_count++] = id - 1;
		++gens[id - 1];
	}

	entities_count = 0;
	pending = false;
	++epoch;

	printf("tick %lu: arena (%g, %g) to (%g, %g), split %u, merge %u, depth %u, min size %g, "
		"max entities %u, queries in callbacks %d, changes in callbacks %d\n",
		tick, qt.rect_extent.min_x, qt.rect_extent.min_y, qt.rect_extent.max_x, qt.rect_extent.max_y,
		qt.split_threshold, qt.merge_threshold, qt.max_depth, qt.min_size,
		max_entities, queries_in_callbacks, changes_in_callbacks);
	fflush(stdout);
}


int
main(
	int argc,
	char** argv
	)
{
	uint64_t seed = argc > 1 ? strtoull(argv[1], NULL, 0) : (uint64_t) time(NULL) ^ ((uint64_t) clock() << 32);
	uint64_t ticks = argc > 2 ? strtoull(argv[2], NULL, 0) : 0;
	paranoid = argc > 3 && atoi(argv[3]);

	printf("seed %lu\n", seed);
	fflush(stdout);

	rng_state = seed;
	chaos_reset();

	while(!ticks || tick < ticks)
	{
		uint32_t ops = 1 + rng_below(12);

		while(ops--)
		{
			uint32_t op = rng_below(100);

			if(paranoid)
			{
				printf("tick %lu op %u entities %u pending %d\n", tick, op, entities_count, pending);
				fflush(stdout);
			}

			if(op < 20)
			{
				uint32_t count = rng_below(max_entities / 4 + 2);
				while(count--)
				{
					chaos_insert();
				}
			}
			else if(op < 30)
			{
				chaos_verify_all();

				uint32_t count = rng_below(entities_count / 8 + 2);
				while(count--)
				{
					uint32_t id = rng_below(CHAOS_MAX_ENTITIES);
					if(states[id] == CHAOS_LIVE && pointer_epochs[id] == epoch)
					{
						chaos_remove(id, pointers[id]);
					}
				}
			}
			else if(op < 55)
			{
				chaos_query();
			}
			else if(op < 70)
			{
				chaos_collide();
			}
			else if(op < 90)
			{
				chaos_update();
			}
			else if(op < 97)
			{
				chaos_sync();
				++epoch;
				quadtree_normalize(&qt);
			}
			else if(op < 99)
			{
				chaos_sync();
				quadtree_check(&qt);
			}
			else if(rng_chance(0.1f))
			{
				chaos_reset();
			}

			if(paranoid && !pending)
			{
				chaos_verify_all();
				quadtree_check(&qt);
			}
		}

		chaos_update();
		chaos_sync();
		++epoch;
		quadtree_normalize(&qt);
		chaos_verify_all();
		quadtree_check(&qt);

		++tick;
		if(!(tick % 1000))
		{
			printf("tick %lu: %u entities, %u nodes\n", tick, entities_count, qt.nodes_used);
			fflush(stdout);
		}
	}

	quadtree_free(&qt);
	printf("passed %lu ticks\n", tick);

	return 0;
}
