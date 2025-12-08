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

#include "quadtree.h"
#include "alloc/include/debug.h"
#include "alloc/include/alloc_ext.h"

#include <math.h>
#include <string.h>


void
quadtree_init(
	quadtree_t* qt
	)
{
	assert_not_null(qt);
	assert_null(qt->nodes);

	if(!qt->split_threshold)
	{
		qt->split_threshold = 13;
	}

	if(!qt->merge_threshold && !qt->merge_threshold_set)
	{
		qt->merge_threshold = 12;
	}
	qt->merge_threshold = MACRO_MIN(qt->merge_threshold, qt->split_threshold - 1);

	if(!qt->max_depth)
	{
		qt->max_depth = 30;
	}

	qt->dfs_length = qt->max_depth * 3 + 1;

	qt->merge_ht_size = MACRO_NEXT_OR_EQUAL_POWER_OF_2(qt->merge_threshold * 2);

	if(!qt->min_size)
	{
		qt->min_size = 1.0f;
	}

	qt->nodes = alloc_malloc(qt->nodes, 1);
	assert_not_null(qt->nodes);

	qt->merge_ht = alloc_malloc(qt->merge_ht, qt->merge_ht_size);
	assert_ptr(qt->merge_ht, qt->merge_ht_size);

	qt->nodes_used = 1;
	qt->nodes_size = 1;

	qt->node_entities_used = 1;

	qt->entities_used = 1;

#if QUADTREE_DEDUPE_COLLISIONS == 1
	qt->ht_entries_used = 1;
#endif

	qt->nodes[0].head = 0;
	qt->nodes[0].position_flags = 0b1111; /* TRBL */
	qt->nodes[0].count = 0;
	qt->nodes[0].type = QUADTREE_NODE_TYPE_LEAF;
}


void
quadtree_free(
	quadtree_t* qt
	)
{
	assert_not_null(qt);

	alloc_free(qt->merge_ht, qt->merge_ht_size);
	alloc_free(qt->reinsertions, qt->reinsertions_size);
	alloc_free(qt->insertions, qt->insertions_size);
	alloc_free(qt->node_removals, qt->node_removals_size);
	alloc_free(qt->removals, qt->removals_size);
#if QUADTREE_DEDUPE_COLLISIONS == 1
	alloc_free(qt->ht_entries, qt->ht_entries_size);
#endif
	alloc_free(qt->entities, qt->entities_size);
	alloc_free(qt->node_entities.flags, qt->node_entities_size);
	alloc_free(qt->node_entities.entities, qt->node_entities_size);
	alloc_free(qt->node_entities.next, qt->node_entities_size);
	alloc_free(qt->nodes, qt->nodes_size);
}


#define quadtree_fill_node_default(_node_idx, _extent)	\
(quadtree_node_info_t)									\
{														\
	.node_idx = _node_idx,								\
	.extent = _extent									\
}

#define quadtree_fill_node(...)			\
quadtree_fill_node_default(__VA_ARGS__)

#define quadtree_descend(_extent, ...)				\
do													\
{													\
	float half_w = info.extent.w * 0.5f;			\
	float half_h = info.extent.h * 0.5f;			\
													\
	if(_extent.min_x <= info.extent.x)				\
	{												\
		if(_extent.min_y <= info.extent.y)			\
		{											\
			*(node_info++) = quadtree_fill_node(	\
				node->heads[0],						\
				((half_extent_t)					\
				{									\
					.x = info.extent.x - half_w,	\
					.y = info.extent.y - half_h,	\
					.w = half_w,					\
					.h = half_h						\
				}) __VA_OPT__(,)					\
				__VA_ARGS__							\
				);									\
		}											\
		if(_extent.max_y >= info.extent.y)			\
		{											\
			*(node_info++) = quadtree_fill_node(	\
				node->heads[1],						\
				((half_extent_t)					\
				{									\
					.x = info.extent.x - half_w,	\
					.y = info.extent.y + half_h,	\
					.w = half_w,					\
					.h = half_h						\
				}) __VA_OPT__(,)					\
				__VA_ARGS__							\
				);									\
		}											\
	}												\
	if(_extent.max_x >= info.extent.x)				\
	{												\
		if(_extent.min_y <= info.extent.y)			\
		{											\
			*(node_info++) = quadtree_fill_node(	\
				node->heads[2],						\
				((half_extent_t)					\
				{									\
					.x = info.extent.x + half_w,	\
					.y = info.extent.y - half_h,	\
					.w = half_w,					\
					.h = half_h						\
				}) __VA_OPT__(,)					\
				__VA_ARGS__							\
				);									\
		}											\
		if(_extent.max_y >= info.extent.y)			\
		{											\
			*(node_info++) = quadtree_fill_node(	\
				node->heads[3],						\
				((half_extent_t)					\
				{									\
					.x = info.extent.x + half_w,	\
					.y = info.extent.y + half_h,	\
					.w = half_w,					\
					.h = half_h						\
				}) __VA_OPT__(,)					\
				__VA_ARGS__							\
				);									\
		}											\
	}												\
}													\
while(0)

#define quadtree_descend_all(...)			\
do											\
{											\
	float half_w = info.extent.w * 0.5f;	\
	float half_h = info.extent.h * 0.5f;	\
											\
	*(node_info++) = quadtree_fill_node(	\
		node->heads[0],						\
		((half_extent_t)					\
		{									\
			.x = info.extent.x - half_w,	\
			.y = info.extent.y - half_h,	\
			.w = half_w,					\
			.h = half_h						\
		}) __VA_OPT__(,)					\
		__VA_ARGS__							\
		);									\
											\
	*(node_info++) = quadtree_fill_node(	\
		node->heads[1],						\
		((half_extent_t)					\
		{									\
			.x = info.extent.x - half_w,	\
			.y = info.extent.y + half_h,	\
			.w = half_w,					\
			.h = half_h						\
		}) __VA_OPT__(,)					\
		__VA_ARGS__							\
		);									\
											\
	*(node_info++) = quadtree_fill_node(	\
		node->heads[2],						\
		((half_extent_t)					\
		{									\
			.x = info.extent.x + half_w,	\
			.y = info.extent.y - half_h,	\
			.w = half_w,					\
			.h = half_h						\
		}) __VA_OPT__(,)					\
		__VA_ARGS__							\
		);									\
											\
	*(node_info++) = quadtree_fill_node(	\
		node->heads[3],						\
		((half_extent_t)					\
		{									\
			.x = info.extent.x + half_w,	\
			.y = info.extent.y + half_h,	\
			.w = half_w,					\
			.h = half_h						\
		}) __VA_OPT__(,)					\
		__VA_ARGS__							\
		);									\
}											\
while(0)

#define quadtree_descend_extentless(...)	\
do											\
{											\
	*(node_info++) = quadtree_fill_node(	\
		node->heads[0],						\
		((half_extent_t){0}) __VA_OPT__(,)	\
		__VA_ARGS__							\
		);									\
											\
	*(node_info++) = quadtree_fill_node(	\
		node->heads[1],						\
		((half_extent_t){0}) __VA_OPT__(,)	\
		__VA_ARGS__							\
		);									\
											\
	*(node_info++) = quadtree_fill_node(	\
		node->heads[2],						\
		((half_extent_t){0}) __VA_OPT__(,)	\
		__VA_ARGS__							\
		);									\
											\
	*(node_info++) = quadtree_fill_node(	\
		node->heads[3],						\
		((half_extent_t){0}) __VA_OPT__(,)	\
	  __VA_ARGS__							\
	  );									\
}											\
while(0)

#define quadtree_reset_flags()								\
do															\
{															\
	uint8_t flags = 0;										\
	if(entity_extent.max_y >= node_extent.max_y &&			\
		!(node->position_flags & 0b1000)) flags |= 0b1000;	\
	if(entity_extent.max_x >= node_extent.max_x &&			\
		!(node->position_flags & 0b0100)) flags |= 0b0100;	\
	if(entity_extent.min_y <= node_extent.min_y &&			\
		!(node->position_flags & 0b0010)) flags |= 0b0010;	\
	if(entity_extent.min_x <= node_extent.min_x &&			\
		!(node->position_flags & 0b0001)) flags |= 0b0001;	\
	node_entities.flags[node_entity_idx] = flags;			\
}															\
while(0);


