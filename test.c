#include "extent.h"

typedef struct entity_t
{
	rect_extent_t extent;
	float vx, vy;
}
entity_t;

#define quadtree_entity_data entity_t
#define quadtree_get_entity_data_rect_extent(entity) (entity).extent

#include "quadtree.h"
#include "alloc/include/alloc_ext.h"

#include "extent.c"
#include "window.c"
#include "alloc/src/sync.c"
#include "alloc/src/debug.c"
#include "alloc/src/alloc.c"
#include "quadtree.c"

#include <stdio.h>
#include <sched.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <tgmath.h>

#define ITER UINT32_C(50000)
#define RADIUS_ODDS 2000.0f
#define RADIUS_MIN 16.0f
#define RADIUS_MAX 768.0f
#define MIN_SIZE 16.0f
#define ARENA_WIDTH 10000.0f
#define ARENA_HEIGHT 10000.0f
#define MEASURE_TICKS 1000
#define INITIAL_VELOCITY 0.9f
#define BOUNDS_VELOCITY_LOSS 0.99f
#define CANT_ESCAPE_AREA 1
#define DO_THEM_QUERIES 1
#define QUERIES_NUM 1000

static quadtree_t qt = {0};

static double start, end;
static double time_elapsed;

typedef struct measurement_t
{
	double times[MEASURE_TICKS];
	uint64_t count;
}
measurement_t;

static measurement_t measure_normalize;
static measurement_t measure_collide;
static measurement_t measure_update;
static measurement_t measure_reinsertions;
static measurement_t measure_node_removals;
static measurement_t measure_rdtsc;
#if DO_THEM_QUERIES == 1
static measurement_t measure_query;
#endif

static float
randf(
	void
	)
{
	return rand() / (float) RAND_MAX;
}

static double
measure(
	measurement_t* measurement,
	double time_val
	)
{
	measurement->times[measurement->count] = time_val;
	measurement->count = (measurement->count + 1) % MEASURE_TICKS;
	if(measurement->count == 0)
	{
		double total = 0;
		for(uint64_t i = 0; i < MEASURE_TICKS; ++i)
		{
			total += measurement->times[i];
		}
		total /= MEASURE_TICKS;
		return total;
	}
	return 0;
}

static quadtree_status_t
update_entity(
	quadtree_t* qt,
	quadtree_entity_info_t info
	)
{
	entity_t* entity = info.data;

	entity->extent.min_x += entity->vx;
	entity->extent.max_x += entity->vx;
	entity->extent.min_y += entity->vy;
	entity->extent.max_y += entity->vy;

#if CANT_ESCAPE_AREA == 1
	if(entity->extent.min_x < qt->rect_extent.min_x)
	{
		float width = entity->extent.max_x - entity->extent.min_x;
		entity->extent.min_x = qt->rect_extent.min_x;
		entity->extent.max_x = qt->rect_extent.min_x + width;
		entity->vx = fabs(entity->vx) * BOUNDS_VELOCITY_LOSS;
	}
	else if(entity->extent.max_x > qt->rect_extent.max_x)
	{
		float width = entity->extent.max_x - entity->extent.min_x;
		entity->extent.max_x = qt->rect_extent.max_x;
		entity->extent.min_x = qt->rect_extent.max_x - width;
		entity->vx = -fabs(entity->vx) * BOUNDS_VELOCITY_LOSS;
	}

	if(entity->extent.min_y < qt->rect_extent.min_y)
	{
		float height = entity->extent.max_y - entity->extent.min_y;
		entity->extent.min_y = qt->rect_extent.min_y;
		entity->extent.max_y = qt->rect_extent.min_y + height;
		entity->vy = fabs(entity->vy) * BOUNDS_VELOCITY_LOSS;
	}
	else if(entity->extent.max_y > qt->rect_extent.max_y)
	{
		float height = entity->extent.max_y - entity->extent.min_y;
		entity->extent.max_y = qt->rect_extent.max_y;
		entity->extent.min_y = qt->rect_extent.max_y - height;
		entity->vy = -fabs(entity->vy) * BOUNDS_VELOCITY_LOSS;
	}
#else
	if(entity->extent.min_x < qt->rect_extent.min_x)
	{
		entity->vx *= BOUNDS_VELOCITY_LOSS;
		++entity->vx;
	}
	else if(entity->extent.max_x > qt->rect_extent.max_x)
	{
		entity->vx *= BOUNDS_VELOCITY_LOSS;
		--entity->vx;
	}

	if(entity->extent.min_y < qt->rect_extent.min_y)
	{
		entity->vy *= BOUNDS_VELOCITY_LOSS;
		++entity->vy;
	}
	else if(entity->extent.max_y > qt->rect_extent.max_y)
	{
		entity->vy *= BOUNDS_VELOCITY_LOSS;
		--entity->vy;
	}
#endif

	return QUADTREE_STATUS_CHANGED;
}

