/*
 *   Copyright 2025 Franciszek Balcerak
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include "extent.h"
#include "alloc/include/macro.h"

/* Above or equal, if can, 1 node splits into 4 */
#define QUADTREE_SPLIT_THRESHOLD 17

/* Below or equal, if can, 4 nodes merge into 1 */
#define QUADTREE_MERGE_THRESHOLD 16

#define QUADTREE_DEDUPE_COLLISIONS 1

/* Do not modify unless you know what you are doing. Use octree.min_size. */
#define QUADTREE_MAX_DEPTH 30

/* Do not modify */
#define QUADTREE_DFS_LENGTH (QUADTREE_MAX_DEPTH * 3 + 1)

#define QUADTREE_MERGE_HT_SIZE MACRO_NEXT_OR_EQUAL_POWER_OF_2_CONST(QUADTREE_MERGE_THRESHOLD * 2)


typedef enum quadtree_node_type
{
	QUADTREE_NODE_TYPE_LEAF,
	MACRO_ENUM_BITS(QUADTREE_NODE_TYPE)
}
quadtree_node_type_t;


typedef union quadtree_node
{
	uint32_t next;

	struct
	{
		uint32_t head;
		uint32_t position_flags;
		uint32_t count;
		quadtree_node_type_t type;
	};

	uint32_t heads[4];
}
quadtree_node_t;


typedef struct quadtree_node_entity
{
	uint32_t next;
	uint32_t entity;
}
quadtree_node_entity_t;


#ifndef quadtree_entity_data


	typedef struct quadtree_entity_data
	{
		rect_extent_t rect_extent;
	}
	quadtree_entity_data_t;


	#define quadtree_entity_data quadtree_entity_data_t
	#define quadtree_get_entity_data_rect_extent(entity) (entity).rect_extent
#endif


typedef struct quadtree_entity
{
	union
	{
		quadtree_entity_data data;
		uint32_t next;
	};

	uint32_t query_tick;
	uint8_t update_tick;
	bool fully_in_node;
}
quadtree_entity_t;


#define quadtree_get_entity_rect_extent(entity)	\
quadtree_get_entity_data_rect_extent((entity)->data)


typedef struct quadtree_node_info
{
	uint32_t node_idx;
	half_extent_t extent;
}
quadtree_node_info_t;


typedef struct quadtree_ht_entry
{
	uint32_t next;
	uint32_t idx[2];
}
quadtree_ht_entry_t;


typedef struct quadtree_removal
{
	uint32_t entity_idx;
}
quadtree_removal_t;


typedef struct quadtree_node_removal
{
	uint32_t node_idx;
	uint32_t node_entity_idx;
	uint32_t prev_node_entity_idx;
}
quadtree_node_removal_t;


typedef struct quadtree_insertion
{
	quadtree_entity_data data;
}
quadtree_insertion_t;


typedef struct quadtree_reinsertion
{
	uint32_t entity_idx;
}
quadtree_reinsertion_t;


typedef enum quadtree_status
{
	QUADTREE_STATUS_CHANGED,
	QUADTREE_STATUS_NOT_CHANGED,
	MACRO_ENUM_BITS(QUADTREE_STATUS)
}
quadtree_status_t;


typedef struct quadtree quadtree_t;


typedef void
(*quadtree_query_fn_t)(
	quadtree_t* qt,
	uint32_t entity_idx,
	quadtree_entity_data* entity
	);


typedef void
(*quadtree_node_query_fn_t)(
	quadtree_t* qt,
	const quadtree_node_info_t* info
	);


typedef void
(*quadtree_collide_fn_t)(
	const quadtree_t* qt,
	quadtree_entity_data* entity_a,
	quadtree_entity_data* entity_b
	);


typedef quadtree_status_t
(*quadtree_update_fn_t)(
	quadtree_t* qt,
	uint32_t entity_idx,
	quadtree_entity_data* entity
	);


struct quadtree
{
	quadtree_node_t* nodes;
	quadtree_node_entity_t* node_entities;
	quadtree_entity_t* entities;
#if QUADTREE_DEDUPE_COLLISIONS == 1
	quadtree_ht_entry_t* ht_entries;
#endif
	quadtree_removal_t* removals;
	quadtree_node_removal_t* node_removals;
	quadtree_insertion_t* insertions;
	quadtree_reinsertion_t* reinsertions;
	uint32_t* merge_ht;

	uint32_t nodes_used;
	uint32_t nodes_size;

	uint32_t node_entities_used;
	uint32_t node_entities_size;

	uint32_t entities_used;
	uint32_t entities_size;

#if QUADTREE_DEDUPE_COLLISIONS == 1
	uint32_t ht_entries_used;
	uint32_t ht_entries_size;
#endif

	uint32_t removals_used;
	uint32_t removals_size;

	uint32_t node_removals_used;
	uint32_t node_removals_size;

	uint32_t insertions_used;
	uint32_t insertions_size;

	uint32_t reinsertions_used;
	uint32_t reinsertions_size;

	uint32_t query_tick;
	uint8_t update_tick;

	rect_extent_t rect_extent;
	half_extent_t half_extent;

	float min_size;
};


extern void
quadtree_init(
	quadtree_t* qt
	);


extern void
quadtree_free(
	quadtree_t* qt
	);


extern void
quadtree_insert(
	quadtree_t* qt,
	const quadtree_entity_data* data
	);


extern void
quadtree_remove(
	quadtree_t* qt,
	uint32_t entity_idx
	);


extern void
quadtree_normalize(
	quadtree_t* qt
	);


extern void
quadtree_update(
	quadtree_t* qt,
	quadtree_update_fn_t update_fn
	);


extern void
quadtree_query(
	quadtree_t* qt,
	rect_extent_t extent,
	quadtree_query_fn_t query_fn
	);


extern void
quadtree_query_nodes(
	quadtree_t* qt,
	rect_extent_t extent,
	quadtree_node_query_fn_t node_query_fn
	);


extern void
quadtree_collide(
	quadtree_t* qt,
	quadtree_collide_fn_t collide_fn
	);


extern uint32_t
quadtree_depth(
	const quadtree_t* qt
	);