void
quadtree_insert(
	quadtree_t* qt,
	const quadtree_entity_data* data
	)
{
	assert_not_null(qt);
	assert_not_null(data);

	if(qt->insertions_used >= qt->insertions_size)
	{
		uint32_t new_size = (qt->insertions_used << 1) | 1;

		qt->insertions = alloc_remalloc(qt->insertions, qt->insertions_size, new_size);
		assert_not_null(qt->insertions);

		qt->insertions_size = new_size;
	}

	uint32_t insertion_idx = qt->insertions_used++;
	quadtree_insertion_t* insertion = qt->insertions + insertion_idx;

	insertion->data = *data;

	qt->normalization |= QUADTREE_NOT_NORMALIZED_HARD;
}


void
quadtree_remove(
	quadtree_t* qt,
	uint32_t entity_idx
	)
{
	assert_not_null(qt);
	assert_gt(entity_idx, 0);
	assert_lt(entity_idx, qt->entities_used);

	if(qt->removals_used >= qt->removals_size)
	{
		uint32_t new_size = (qt->removals_used << 1) | 1;

		qt->removals = alloc_remalloc(qt->removals, qt->removals_size, new_size);
		assert_not_null(qt->removals);

		qt->removals_size = new_size;
	}

	uint32_t removal_idx = qt->removals_used++;
	quadtree_removal_t* removal = qt->removals + removal_idx;

	removal->entity_idx = entity_idx;

	qt->normalization |= QUADTREE_NOT_NORMALIZED_HARD;
}