static void
collide_entities(
	const quadtree_t* qt,
	quadtree_entity_info_t info_a,
	quadtree_entity_info_t info_b
	)
{
	(void) qt;

	entity_t* entity_a = info_a.data;
	entity_t* entity_b = info_b.data;

	half_extent_t extent_a = rect_to_half_extent(entity_a->extent);
	half_extent_t extent_b = rect_to_half_extent(entity_b->extent);

	float diff_x = extent_a.x - extent_b.x;
	float diff_y = extent_a.y - extent_b.y;
	float overlap_x = (extent_a.w + extent_b.w) - fabs(diff_x);
	float overlap_y = (extent_a.h + extent_b.h) - fabs(diff_y);

	if(overlap_x > 0 && overlap_y > 0)
	{
		float size_a = extent_a.w * extent_a.h * 4.0f;
		float size_b = extent_b.w * extent_b.h * 4.0f;
		float total_size = size_a + size_b;

		if(overlap_x < overlap_y)
		{
			float push_a = overlap_x * (size_b / total_size);
			float push_b = overlap_x * (size_a / total_size);

			if(diff_x > 0)
			{
				entity_a->extent.min_x += push_a;
				entity_a->extent.max_x += push_a;
				entity_b->extent.min_x -= push_b;
				entity_b->extent.max_x -= push_b;
			}
			else
			{
				entity_a->extent.min_x -= push_a;
				entity_a->extent.max_x -= push_a;
				entity_b->extent.min_x += push_b;
				entity_b->extent.max_x += push_b;
			}

			float temp_vx = entity_a->vx;
			entity_a->vx = (entity_a->vx * (size_a - size_b) + 2 * size_b * entity_b->vx) / total_size;
			entity_b->vx = (entity_b->vx * (size_b - size_a) + 2 * size_a * temp_vx) / total_size;
		}
		else
		{
			float push_a = overlap_y * (size_b / total_size);
			float push_b = overlap_y * (size_a / total_size);

			if(diff_y > 0)
			{
				entity_a->extent.min_y += push_a;
				entity_a->extent.max_y += push_a;
				entity_b->extent.min_y -= push_b;
				entity_b->extent.max_y -= push_b;
			}
			else
			{
				entity_a->extent.min_y -= push_a;
				entity_a->extent.max_y -= push_a;
				entity_b->extent.min_y += push_b;
				entity_b->extent.max_y += push_b;
			}

			float temp_vy = entity_a->vy;
			entity_a->vy = (entity_a->vy * (size_a - size_b) + 2 * size_b * entity_b->vy) / total_size;
			entity_b->vy = (entity_b->vy * (size_b - size_a) + 2 * size_a * temp_vy) / total_size;
		}
	}
}

static void
draw_node(
	quadtree_t* qt,
	const quadtree_node_info_t* info
	)
{
	rect_t rect = to_screen(half_to_rect_extent(info->extent));

	for(int x = rect.min_x; x <= rect.max_x; ++x)
	{
		if(x & 1)
		{
			paint_pixel(x, rect.min_y, 0x40000000);
			paint_pixel(x, rect.max_y, 0x40000000);
		}
	}

	for(int y = rect.min_y; y <= rect.max_y; ++y)
	{
		if(y & 1)
		{
			paint_pixel(rect.min_x, y, 0x40000000);
			paint_pixel(rect.max_x, y, 0x40000000);
		}
	}
}