void
quadtree_normalize(
	quadtree_t* qt
	)
{
	assert_not_null(qt);

	if(qt->normalization == QUADTREE_NORMALIZED)
	{
		return;
	}

	qt->normalization = QUADTREE_NORMALIZED;

	quadtree_node_t* nodes = qt->nodes;
	quadtree_node_entities_t node_entities = qt->node_entities;
	quadtree_entity_t* entities = qt->entities;

	uint32_t free_node_entity = 0;
	uint32_t node_entities_used = qt->node_entities_used;
	uint32_t node_entities_size = qt->node_entities_size;

	uint32_t free_entity = 0;
	uint32_t entities_used = qt->entities_used;
	uint32_t entities_size = qt->entities_size;

	quadtree_node_info_t node_infos[qt->dfs_length];
	quadtree_node_info_t* node_info;


	if(qt->node_removals_used)
	{
		quadtree_node_removal_t* node_removals = qt->node_removals;
		quadtree_node_removal_t* node_removal = node_removals + qt->node_removals_used - 1;

		while(node_removal >= node_removals)
		{
			uint32_t node_idx = node_removal->node_idx;
			quadtree_node_t* node = nodes + node_idx;

			uint32_t node_entity_idx = node_removal->node_entity_idx;
			uint32_t* node_entity_next = node_entities.next + node_entity_idx;
			uint32_t entity_idx = node_removal->entity_idx;

			uint32_t next = *node_entity_next;
			quadtree_entity_t* entity = entities + entity_idx;

			if(node->head != node_entity_idx)
			{
				*(node_entity_next - 1) = next;
				if(!next)
				{
					node_entities.entities[node_entity_idx - 1].is_last = true;
				}
			}
			else
			{
				node->head = next;
			}

			--node->count;
			--entity->in_nodes_minus_one;

			*node_entity_next = free_node_entity;
			free_node_entity = node_entity_idx;

			--node_removal;
		}

		alloc_free(node_removals, qt->node_removals_size);
		qt->node_removals = NULL;
		qt->node_removals_used = 0;
		qt->node_removals_size = 0;
	}


	{
		quadtree_reinsertion_t* reinsertions = qt->reinsertions;
		quadtree_reinsertion_t* reinsertion = reinsertions;
		quadtree_reinsertion_t* reinsertion_end = reinsertion + qt->reinsertions_used;

		while(reinsertion != reinsertion_end)
		{
			uint32_t entity_idx = reinsertion->entity_idx;
			quadtree_entity_t* entity = entities + entity_idx;

			rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);
			uint32_t in_nodes = 0;

			node_info = node_infos;

			*(node_info++) =
			(quadtree_node_info_t)
			{
				.node_idx = 0,
				.extent = qt->half_extent
			};

			do
			{
				quadtree_node_info_t info = *(--node_info);
				quadtree_node_t* node = nodes + info.node_idx;

				if(node->type != QUADTREE_NODE_TYPE_LEAF)
				{
					quadtree_descend(entity_extent);
					continue;
				}

				rect_extent_t node_extent = half_to_rect_extent(info.extent);
				uint32_t node_entity_idx = node->head;

				++in_nodes;

				while(node_entity_idx)
				{
					if(node_entities.entities[node_entity_idx].index == entity_idx)
					{
						goto goto_skip;
					}

					node_entity_idx = node_entities.next[node_entity_idx];
				}

				if(free_node_entity)
				{
					node_entity_idx = free_node_entity;
					free_node_entity = node_entities.next[node_entity_idx];
				}
				else
				{
					if(node_entities_used >= node_entities_size)
					{
						uint32_t new_size = (node_entities_used << 1) | 1;

						node_entities.next = alloc_remalloc(node_entities.next, node_entities_size, new_size);
						assert_not_null(node_entities.next);

						node_entities.entities = alloc_remalloc(node_entities.entities, node_entities_size, new_size);
						assert_not_null(node_entities.entities);

						node_entities.flags = alloc_remalloc(node_entities.flags, node_entities_size, new_size);
						assert_not_null(node_entities.flags);

						node_entities_size = new_size;
					}

					node_entity_idx = node_entities_used++;
				}

				node_entities.next[node_entity_idx] = node->head;
				node_entities.entities[node_entity_idx].is_last = !node->head;
				node_entities.entities[node_entity_idx].index = entity_idx;
				node->head = node_entity_idx;

				++node->count;

				goto_skip:;

				quadtree_reset_flags();
			}
			while(node_info != node_infos);

			assert_neq(in_nodes, 0);
			entity->in_nodes_minus_one = in_nodes - 1;

			++reinsertion;
		}

		alloc_free(reinsertions, qt->reinsertions_size);
		qt->reinsertions = NULL;
		qt->reinsertions_used = 0;
		qt->reinsertions_size = 0;
	}


	{
		quadtree_removal_t* removals = qt->removals;
		quadtree_removal_t* removal = removals;
		quadtree_removal_t* removal_end = removal + qt->removals_used;

		while(removal != removal_end)
		{
			node_info = node_infos;

			*(node_info++) =
			(quadtree_node_info_t)
			{
				.node_idx = 0,
				.extent = qt->half_extent
			};

			uint32_t entity_idx = removal->entity_idx;
			quadtree_entity_t* entity = entities + entity_idx;
			rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);

			do
			{
				quadtree_node_info_t info = *(--node_info);
				quadtree_node_t* node = nodes + info.node_idx;

				if(node->type != QUADTREE_NODE_TYPE_LEAF)
				{
					quadtree_descend(entity_extent);
					continue;
				}

				uint32_t prev_node_entity_idx = 0;
				uint32_t node_entity_idx = node->head;

				while(node_entity_idx)
				{
					if(node_entities.entities[node_entity_idx].index == entity_idx)
					{
						if(prev_node_entity_idx)
						{
							node_entities.next[prev_node_entity_idx] = node_entities.next[node_entity_idx];
							if(!node_entities.next[prev_node_entity_idx])
							{
								node_entities.entities[prev_node_entity_idx].is_last = true;
							}
						}
						else
						{
							node->head = node_entities.next[node_entity_idx];
						}

						--node->count;

						node_entities.next[node_entity_idx] = free_node_entity;
						free_node_entity = node_entity_idx;

						break;
					}

					prev_node_entity_idx = node_entity_idx;
					node_entity_idx = node_entities.next[node_entity_idx];
				}
			}
			while(node_info != node_infos);

			entity->next = free_entity;
			free_entity = entity_idx;

			++removal;
		}

		alloc_free(removals, qt->removals_size);
		qt->removals = NULL;
		qt->removals_used = 0;
		qt->removals_size = 0;
	}


	{
		quadtree_insertion_t* insertions = qt->insertions;
		quadtree_insertion_t* insertion = insertions;
		quadtree_insertion_t* insertion_end = insertion + qt->insertions_used;

		while(insertion != insertion_end)
		{
			quadtree_entity_data* data = &insertion->data;

			uint32_t entity_idx;
			quadtree_entity_t* entity;

			if(free_entity)
			{
				entity_idx = free_entity;
				entity = entities + entity_idx;
				free_entity = entity->next;
			}
			else
			{
				if(entities_used >= entities_size)
				{
					uint32_t new_size = (entities_used << 1) | 1;

					entities = alloc_remalloc(entities, entities_size, new_size);
					assert_not_null(entities);

					entities_size = new_size;
				}

				entity_idx = entities_used++;
				entity = entities + entity_idx;
			}

			entity->data = *data;
			entity->query_tick = qt->query_tick;
			entity->update_tick = qt->update_tick;
			entity->reinsertion_tick = qt->update_tick;

			rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);
			uint32_t in_nodes = 0;

			node_info = node_infos;

			*(node_info++) =
			(quadtree_node_info_t)
			{
				.node_idx = 0,
				.extent = qt->half_extent
			};

			do
			{
				quadtree_node_info_t info = *(--node_info);
				quadtree_node_t* node = nodes + info.node_idx;

				if(node->type != QUADTREE_NODE_TYPE_LEAF)
				{
					quadtree_descend(entity_extent);
					continue;
				}

				rect_extent_t node_extent = half_to_rect_extent(info.extent);
				uint32_t node_entity_idx;

				++in_nodes;

				if(free_node_entity)
				{
					node_entity_idx = free_node_entity;
					free_node_entity = node_entities.next[node_entity_idx];
				}
				else
				{
					if(node_entities_used >= node_entities_size)
					{
						uint32_t new_size = (node_entities_used << 1) | 1;

						node_entities.next = alloc_remalloc(node_entities.next, node_entities_size, new_size);
						assert_not_null(node_entities.next);

						node_entities.entities = alloc_remalloc(node_entities.entities, node_entities_size, new_size);
						assert_not_null(node_entities.entities);

						node_entities.flags = alloc_remalloc(node_entities.flags, node_entities_size, new_size);
						assert_not_null(node_entities.flags);

						node_entities_size = new_size;
					}

					node_entity_idx = node_entities_used++;
				}

				node_entities.next[node_entity_idx] = node->head;
				node_entities.entities[node_entity_idx].is_last = !node->head;
				node_entities.entities[node_entity_idx].index = entity_idx;
				node->head = node_entity_idx;

				quadtree_reset_flags();

				++node->count;
			}
			while(node_info != node_infos);

			assert_neq(in_nodes, 0);
			entity->in_nodes_minus_one = in_nodes - 1;

			++insertion;
		}

		alloc_free(insertions, qt->insertions_size);
		qt->insertions = NULL;
		qt->insertions_used = 0;
		qt->insertions_size = 0;
	}


	{
		uint32_t free_node = 0;
		uint32_t nodes_used = qt->nodes_used;
		uint32_t nodes_size = qt->nodes_size;

		quadtree_node_t* new_nodes;
		quadtree_node_entities_t new_node_entities;
		quadtree_entity_t* new_entities;

		uint32_t new_nodes_used = 0;
		uint32_t new_nodes_size;

		if(nodes_size >> 2 < nodes_used)
		{
			new_nodes_size = nodes_size;
		}
		else
		{
			new_nodes_size = nodes_size >> 1;
		}

		uint32_t new_node_entities_used = 1;
		uint32_t new_node_entities_size;

		if(node_entities_size >> 2 < node_entities_used)
		{
			new_node_entities_size = node_entities_size;
		}
		else
		{
			new_node_entities_size = node_entities_size >> 1;
		}

		uint32_t new_entities_used = 1;
		uint32_t new_entities_size;

		if(entities_size >> 2 < entities_used)
		{
			new_entities_size = entities_size;
		}
		else
		{
			new_entities_size = entities_size >> 1;
		}

		new_nodes = alloc_malloc(new_nodes, new_nodes_size);
		assert_not_null(new_nodes);

		new_node_entities.next = alloc_malloc(new_node_entities.next, new_node_entities_size);
		assert_not_null(new_node_entities.next);

		new_node_entities.entities = alloc_malloc(new_node_entities.entities, new_node_entities_size);
		assert_not_null(new_node_entities.entities);

		new_node_entities.flags = alloc_malloc(new_node_entities.flags, new_node_entities_size);
		assert_not_null(new_node_entities.flags);

		new_entities = alloc_malloc(new_entities, new_entities_size);
		assert_ptr(new_entities, new_entities_size);

		uint32_t* entity_map = alloc_calloc(entity_map, entities_size);
		assert_ptr(entity_map, entities_size);


		typedef struct quadtree_node_reorder_info
		{
			uint32_t node_idx;
			half_extent_t extent;
			uint32_t parent_node_idx;
			uint32_t head_idx;
			uint32_t depth;
		}
		quadtree_node_reorder_info_t;

		quadtree_node_reorder_info_t node_infos[qt->dfs_length];
		quadtree_node_reorder_info_t* node_info = node_infos;

		*(node_info++) =
		(quadtree_node_reorder_info_t)
		{
			.node_idx = 0,
			.extent = qt->half_extent,
			.parent_node_idx = 0,
			.head_idx = 0,
			.depth = 1
		};

		do
		{
			quadtree_node_reorder_info_t info = *(--node_info);
			quadtree_node_t* node = nodes + info.node_idx;

			uint32_t new_node_idx = new_nodes_used++;
			quadtree_node_t* new_node = new_nodes + new_node_idx;

			new_nodes[info.parent_node_idx].heads[info.head_idx] = new_node_idx;

			if(node->type != QUADTREE_NODE_TYPE_LEAF)
			{
				uint32_t total = 0;
				bool possible = true;

				for(int i = 0; i < 4; ++i)
				{
					uint32_t node_idx = node->heads[i];
					quadtree_node_t* node = nodes + node_idx;

					if(node->type != QUADTREE_NODE_TYPE_LEAF)
					{
						possible = false;
						break;
					}

					total += node->count;
				}

				if(possible && total <= qt->merge_threshold)
				{
					uint32_t heads[4];
					memcpy(heads, node->heads, sizeof(heads));

					quadtree_node_t* children[4];
					for(int i = 0; i < 4; ++i)
					{
						children[i] = nodes + heads[i];
					}

					node->head = 0;
					node->position_flags = 0;
					node->count = 0;
					node->type = QUADTREE_NODE_TYPE_LEAF;

					rect_extent_t node_extent = half_to_rect_extent(info.extent);

					assert_ge(qt->merge_ht_size, qt->merge_threshold);
					memset(qt->merge_ht, 0, sizeof(*qt->merge_ht) * qt->merge_ht_size);

					for(int i = 0; i < 4; ++i)
					{
						uint32_t child_idx = heads[i];
						quadtree_node_t* child = children[i];

						node->position_flags |= child->position_flags;

						uint32_t node_entity_idx = child->head;
						while(node_entity_idx)
						{
							uint32_t entity_idx = node_entities.entities[node_entity_idx].index;
							quadtree_entity_t* entity = entities + entity_idx;

							uint32_t hash = (entity_idx * 2654435761u) & (qt->merge_ht_size - 1);
							uint32_t* ht_entry = qt->merge_ht + hash;

							uint32_t next_node_entity_idx = node_entities.next[node_entity_idx];

							while(1)
							{
								if(!*ht_entry)
								{
									*ht_entry = entity_idx;

									node_entities.next[node_entity_idx] = node->head;
									node_entities.entities[node_entity_idx].is_last = !node->head;
									node->head = node_entity_idx;

									rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);
									quadtree_reset_flags();

									++node->count;

									break;
								}

								if(*ht_entry == entity_idx)
								{
									node_entities.next[node_entity_idx] = free_node_entity;
									free_node_entity = node_entity_idx;

									--entity->in_nodes_minus_one;
									if(entity_map[entity_idx])
									{
										new_entities[entity_map[entity_idx]].in_nodes_minus_one = entity->in_nodes_minus_one;
									}

									break;
								}

								hash = (hash + 1) & (qt->merge_ht_size - 1);
								ht_entry = qt->merge_ht + hash;
							}

							node_entity_idx = next_node_entity_idx;
						}

						child->next = free_node;
						free_node = child_idx;
					}

					qt->normalization |= QUADTREE_NOT_NORMALIZED_SOFT;
				}
			}
			else if(
				node->count >= qt->split_threshold &&
				info.extent.w >= qt->min_size &&
				info.extent.h >= qt->min_size &&
				info.depth < qt->max_depth
				)
			{
				uint32_t child_idxs[4];
				for(int i = 0; i < 4; ++i)
				{
					uint32_t child_idx;

					if(free_node)
					{
						child_idx = free_node;
						free_node = nodes[child_idx].next;
					}
					else
					{
						if(nodes_used >= nodes_size)
						{
							uint32_t new_size = (nodes_used << 1) | 1;

							nodes = alloc_remalloc(nodes, nodes_size, new_size);
							assert_not_null(nodes);

							nodes_size = new_size;

							node = nodes + info.node_idx;


							if(new_size > new_nodes_size)
							{
								new_nodes = alloc_remalloc(new_nodes, new_nodes_size, new_size);
								assert_not_null(new_nodes);

								new_nodes_size = new_size;

								new_node = new_nodes + new_node_idx;
							}
						}

						child_idx = nodes_used++;
					}

					child_idxs[i] = child_idx;
				}

				quadtree_node_t* children[4];
				uint32_t head = node->head;
				uint32_t position_flags = node->position_flags;

				for(int i = 0; i < 4; ++i)
				{
					uint32_t child_idx = child_idxs[i];
					quadtree_node_t* child = nodes + child_idx;
					children[i] = child;

					node->heads[i] = child_idx;

					child->head = 0;
					child->count = 0;
					child->type = QUADTREE_NODE_TYPE_LEAF;

					static const uint32_t position_flags_mask[4] =
					{
						0b0011,
						0b1001,
						0b0110,
						0b1100
					};

					child->position_flags = position_flags & position_flags_mask[i];
				}

				uint32_t node_entity_idx = head;
				while(node_entity_idx)
				{
					uint32_t entity_idx = node_entities.entities[node_entity_idx].index;
					quadtree_entity_t* entity = entities + entity_idx;

					rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);

					uint32_t target_node_idxs[4];
					uint32_t* current_target_node_idx = target_node_idxs;

					if(entity_extent.min_x <= info.extent.x)
					{
						if(entity_extent.min_y <= info.extent.y)
						{
							*(current_target_node_idx++) = 0;
						}
						if(entity_extent.max_y >= info.extent.y)
						{
							*(current_target_node_idx++) = 1;
						}
					}
					if(entity_extent.max_x >= info.extent.x)
					{
						if(entity_extent.min_y <= info.extent.y)
						{
							*(current_target_node_idx++) = 2;
						}
						if(entity_extent.max_y >= info.extent.y)
						{
							*(current_target_node_idx++) = 3;
						}
					}

					entity->in_nodes_minus_one += current_target_node_idx - target_node_idxs - 1;
					if(entity_map[entity_idx])
					{
						new_entities[entity_map[entity_idx]].in_nodes_minus_one = entity->in_nodes_minus_one;
					}

					for(uint32_t* target_node_idx = target_node_idxs; target_node_idx != current_target_node_idx; ++target_node_idx)
					{
						quadtree_node_t* target_node = children[*target_node_idx];

						uint32_t new_node_entity_idx;

						if(free_node_entity)
						{
							new_node_entity_idx = free_node_entity;
							free_node_entity = node_entities.next[new_node_entity_idx];
						}
						else
						{
							if(node_entities_used >= node_entities_size)
							{
								uint32_t new_size = (node_entities_used << 1) | 1;

								node_entities.next = alloc_remalloc(node_entities.next, node_entities_size, new_size);
								assert_not_null(node_entities.next);

								node_entities.entities = alloc_remalloc(node_entities.entities, node_entities_size, new_size);
								assert_not_null(node_entities.entities);

								node_entities.flags = alloc_remalloc(node_entities.flags, node_entities_size, new_size);
								assert_not_null(node_entities.flags);

								node_entities_size = new_size;

								if(new_size > new_node_entities_size)
								{
									new_node_entities.next = alloc_remalloc(new_node_entities.next, new_node_entities_size, new_size);
									assert_not_null(new_node_entities.next);

									new_node_entities.entities = alloc_remalloc(new_node_entities.entities, new_node_entities_size, new_size);
									assert_not_null(new_node_entities.entities);

									new_node_entities.flags = alloc_remalloc(new_node_entities.flags, new_node_entities_size, new_size);
									assert_not_null(new_node_entities.flags);

									new_node_entities_size = new_size;
								}
							}

							new_node_entity_idx = node_entities_used++;
						}

						node_entities.next[new_node_entity_idx] = target_node->head;
						node_entities.entities[new_node_entity_idx].is_last = !target_node->head;
						node_entities.entities[new_node_entity_idx].index = entity_idx;
						node_entities.flags[new_node_entity_idx] = node_entities.flags[node_entity_idx];
						target_node->head = new_node_entity_idx;

						++target_node->count;
					}

					uint32_t next_node_entity_idx = node_entities.next[node_entity_idx];

					node_entities.next[node_entity_idx] = free_node_entity;
					free_node_entity = node_entity_idx;

					node_entity_idx = next_node_entity_idx;
				}
			}

			if(node->type != QUADTREE_NODE_TYPE_LEAF)
			{
				float half_w = info.extent.w * 0.5f;
				float half_h = info.extent.h * 0.5f;
				uint32_t next_depth = info.depth + 1;

				*(node_info++) =
				(quadtree_node_reorder_info_t)
				{
					.node_idx = node->heads[0],
					.extent =
					(half_extent_t)
					{
						.x = info.extent.x - half_w,
						.y = info.extent.y - half_h,
						.w = half_w,
						.h = half_h
					},
					.parent_node_idx = new_node_idx,
					.head_idx = 0,
					.depth = next_depth
				};

				*(node_info++) =
				(quadtree_node_reorder_info_t)
				{
					.node_idx = node->heads[1],
					.extent =
					(half_extent_t)
					{
						.x = info.extent.x - half_w,
						.y = info.extent.y + half_h,
						.w = half_w,
						.h = half_h
					},
					.parent_node_idx = new_node_idx,
					.head_idx = 1,
					.depth = next_depth
				};

				*(node_info++) =
				(quadtree_node_reorder_info_t)
				{
					.node_idx = node->heads[2],
					.extent =
					(half_extent_t)
					{
						.x = info.extent.x + half_w,
						.y = info.extent.y - half_h,
						.w = half_w,
						.h = half_h
					},
					.parent_node_idx = new_node_idx,
					.head_idx = 2,
					.depth = next_depth
				};

				*(node_info++) =
				(quadtree_node_reorder_info_t)
				{
					.node_idx = node->heads[3],
					.extent =
					(half_extent_t)
					{
						.x = info.extent.x + half_w,
						.y = info.extent.y + half_h,
						.w = half_w,
						.h = half_h
					},
					.parent_node_idx = new_node_idx,
					.head_idx = 3,
					.depth = next_depth
				};
			}
			else
			{
				new_node->position_flags = node->position_flags;
				new_node->type = QUADTREE_NODE_TYPE_LEAF;

				if(!node->head)
				{
					new_node->head = 0;
					new_node->count = 0;

					continue;
				}

				uint32_t node_entity_idx = node->head;

				new_node->head = new_node_entities_used;
				new_node->count = node->count;

				while(1)
				{
					uint32_t entity_idx = node_entities.entities[node_entity_idx].index;
					if(!entity_map[entity_idx])
					{
						uint32_t new_entity_idx = new_entities_used++;
						entity_map[entity_idx] = new_entity_idx;
						new_entities[new_entity_idx] = entities[entity_idx];
					}

					uint32_t new_entity_idx = entity_map[entity_idx];
					new_node_entities.entities[new_node_entities_used].index = new_entity_idx;
					new_node_entities.flags[new_node_entities_used] = node_entities.flags[node_entity_idx];

					if(node_entities.next[node_entity_idx])
					{
						node_entity_idx = node_entities.next[node_entity_idx];
						new_node_entities.entities[new_node_entities_used].is_last = false;
						new_node_entities.next[new_node_entities_used] = new_node_entities_used + 1;
						++new_node_entities_used;
					}
					else
					{
						new_node_entities.entities[new_node_entities_used].is_last = true;
						new_node_entities.next[new_node_entities_used] = 0;
						++new_node_entities_used;
						break;
					}
				}
			}
		}
		while(node_info != node_infos);

		alloc_free(nodes, nodes_size);
		qt->nodes = new_nodes;
		qt->nodes_used = new_nodes_used;
		qt->nodes_size = new_nodes_size;

		alloc_free(node_entities.next, node_entities_size);
		alloc_free(node_entities.entities, node_entities_size);
		alloc_free(node_entities.flags, node_entities_size);
		qt->node_entities = new_node_entities;
		qt->node_entities_used = new_node_entities_used;
		qt->node_entities_size = new_node_entities_size;

		alloc_free(entities, entities_size);
		qt->entities = new_entities;
		qt->entities_used = new_entities_used;
		qt->entities_size = new_entities_size;

		alloc_free(entity_map, entities_size);
	}
}


static void
quadtree_normalize_hard(
	quadtree_t* qt
	)
{
	assert_not_null(qt);

	if(qt->normalization <= QUADTREE_NOT_NORMALIZED_SOFT)
	{
		return;
	}

	quadtree_normalize(qt);
}


void
quadtree_update(
	quadtree_t* qt,
	quadtree_update_fn_t update_fn,
	void* user_data
	)
{
	assert_not_null(qt);
	assert_not_null(update_fn);

	quadtree_normalize_hard(qt);

	qt->update_tick ^= 1;
	uint32_t update_tick = qt->update_tick;

	quadtree_node_t* nodes = qt->nodes;
	uint32_t* node_entities_next = qt->node_entities.next;
	quadtree_node_entity_t* node_entities = qt->node_entities.entities;
	uint8_t* node_entities_flags = qt->node_entities.flags;
	uint8_t* node_entities_flags_copy = node_entities_flags;
	quadtree_entity_t* entities = qt->entities;
	quadtree_reinsertion_t* reinsertions = qt->reinsertions;
	quadtree_node_removal_t* node_removals = qt->node_removals;

	uint32_t reinsertions_used = qt->reinsertions_used;
	uint32_t reinsertions_size = qt->reinsertions_size;

	uint32_t node_removals_used = qt->node_removals_used;
	uint32_t node_removals_size = qt->node_removals_size;

	quadtree_node_info_t node_infos[qt->dfs_length];
	quadtree_node_info_t* node_info = node_infos;

	*(node_info++) =
	(quadtree_node_info_t)
	{
		.node_idx = 0,
		.extent = qt->half_extent
	};

	do
	{
		quadtree_node_info_t info = *(--node_info);
		quadtree_node_t* node = nodes + info.node_idx;

		if(node->type != QUADTREE_NODE_TYPE_LEAF)
		{
			quadtree_descend_all();
			continue;
		}

		if(!node->head)
		{
			continue;
		}

		rect_extent_t node_extent = half_to_rect_extent(info.extent);

		do
		{
			++node_entities_next;
			++node_entities;
			++node_entities_flags;

			uint32_t entity_idx = node_entities->index;
			quadtree_entity_t* entity = entities + entity_idx;

			if(entity->update_tick != update_tick)
			{
				entity->update_tick = update_tick;
				entity->reinsertion_tick = update_tick ^ 1;

				quadtree_entity_info_t entity_info =
				{
					.idx = entity_idx,
					.data = &entity->data
				};
				entity->status = update_fn(qt, entity_info, user_data);
			}

			if(entity->status == QUADTREE_STATUS_NOT_CHANGED)
			{
				continue;
			}

			rect_extent_t extent = quadtree_get_entity_rect_extent(entity);

			bool crossed_new_boundary = false;
			uint8_t flags = *node_entities_flags;

			if(extent.max_y >= node_extent.max_y && !(node->position_flags & 0b1000))
			{
				if(!(flags & 0b1000))
				{
					flags |= 0b1000;
					crossed_new_boundary = true;
				}
			}
			else if(flags & 0b1000)
			{
				flags &= ~0b1000;
			}

			if(extent.max_x >= node_extent.max_x && !(node->position_flags & 0b0100))
			{
				if(!(flags & 0b0100))
				{
					flags |= 0b0100;
					crossed_new_boundary = true;
				}
			}
			else if(flags & 0b0100)
			{
				flags &= ~0b0100;
			}

			if(extent.min_y <= node_extent.min_y && !(node->position_flags & 0b0010))
			{
				if(!(flags & 0b0010))
				{
					flags |= 0b0010;
					crossed_new_boundary = true;
				}
			}
			else if(flags & 0b0010)
			{
				flags &= ~0b0010;
			}

			if(extent.min_x <= node_extent.min_x && !(node->position_flags & 0b0001))
			{
				if(!(flags & 0b0001))
				{
					flags |= 0b0001;
					crossed_new_boundary = true;
				}
			}
			else if(flags & 0b0001)
			{
				flags &= ~0b0001;
			}

			*node_entities_flags = flags;

			if(crossed_new_boundary && entity->reinsertion_tick != update_tick)
			{
				entity->reinsertion_tick = update_tick;

				uint32_t reinsertion_idx;
				quadtree_reinsertion_t* reinsertion;

				if(reinsertions_used >= reinsertions_size)
				{
					uint32_t new_size = (reinsertions_used << 1) | 1;

					reinsertions = alloc_remalloc(reinsertions, reinsertions_size, new_size);
					assert_not_null(reinsertions);

					reinsertions_size = new_size;
				}

				reinsertion_idx = reinsertions_used++;
				reinsertion = reinsertions + reinsertion_idx;

				reinsertion->entity_idx = entity_idx;

				qt->normalization |= QUADTREE_NOT_NORMALIZED_HARD;
			}

			if(
				(extent.max_x < node_extent.min_x && !(node->position_flags & 0b0001)) ||
				(extent.max_y < node_extent.min_y && !(node->position_flags & 0b0010)) ||
				(node_extent.max_x < extent.min_x && !(node->position_flags & 0b0100)) ||
				(node_extent.max_y < extent.min_y && !(node->position_flags & 0b1000))
				)
			{
				uint32_t node_removal_idx;
				quadtree_node_removal_t* node_removal;

				if(node_removals_used >= node_removals_size)
				{
					uint32_t new_size = (node_removals_used << 1) | 1;

					node_removals = alloc_remalloc(node_removals, node_removals_size, new_size);
					assert_not_null(node_removals);

					node_removals_size = new_size;
				}

				node_removal_idx = node_removals_used++;
				node_removal = node_removals + node_removal_idx;

				uint32_t idx = node_entities_flags - node_entities_flags_copy;

				node_removal->node_idx = info.node_idx;
				node_removal->node_entity_idx = idx;
				node_removal->entity_idx = entity_idx;

				qt->normalization |= QUADTREE_NOT_NORMALIZED_HARD;
			}
		}
		while(*node_entities_next);
	}
	while(node_info != node_infos);

	qt->reinsertions = reinsertions;
	qt->reinsertions_used = reinsertions_used;
	qt->reinsertions_size = reinsertions_size;

	qt->node_removals = node_removals;
	qt->node_removals_used = node_removals_used;
	qt->node_removals_size = node_removals_size;
}