static void
draw_entity(
	quadtree_t* qt,
	quadtree_entity_info_t info
	)
{
	entity_t* entity = info.data;

	rect_t rect = to_screen(entity->extent);

	for(int x = rect.min_x; x <= rect.max_x; ++x)
	{
		paint_pixel(x, rect.min_y, 0xFF000000);
		paint_pixel(x, rect.max_y, 0xFF000000);
	}

	for(int y = rect.min_y; y <= rect.max_y; ++y)
	{
		paint_pixel(rect.min_x, y, 0xFF000000);
		paint_pixel(rect.max_x, y, 0xFF000000);
	}
}

float
gen_radius(
	void
	)
{
	float r = randf() * RADIUS_ODDS;
	float len = RADIUS_MAX - RADIUS_MIN;
	r = (1.0f / r) * len + RADIUS_MIN;
	r = fminf(r, RADIUS_MAX);
	return r;
}

static void
init(
	void
	)
{
	printf(
		"Simulation settings:\n"
		"Initial count:    %d\n"
		"Arena size:       %.01Lf x %.01Lf\n"
		"Initial Radius:   From %.01f to %.01f\n"
		, ITER, (long double) qt.half_extent.w * 2, (long double) qt.half_extent.h * 2, RADIUS_MIN, RADIUS_MAX
		);

	entity_t* rands = alloc_malloc(rands, ITER);
	assert(rands);

	for(int i = 0; i < ITER; ++i)
	{
		float dim[2];
		int idx = i & 1;

		float w = gen_radius();
		float temp = gen_radius();
		float h = w + (temp - temp * 0.5f);
		h = fminf(h, RADIUS_MAX);
		h = fmaxf(h, RADIUS_MIN);

		dim[idx] = w;
		dim[!idx] = h;

		w = dim[0];
		h = dim[1];

		float qtw = qt.half_extent.w * 2.0f - w;
		float qth = qt.half_extent.h * 2.0f - h;

		rands[i].extent.min_x = qt.rect_extent.min_x + qtw * randf();
		rands[i].extent.max_x = rands[i].extent.min_x + w;

		rands[i].extent.min_y = qt.rect_extent.min_y + qth * randf();
		rands[i].extent.max_y = rands[i].extent.min_y + h;

		rands[i].vx = (1 - 2 * randf()) * INITIAL_VELOCITY;
		rands[i].vy = (1 - 2 * randf()) * INITIAL_VELOCITY;
	}

	start = get_time();
	for(int i = 0; i < ITER; ++i)
	{
		quadtree_insert(&qt, rands + i);
	}
	end = get_time();
	printf("Insertion: %.02lfms\n", end - start);

	alloc_free(rands, ITER);
}

#if DO_THEM_QUERIES == 1

static void
query_ignore(
	quadtree_t* qt,
	quadtree_entity_info_t info
	)
{
	(void) qt;
	(void) info;
}

#endif