void
quadtree_query_rect(
	quadtree_t* qt,
	rect_extent_t extent,
	quadtree_query_fn_t query_fn,
	void* user_data
	)
{
	assert_not_null(qt);
	assert_not_null(query_fn);

	quadtree_normalize_hard(qt);

	++qt->query_tick;
	uint32_t query_tick = qt->query_tick;

	quadtree_node_t* nodes = qt->nodes;
	quadtree_node_entity_t* node_entities = qt->node_entities.entities;
	quadtree_entity_t* entities = qt->entities;

	quadtree_node_info_t node_infos[qt->dfs_length];
	quadtree_node_info_t* node_info = node_infos;

	*(node_info++) =
	(quadtree_node_info_t)
	{
		.node_idx = 0,
		.extent = qt->half_extent
	};

	do
	{
		quadtree_node_info_t info = *(--node_info);
		quadtree_node_t* node = nodes + info.node_idx;

		if(node->type != QUADTREE_NODE_TYPE_LEAF)
		{
			quadtree_descend(extent);
			continue;
		}

		uint32_t idx = node->head;
		if(!idx)
		{
			continue;
		}

		quadtree_node_entity_t* node_entity = node_entities + idx;

		while(1)
		{
			uint32_t entity_idx = node_entity->index;
			quadtree_entity_t* entity = entities + entity_idx;

			if(entity->query_tick != query_tick)
			{
				entity->query_tick = query_tick;

				if(rect_extent_intersects(quadtree_get_entity_rect_extent(entity), extent))
				{
					quadtree_entity_info_t entity_info =
					{
						.idx = entity_idx,
						.data = &entity->data
					};
					query_fn(qt, entity_info, user_data);
				}
			}

			if(node_entity->is_last)
			{
				break;
			}
			++node_entity;
		}
	}
	while(node_info != node_infos);
}