static void
tick(
	void
	)
{
	start = get_time();
	quadtree_collide(&qt, collide_entities);
	end = get_time();
	time_elapsed = measure(&measure_collide, end - start);
	if(time_elapsed)
	{
		printf("\nCollide: %.02lfms\n", time_elapsed);
	}

	start = get_time();
	quadtree_update(&qt, update_entity);
	end = get_time();
	time_elapsed = measure(&measure_update, end - start);
	if(time_elapsed)
	{
		printf("Update: %.02lfms\n", time_elapsed);
	}

	time_elapsed = measure(&measure_reinsertions, qt.reinsertions_used);
	if(time_elapsed)
	{
		printf("Reinsertions: %u\n", (uint32_t) time_elapsed);
	}

	time_elapsed = measure(&measure_node_removals, qt.node_removals_used);
	if(time_elapsed)
	{
		printf("Node Removals: %u\n", (uint32_t) time_elapsed);
	}

	start = get_time();
	quadtree_normalize(&qt);
	end = get_time();
	time_elapsed = measure(&measure_normalize, end - start);
	if(time_elapsed)
	{
		printf("Normalize: %.02lfms\n", time_elapsed);
		printf("Nodes: %u\n", qt.nodes_used);
		printf("Node entities: %u\n", qt.node_entities_used);
		printf("Entities: %u\n", qt.entities_used);
	}

#if DO_THEM_QUERIES == 1
	start = get_time();
	for(int i = 1; i <= QUERIES_NUM; ++i)
	{
		rect_extent_t extent =
		(rect_extent_t)
		{
			.min_x = qt.entities[i].data.extent.min_x - 1920.0f * 0.5f,
			.max_x = qt.entities[i].data.extent.max_x + 1920.0f * 0.5f,
			.min_y = qt.entities[i].data.extent.min_y - 1080.0f * 0.5f,
			.max_y = qt.entities[i].data.extent.max_y + 1080.0f * 0.5f
		};
		quadtree_query(&qt, extent, query_ignore);
	}
	end = get_time();
	time_elapsed = measure(&measure_query, end - start);
	if(time_elapsed)
	{
		printf("1k Queries: %.02lfms\n", time_elapsed);
	}
#endif
}

static inline uint64_t
rdtsc_start()
{
	uint32_t hi, lo;
	__asm__ volatile("CPUID\n\t"
					 "RDTSC\n\t"
					 "mov %%edx, %0\n\t"
					 "mov %%eax, %1\n\t"
					 : "=r" (hi), "=r" (lo)
					 :: "%rax", "%rbx", "%rcx", "%rdx");
	return ((uint64_t) hi << 32) | lo;
}

static inline uint64_t
rdtsc_end()
{
	uint32_t hi, lo;
	__asm__ volatile("RDTSCP\n\t"
					 "mov %%edx, %0\n\t"
					 "mov %%eax, %1\n\t"
					 "CPUID\n\t"
					 : "=r" (hi), "=r" (lo)
					 :: "%rax", "%rbx", "%rcx", "%rdx");
	return ((uint64_t) hi << 32) | lo;
}

int
main()
{
	draw_init();

	struct sched_param param;
	param.sched_priority = 99;
	sched_setscheduler(0, SCHED_FIFO, &param);

	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(0, &set);
	sched_setaffinity(0, sizeof(set), &set);

	uint64_t seed = get_time() * 100000;
	printf("Seed: %lu\n", seed);
	srand(5);

	qt.rect_extent =
	(rect_extent_t)
	{
		.min_x = -ARENA_WIDTH * 0.5f,
		.max_x =  ARENA_WIDTH * 0.5f,
		.min_y = -ARENA_HEIGHT * 0.5f,
		.max_y =  ARENA_HEIGHT * 0.5f
	};

	qt.half_extent =
	(half_extent_t)
	{
		.x = 0,
		.y = 0,
		.w = ARENA_WIDTH * 0.5f,
		.h = ARENA_HEIGHT * 0.5f
	};

	qt.min_size = MIN_SIZE;

	quadtree_init(&qt);

	init();

	while(1)
	{
		if(draw_start() != DRAW_STATE_OK)
		{
			break;
		}

		uint64_t start_tsc = rdtsc_start();
		tick();
		uint64_t end_tsc = rdtsc_end();
		time_elapsed = measure(&measure_rdtsc, end_tsc - start_tsc);
		if(time_elapsed)
		{
			printf("RDTSC: %.02lf\n", time_elapsed);
		}

		rect_extent_t rect_view = half_to_rect_extent(view);

		quadtree_query_nodes(&qt, rect_view, draw_node);
		quadtree_query(&qt, rect_view, draw_entity);

		draw_end();

		sched_yield();
	}

	draw_free();

	quadtree_free(&qt);

	return 0;
}