private float
quadtree_point_to_extent_distance_sq(
	float x,
	float y,
	rect_extent_t extent
	)
{
	float dx = MACRO_MAX(MACRO_MAX(extent.min_x - x, 0.0f), x - extent.max_x);
	float dy = MACRO_MAX(MACRO_MAX(extent.min_y - y, 0.0f), y - extent.max_y);
	return dx * dx + dy * dy;
}


void
quadtree_query_circle(
	quadtree_t* qt,
	float x,
	float y,
	float radius,
	quadtree_query_fn_t query_fn,
	void* user_data
	)
{
	assert_not_null(qt);
	assert_not_null(query_fn);

	quadtree_normalize_hard(qt);

	++qt->query_tick;
	uint32_t query_tick = qt->query_tick;

	float radius_sq = radius * radius;

	rect_extent_t search_extent =
	{
		.min_x = x - radius,
		.min_y = y - radius,
		.max_x = x + radius,
		.max_y = y + radius
	};

	quadtree_node_t* nodes = qt->nodes;
	quadtree_node_entity_t* node_entities = qt->node_entities.entities;
	quadtree_entity_t* entities = qt->entities;

	quadtree_node_info_t node_infos[qt->dfs_length];
	quadtree_node_info_t* node_info = node_infos;

	*(node_info++) =
	(quadtree_node_info_t)
	{
		.node_idx = 0,
		.extent = qt->half_extent
	};

	do
	{
		quadtree_node_info_t info = *(--node_info);
		quadtree_node_t* node = nodes + info.node_idx;

		rect_extent_t node_extent = half_to_rect_extent(info.extent);

		if(quadtree_point_to_extent_distance_sq(x, y, node_extent) > radius_sq)
		{
			continue;
		}

		if(node->type != QUADTREE_NODE_TYPE_LEAF)
		{
			quadtree_descend(search_extent);
			continue;
		}

		uint32_t idx = node->head;
		if(!idx)
		{
			continue;
		}

		quadtree_node_entity_t* node_entity = node_entities + idx;

		while(1)
		{
			uint32_t entity_idx = node_entity->index;
			quadtree_entity_t* entity = entities + entity_idx;

			if(entity->query_tick != query_tick)
			{
				entity->query_tick = query_tick;

				rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);

				float edx = MACRO_MAX(MACRO_MAX(entity_extent.min_x - x, 0.0f), x - entity_extent.max_x);
				float edy = MACRO_MAX(MACRO_MAX(entity_extent.min_y - y, 0.0f), y - entity_extent.max_y);

				if(edx * edx + edy * edy <= radius_sq)
				{
					quadtree_entity_info_t entity_info =
					{
						.idx = entity_idx,
						.data = &entity->data
					};
					query_fn(qt, entity_info, user_data);
				}
			}

			if(node_entity->is_last)
			{
				break;
			}
			++node_entity;
		}
	}
	while(node_info != node_infos);
}


void
quadtree_query_nodes_rect(
	quadtree_t* qt,
	rect_extent_t extent,
	quadtree_node_query_fn_t node_query_fn,
	void* user_data
	)
{
	assert_not_null(qt);
	assert_not_null(node_query_fn);

	quadtree_normalize_hard(qt);

	quadtree_node_t* nodes = qt->nodes;

	quadtree_node_info_t node_infos[qt->dfs_length];
	quadtree_node_info_t* node_info = node_infos;

	*(node_info++) =
	(quadtree_node_info_t)
	{
		.node_idx = 0,
		.extent = qt->half_extent
	};

	do
	{
		quadtree_node_info_t info = *(--node_info);
		quadtree_node_t* node = nodes + info.node_idx;

		if(node->type != QUADTREE_NODE_TYPE_LEAF)
		{
			quadtree_descend(extent);
			continue;
		}

		node_query_fn(qt, &info, user_data);
	}
	while(node_info != node_infos);
}


void
quadtree_query_nodes_circle(
	quadtree_t* qt,
	float x,
	float y,
	float radius,
	quadtree_node_query_fn_t node_query_fn,
	void* user_data
	)
{
	assert_not_null(qt);
	assert_not_null(node_query_fn);

	quadtree_normalize_hard(qt);

	float radius_sq = radius * radius;

	rect_extent_t search_extent =
	{
		.min_x = x - radius,
		.min_y = y - radius,
		.max_x = x + radius,
		.max_y = y + radius
	};

	quadtree_node_t* nodes = qt->nodes;

	quadtree_node_info_t node_infos[qt->dfs_length];
	quadtree_node_info_t* node_info = node_infos;

	*(node_info++) =
	(quadtree_node_info_t)
	{
		.node_idx = 0,
		.extent = qt->half_extent
	};

	do
	{
		quadtree_node_info_t info = *(--node_info);
		quadtree_node_t* node = nodes + info.node_idx;

		rect_extent_t node_extent = half_to_rect_extent(info.extent);

		if(quadtree_point_to_extent_distance_sq(x, y, node_extent) > radius_sq)
		{
			continue;
		}

		if(node->type != QUADTREE_NODE_TYPE_LEAF)
		{
			quadtree_descend(search_extent);
			continue;
		}

		node_query_fn(qt, &info, user_data);
	}
	while(node_info != node_infos);
}


void
quadtree_collide(
	quadtree_t* qt,
	quadtree_collide_fn_t collide_fn,
	void* user_data
	)
{
	assert_not_null(qt);
	assert_not_null(collide_fn);

	quadtree_normalize_hard(qt);

	if(qt->entities_used <= 1)
	{
		return;
	}

#if QUADTREE_DEDUPE_COLLISIONS == 1
	uint32_t ht_size = qt->ht_entries_used * 2;
	uint32_t* ht = alloc_calloc(ht, ht_size);
	assert_not_null(ht);

	quadtree_ht_entry_t* ht_entries = qt->ht_entries;

	uint32_t ht_entries_used = 1;
	uint32_t ht_entries_size = qt->ht_entries_size;
#endif

	quadtree_node_entity_t* node_entities = qt->node_entities.entities;
	quadtree_entity_t* entities = qt->entities;

	quadtree_node_entity_t* node_entity = node_entities;
	quadtree_node_entity_t* node_entities_end = node_entities + qt->node_entities_used - 1;

	do
	{
		++node_entity;
		if(node_entity->is_last)
		{
			continue;
		}

		uint32_t entity_idx = node_entity->index;
		quadtree_entity_t* entity = entities + entity_idx;
		rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);
		quadtree_entity_info_t entity_info =
		{
			.idx = entity_idx,
			.data = &entity->data
		};

		quadtree_node_entity_t* other_node_entity = node_entity;

		do
		{
			++other_node_entity;

			uint32_t other_entity_idx = other_node_entity->index;
			quadtree_entity_t* other_entity = entities + other_entity_idx;

			if(!rect_extent_intersects(
				entity_extent,
				quadtree_get_entity_rect_extent(other_entity)
				))
			{
				continue;
			}

#if QUADTREE_DEDUPE_COLLISIONS == 1
			if(entity->in_nodes_minus_one || other_entity->in_nodes_minus_one)
			{
				uint32_t index_a = entity_idx;
				uint32_t index_b = other_entity_idx;

				if(index_a > index_b)
				{
					uint32_t temp = index_a;
					index_a = index_b;
					index_b = temp;
				}

				uint32_t hash = index_a * 48611 + index_b * 50261;
				hash %= ht_size;

				uint32_t index = ht[hash];
				quadtree_ht_entry_t* entry;

				while(index)
				{
					entry = ht_entries + index;

					if(entry->idx[0] == index_a && entry->idx[1] == index_b)
					{
						goto goto_skip;
					}

					index = entry->next;
				}

				if(ht_entries_used >= ht_entries_size)
				{
					uint32_t new_size = (ht_entries_used << 1) | 1;

					ht_entries = alloc_remalloc(ht_entries, ht_entries_size, new_size);
					assert_not_null(ht_entries);

					ht_entries_size = new_size;
				}

				uint32_t entry_idx = ht_entries_used++;
				entry = ht_entries + entry_idx;

				entry->idx[0] = index_a;
				entry->idx[1] = index_b;
				entry->next = ht[hash];
				ht[hash] = entry_idx;
			}
#endif

			quadtree_entity_info_t other_entity_info =
			{
				.idx = other_entity_idx,
				.data = &other_entity->data
			};
			collide_fn(qt, entity_info, other_entity_info, user_data);

			goto_skip:;
		}
		while(!other_node_entity->is_last);
	}
	while(node_entity != node_entities_end);

#if QUADTREE_DEDUPE_COLLISIONS == 1
	if(ht_entries_used * 4 <= ht_entries_size)
	{
		uint32_t new_size = ht_entries_size >> 1;

		ht_entries = alloc_remalloc(ht_entries, ht_entries_size, new_size);
		assert_not_null(ht_entries);

		ht_entries_size = new_size;
	}

	qt->ht_entries = ht_entries;
	qt->ht_entries_used = ht_entries_used;
	qt->ht_entries_size = ht_entries_size;

	alloc_free(ht, ht_size);
#endif
}


uint32_t
quadtree_depth(
	quadtree_t* qt
	)
{
	assert_not_null(qt);

	quadtree_normalize_hard(qt);

	quadtree_node_t* nodes = qt->nodes;

	uint32_t max_depth = 0;

	typedef struct quadtree_node_depth_info
	{
		uint32_t node_idx;
		uint32_t depth;
	}
	quadtree_node_depth_info;

#undef quadtree_fill_node
#define quadtree_fill_node(_node_idx, _extent, _depth)	\
(quadtree_node_depth_info)								\
{														\
	.node_idx = _node_idx,								\
	.depth = _depth										\
}

	quadtree_node_depth_info node_infos[qt->dfs_length];
	quadtree_node_depth_info* node_info = node_infos;

	*(node_info++) =
	(quadtree_node_depth_info)
	{
		.node_idx = 0,
		.depth = 1
	};

	do
	{
		quadtree_node_depth_info info = *(--node_info);
		quadtree_node_t* node = nodes + info.node_idx;

		if(node->type != QUADTREE_NODE_TYPE_LEAF)
		{
			quadtree_descend_extentless(info.depth + 1);
			continue;
		}

		if(info.depth > max_depth)
		{
			max_depth = info.depth;
		}
	}
	while(node_info != node_infos);

#undef quadtree_fill_node
#define quadtree_fill_node(...)			\
quadtree_fill_node_default(__VA_ARGS__)

	return max_depth;
}


float
quadtree_nearest_rect(
	quadtree_t* qt,
	float x,
	float y,
	rect_extent_t extent,
	quadtree_query_fn_t query_fn,
	void* user_data
	)
{
	assert_not_null(qt);

	quadtree_normalize_hard(qt);

	float max_dist_sq = INFINITY;

	rect_extent_t traversal_extent = extent;

	quadtree_node_t* nodes = qt->nodes;
	quadtree_node_entity_t* node_entities = qt->node_entities.entities;
	quadtree_entity_t* entities = qt->entities;

	quadtree_node_info_t node_infos[qt->dfs_length];
	quadtree_node_info_t* node_info = node_infos;

	*(node_info++) =
	(quadtree_node_info_t)
	{
		.node_idx = 0,
		.extent = qt->half_extent
	};

	do
	{
		quadtree_node_info_t info = *(--node_info);
		quadtree_node_t* node = nodes + info.node_idx;

		rect_extent_t node_extent = half_to_rect_extent(info.extent);

		if(!rect_extent_intersects(node_extent, traversal_extent))
		{
			continue;
		}

		if(node->type != QUADTREE_NODE_TYPE_LEAF)
		{
			float distance = quadtree_point_to_extent_distance_sq(x, y, node_extent);
			if(distance < max_dist_sq)
			{
				quadtree_descend(traversal_extent);
			}
			continue;
		}

		float distance = quadtree_point_to_extent_distance_sq(x, y, node_extent);
		if(distance >= max_dist_sq)
		{
			continue;
		}

		uint32_t idx = node->head;
		if(!idx)
		{
			continue;
		}

		quadtree_node_entity_t* node_entity = node_entities + idx;

		while(1)
		{
			uint32_t entity_idx = node_entity->index;
			quadtree_entity_t* entity = entities + entity_idx;
			rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);

			if(rect_extent_intersects(entity_extent, extent))
			{
				float distance = quadtree_point_to_extent_distance_sq(x, y, entity_extent);

				if(distance < max_dist_sq)
				{
					max_dist_sq = distance;
					float max_dist = sqrtf(max_dist_sq);

					if(query_fn)
					{
						quadtree_entity_info_t entity_info =
						{
							.idx = entity_idx,
							.data = &entity->data
						};
						query_fn(qt, entity_info, user_data);
					}

					traversal_extent.min_x = MACRO_MAX(extent.min_x, x - max_dist);
					traversal_extent.min_y = MACRO_MAX(extent.min_y, y - max_dist);
					traversal_extent.max_x = MACRO_MIN(extent.max_x, x + max_dist);
					traversal_extent.max_y = MACRO_MIN(extent.max_y, y + max_dist);
				}
			}

			if(node_entity->is_last)
			{
				break;
			}
			++node_entity;
		}
	}
	while(node_info != node_infos);

	return sqrtf(max_dist_sq);
}


float
quadtree_nearest_circle(
	quadtree_t* qt,
	float x,
	float y,
	float max_distance,
	quadtree_query_fn_t query_fn,
	void* user_data
	)
{
	assert_not_null(qt);

	quadtree_normalize_hard(qt);

	if(max_distance < 0.0f)
	{
		max_distance = INFINITY;
	}

	float max_dist_sq = max_distance * max_distance;
	float max_dist = max_distance;

	rect_extent_t search_extent =
	{
		.min_x = x - max_distance,
		.min_y = y - max_distance,
		.max_x = x + max_distance,
		.max_y = y + max_distance
	};

	quadtree_node_t* nodes = qt->nodes;
	quadtree_node_entity_t* node_entities = qt->node_entities.entities;
	quadtree_entity_t* entities = qt->entities;

	quadtree_node_info_t node_infos[qt->dfs_length];
	quadtree_node_info_t* node_info = node_infos;

	*(node_info++) =
	(quadtree_node_info_t)
	{
		.node_idx = 0,
		.extent = qt->half_extent
	};

	do
	{
		quadtree_node_info_t info = *(--node_info);
		quadtree_node_t* node = nodes + info.node_idx;

		rect_extent_t node_extent = half_to_rect_extent(info.extent);

		if(quadtree_point_to_extent_distance_sq(x, y, node_extent) >= max_dist_sq)
		{
			continue;
		}

		if(node->type != QUADTREE_NODE_TYPE_LEAF)
		{
			quadtree_descend(search_extent);
			continue;
		}

		uint32_t idx = node->head;
		if(!idx)
		{
			continue;
		}

		quadtree_node_entity_t* node_entity = node_entities + idx;

		while(1)
		{
			uint32_t entity_idx = node_entity->index;
			quadtree_entity_t* entity = entities + entity_idx;

			rect_extent_t extent = quadtree_get_entity_rect_extent(entity);
			float distance = quadtree_point_to_extent_distance_sq(x, y, extent);

			if(distance < max_dist_sq)
			{
				max_dist_sq = distance;
				max_dist = sqrtf(max_dist_sq);

				search_extent.min_x = x - max_dist;
				search_extent.min_y = y - max_dist;
				search_extent.max_x = x + max_dist;
				search_extent.max_y = y + max_dist;

				if(query_fn)
				{
					quadtree_entity_info_t entity_info =
					{
						.idx = entity_idx,
						.data = &entity->data
					};
					query_fn(qt, entity_info, user_data);
				}
			}

			if(node_entity->is_last)
			{
				break;
			}
			++node_entity;
		}
	}
	while(node_info != node_infos);

	return max_dist;
}


private bool
quadtree_ray_intersects_extent(
	float x,
	float y,
	float inv_dx,
	float inv_dy,
	float max_distance,
	rect_extent_t extent
	)
{
	float t1 = (extent.min_x - x) * inv_dx;
	float t2 = (extent.max_x - x) * inv_dx;

	float t_min = MACRO_MIN(t1, t2);
	float t_max = MACRO_MAX(t1, t2);

	t1 = (extent.min_y - y) * inv_dy;
	t2 = (extent.max_y - y) * inv_dy;

	t_min = MACRO_MAX(t_min, MACRO_MIN(t1, t2));
	t_max = MACRO_MIN(MACRO_MIN(t_max, MACRO_MAX(t1, t2)), max_distance);

	return t_max >= t_min && t_max >= 0.0f;
}


void
quadtree_raycast(
	quadtree_t* qt,
	float x,
	float y,
	float dx,
	float dy,
	float max_distance,
	quadtree_query_fn_t query_fn,
	void* user_data
	)
{
	assert_not_null(qt);

	quadtree_normalize_hard(qt);

	++qt->query_tick;
	uint32_t query_tick = qt->query_tick;

	quadtree_node_t* nodes = qt->nodes;
	quadtree_node_entity_t* node_entities = qt->node_entities.entities;
	quadtree_entity_t* entities = qt->entities;

	float inv_dx = 1.0f / dx;
	float inv_dy = 1.0f / dy;

	quadtree_node_info_t node_infos[qt->dfs_length];
	quadtree_node_info_t* node_info = node_infos;

	*(node_info++) =
	(quadtree_node_info_t)
	{
		.node_idx = 0,
		.extent = qt->half_extent
	};

	do
	{
		quadtree_node_info_t info = *(--node_info);
		quadtree_node_t* node = nodes + info.node_idx;

		if(node->type != QUADTREE_NODE_TYPE_LEAF)
		{
			float half_w = info.extent.w * 0.5f;
			float half_h = info.extent.h * 0.5f;

			rect_extent_t child_extent;
			child_extent.min_x = info.extent.x - info.extent.w;
			child_extent.min_y = info.extent.y - info.extent.h;
			child_extent.max_x = info.extent.x;
			child_extent.max_y = info.extent.y;

			if(quadtree_ray_intersects_extent(x, y, inv_dx, inv_dy, max_distance, child_extent))
			{
				*(node_info++) = quadtree_fill_node(
					node->heads[0],
					((half_extent_t)
					{
						.x = info.extent.x - half_w,
						.y = info.extent.y - half_h,
						.w = half_w,
						.h = half_h
					})
					);
			}

			child_extent.min_y = info.extent.y;
			child_extent.max_y = info.extent.y + info.extent.h;

			if(quadtree_ray_intersects_extent(x, y, inv_dx, inv_dy, max_distance, child_extent))
			{
				*(node_info++) = quadtree_fill_node(
					node->heads[1],
					((half_extent_t)
					{
						.x = info.extent.x - half_w,
						.y = info.extent.y + half_h,
						.w = half_w,
						.h = half_h
					})
					);
			}

			child_extent.min_x = info.extent.x;
			child_extent.max_x = info.extent.x + info.extent.w;
			child_extent.min_y = info.extent.y - info.extent.h;
			child_extent.max_y = info.extent.y;

			if(quadtree_ray_intersects_extent(x, y, inv_dx, inv_dy, max_distance, child_extent))
			{
				*(node_info++) = quadtree_fill_node(
					node->heads[2],
					((half_extent_t)
					{
						.x = info.extent.x + half_w,
						.y = info.extent.y - half_h,
						.w = half_w,
						.h = half_h
					})
					);
			}

			child_extent.min_y = info.extent.y;
			child_extent.max_y = info.extent.y + info.extent.h;

			if(quadtree_ray_intersects_extent(x, y, inv_dx, inv_dy, max_distance, child_extent))
			{
				*(node_info++) = quadtree_fill_node(
					node->heads[3],
					((half_extent_t)
					{
						.x = info.extent.x + half_w,
						.y = info.extent.y + half_h,
						.w = half_w,
						.h = half_h
					})
					);
			}

			continue;
		}

		uint32_t idx = node->head;
		if(!idx)
		{
			continue;
		}

		quadtree_node_entity_t* node_entity = node_entities + idx;

		while(1)
		{
			uint32_t entity_idx = node_entity->index;
			quadtree_entity_t* entity = entities + entity_idx;

			if(entity->query_tick != query_tick)
			{
				entity->query_tick = query_tick;

				rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);
				if(quadtree_ray_intersects_extent(x, y, inv_dx, inv_dy, max_distance, entity_extent))
				{
					if(query_fn)
					{
						quadtree_entity_info_t entity_info =
						{
							.idx = entity_idx,
							.data = &entity->data
						};
						query_fn(qt, entity_info, user_data);
					}
				}
			}

			if(node_entity->is_last)
			{
				break;
			}
			++node_entity;
		}
	}
	while(node_info != node_infos);
}


private void
quadtree_check_count_node(
	quadtree_t* qt,
	const quadtree_node_info_t* info,
	void* user_data
	)
{
	(void) qt;
	(void) info;
	uint32_t* check_count = user_data;

	++(*check_count);
}


private void
quadtree_check_in_nodes_count(
	quadtree_t* qt
	)
{
	quadtree_entity_t* entities = qt->entities;
	uint32_t entities_used = qt->entities_used;
	uint32_t i;

	for(i = 1; i < entities_used; ++i)
	{
		quadtree_entity_t* entity = entities + i;
		rect_extent_t extent = quadtree_get_entity_rect_extent(entity);

		uint32_t check_count = 0;
		quadtree_query_nodes_rect(qt, extent, quadtree_check_count_node, &check_count);

		hard_assert_eq(check_count - 1, entity->in_nodes_minus_one);
	}
}


void
quadtree_check(
	quadtree_t* qt
	)
{
	assert_not_null(qt);

	quadtree_normalize_hard(qt);

	quadtree_check_in_nodes_count(qt);
}


#undef quadtree_reset_flags
#undef quadtree_descend_extentless
#undef quadtree_descend_all
#undef quadtree_descend
#undef quadtree_fill_node
#undef quadtree_fill_node_default
