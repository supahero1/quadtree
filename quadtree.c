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

#include <string.h>


void
quadtree_init(
	quadtree_t* qt
	)
{
	assert_not_null(qt);

	if(!qt->split_threshold)
	{
		qt->split_threshold = 17;
	}

	if(!qt->merge_threshold && !qt->merge_threshold_set)
	{
		qt->merge_threshold = 16;
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
	qt->node_entities = NULL;
	qt->entities = NULL;
#if QUADTREE_DEDUPE_COLLISIONS == 1
	qt->ht_entries = NULL;
#endif
	qt->removals = NULL;
	qt->node_removals = NULL;
	qt->insertions = NULL;
	qt->reinsertions = NULL;
	qt->merge_ht = alloc_malloc(qt->merge_ht, qt->merge_ht_size);

	assert_not_null(qt->nodes);
	assert_ptr(qt->merge_ht, qt->merge_ht_size);

	qt->nodes_used = 1;
	qt->nodes_size = 1;

	qt->node_entities_used = 1;
	qt->node_entities_size = 0;

	qt->entities_used = 1;
	qt->entities_size = 0;

#if QUADTREE_DEDUPE_COLLISIONS == 1
	qt->ht_entries_used = 1;
	qt->ht_entries_size = 0;
#endif

	qt->removals_used = 0;
	qt->removals_size = 0;

	qt->node_removals_used = 0;
	qt->node_removals_size = 0;

	qt->insertions_used = 0;
	qt->insertions_size = 0;

	qt->reinsertions_used = 0;
	qt->reinsertions_size = 0;

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
	alloc_free(qt->node_entities, qt->node_entities_size);
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

	qt->normalized = false;
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

	qt->normalized = false;
}


void
quadtree_normalize(
	quadtree_t* qt
	)
{
	assert_not_null(qt);

	if(qt->normalized)
	{
		return;
	}

	qt->normalized = true;

	quadtree_node_t* nodes = qt->nodes;
	quadtree_node_entity_t* node_entities = qt->node_entities;
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
			quadtree_node_entity_t* node_entity = node_entities + node_entity_idx;

			uint32_t prev_node_entity_idx = node_removal->prev_node_entity_idx;
			quadtree_node_entity_t* prev_node_entity = node_entities + prev_node_entity_idx;

			if(prev_node_entity_idx)
			{
				prev_node_entity->next = node_entity->next;
			}
			else
			{
				node->head = node_entity->next;
			}

			--node->count;

			node_entity->next = free_node_entity;
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

				uint32_t node_entity_idx = node->head;
				quadtree_node_entity_t* node_entity;

				while(node_entity_idx)
				{
					node_entity = node_entities + node_entity_idx;

					if(node_entity->entity == entity_idx)
					{
						goto goto_skip;
					}

					node_entity_idx = node_entity->next;
				}

				if(free_node_entity)
				{
					node_entity_idx = free_node_entity;
					node_entity = node_entities + node_entity_idx;
					free_node_entity = node_entity->next;
				}
				else
				{
					if(node_entities_used >= node_entities_size)
					{
						uint32_t new_size = (node_entities_used << 1) | 1;

						node_entities = alloc_remalloc(node_entities, node_entities_size, new_size);
						assert_not_null(node_entities);

						node_entities_size = new_size;
					}

					node_entity_idx = node_entities_used++;
					node_entity = node_entities + node_entity_idx;
				}

				node_entity->next = node->head;
				node_entity->entity = entity_idx;
				node->head = node_entity_idx;

				++node->count;

				goto_skip:;
			}
			while(node_info != node_infos);

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

				quadtree_node_entity_t* prev_node_entity = NULL;
				uint32_t node_entity_idx = node->head;

				while(node_entity_idx)
				{
					quadtree_node_entity_t* node_entity = node_entities + node_entity_idx;

					if(node_entity->entity == entity_idx)
					{
						if(prev_node_entity)
						{
							prev_node_entity->next = node_entity->next;
						}
						else
						{
							node->head = node_entity->next;
						}

						--node->count;

						node_entity->next = free_node_entity;
						free_node_entity = node_entity_idx;

						break;
					}

					prev_node_entity = node_entity;
					node_entity_idx = node_entity->next;
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
			entity->fully_in_node = false;

			rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);

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

				uint32_t node_entity_idx;
				quadtree_node_entity_t* node_entity;

				if(free_node_entity)
				{
					node_entity_idx = free_node_entity;
					node_entity = node_entities + node_entity_idx;
					free_node_entity = node_entity->next;
				}
				else
				{
					if(node_entities_used >= node_entities_size)
					{
						uint32_t new_size = (node_entities_used << 1) | 1;

						node_entities = alloc_remalloc(node_entities, node_entities_size, new_size);
						assert_not_null(node_entities);

						node_entities_size = new_size;
					}

					node_entity_idx = node_entities_used++;
					node_entity = node_entities + node_entity_idx;
				}

				node_entity->next = node->head;
				node_entity->entity = entity_idx;
				node->head = node_entity_idx;

				++node->count;
			}
			while(node_info != node_infos);

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
		quadtree_node_entity_t* new_node_entities;
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

		new_node_entities = alloc_malloc(new_node_entities, new_node_entities_size);
		assert_ptr(new_node_entities, new_node_entities_size);

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
							quadtree_node_entity_t* node_entity = node_entities + node_entity_idx;

							uint32_t hash = (node_entity->entity * 2654435761u) & (qt->merge_ht_size - 1);
							uint32_t* ht_entry = qt->merge_ht + hash;

							uint32_t next_node_entity_idx = node_entity->next;

							while(1)
							{
								if(!*ht_entry)
								{
									*ht_entry = node_entity->entity;

									node_entity->next = node->head;
									node->head = node_entity_idx;

									++node->count;

									break;
								}

								if(*ht_entry == node_entity->entity)
								{
									node_entity->next = free_node_entity;
									free_node_entity = node_entity_idx;

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

					qt->normalized = false;
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
					quadtree_node_entity_t* node_entity = node_entities + node_entity_idx;

					uint32_t entity_idx = node_entity->entity;
					quadtree_entity_t* entity = entities + entity_idx;
					entity->fully_in_node = false;

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

					for(uint32_t* target_node_idx = target_node_idxs; target_node_idx != current_target_node_idx; ++target_node_idx)
					{
						quadtree_node_t* target_node = children[*target_node_idx];

						uint32_t new_node_entity_idx;
						quadtree_node_entity_t* new_node_entity;

						if(free_node_entity)
						{
							new_node_entity_idx = free_node_entity;
							new_node_entity = node_entities + new_node_entity_idx;
							free_node_entity = new_node_entity->next;
						}
						else
						{
							if(node_entities_used >= node_entities_size)
							{
								uint32_t new_size = (node_entities_used << 1) | 1;

								node_entities = alloc_remalloc(node_entities, node_entities_size, new_size);
								assert_not_null(node_entities);

								node_entities_size = new_size;

								node_entity = node_entities + node_entity_idx;


								if(new_size > new_node_entities_size)
								{
									new_node_entities = alloc_remalloc(new_node_entities, new_node_entities_size, new_size);
									assert_not_null(new_node_entities);

									new_node_entities_size = new_size;
								}
							}

							new_node_entity_idx = node_entities_used++;
							new_node_entity = node_entities + new_node_entity_idx;
						}

						new_node_entity->next = target_node->head;
						new_node_entity->entity = entity_idx;
						target_node->head = new_node_entity_idx;

						++target_node->count;
					}

					uint32_t next_node_entity_idx = node_entity->next;

					node_entity->next = free_node_entity;
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
					quadtree_node_entity_t* node_entity = node_entities + node_entity_idx;
					quadtree_node_entity_t* new_node_entity = new_node_entities + new_node_entities_used;
					++new_node_entities_used;

					uint32_t entity_idx = node_entity->entity;
					if(!entity_map[entity_idx])
					{
						uint32_t new_entity_idx = new_entities_used++;
						entity_map[entity_idx] = new_entity_idx;
						new_entities[new_entity_idx] = entities[entity_idx];
					}

					new_node_entity->entity = entity_map[entity_idx];

					if(node_entity->next)
					{
						node_entity_idx = node_entity->next;
						new_node_entity->next = new_node_entities_used;
					}
					else
					{
						new_node_entity->next = 0;
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

		alloc_free(node_entities, node_entities_size);
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


void
quadtree_update(
	quadtree_t* qt,
	quadtree_update_fn_t update_fn
	)
{
	assert_not_null(qt);
	assert_not_null(update_fn);

	quadtree_normalize(qt);

	qt->update_tick ^= 1;
	uint32_t update_tick = qt->update_tick;

	quadtree_node_t* nodes = qt->nodes;
	quadtree_node_entity_t* node_entities = qt->node_entities;
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

		rect_extent_t node_extent = half_to_rect_extent(info.extent);

		uint32_t prev_idx = 0;
		uint32_t idx = node->head;

		while(idx)
		{
			quadtree_node_entity_t* node_entity = node_entities + idx;

			uint32_t entity_idx = node_entity->entity;
			quadtree_entity_t* entity = entities + entity_idx;

			rect_extent_t extent;

			if(entity->update_tick != update_tick)
			{
				entity->update_tick = update_tick;
				quadtree_status_t status = update_fn(qt, entity_idx, &entity->data);
				extent = quadtree_get_entity_rect_extent(entity);

				if(status == QUADTREE_STATUS_CHANGED)
				{
					entity->fully_in_node = rect_extent_is_inside(extent, node_extent);
					if(!entity->fully_in_node)
					{
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

						qt->normalized = false;
					}
				}
			}
			else
			{
				extent = quadtree_get_entity_rect_extent(entity);
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

				node_removal->node_idx = info.node_idx;
				node_removal->node_entity_idx = idx;
				node_removal->prev_node_entity_idx = prev_idx;

				qt->normalized = false;
			}

			prev_idx = idx;
			idx = node_entity->next;
		}
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
quadtree_query(
	quadtree_t* qt,
	rect_extent_t extent,
	quadtree_query_fn_t query_fn
	)
{
	assert_not_null(qt);
	assert_not_null(query_fn);

	quadtree_normalize(qt);

	++qt->query_tick;
	uint32_t query_tick = qt->query_tick;

	quadtree_node_t* nodes = qt->nodes;
	quadtree_node_entity_t* node_entities = qt->node_entities;
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
		while(idx)
		{
			quadtree_node_entity_t* node_entity = node_entities + idx;
			uint32_t entity_idx = node_entity->entity;
			quadtree_entity_t* entity = entities + entity_idx;

			if(entity->query_tick != query_tick)
			{
				entity->query_tick = query_tick;

				if(rect_extent_intersects(quadtree_get_entity_rect_extent(entity), extent))
				{
					query_fn(qt, entity_idx, &entity->data);
				}
			}

			idx = node_entity->next;
		}
	}
	while(node_info != node_infos);
}


void
quadtree_query_nodes(
	quadtree_t* qt,
	rect_extent_t extent,
	quadtree_node_query_fn_t node_query_fn
	)
{
	assert_not_null(qt);
	assert_not_null(node_query_fn);

	quadtree_normalize(qt);

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

		node_query_fn(qt, &info);
	}
	while(node_info != node_infos);
}


void
quadtree_collide(
	quadtree_t* qt,
	quadtree_collide_fn_t collide_fn
	)
{
	assert_not_null(qt);
	assert_not_null(collide_fn);

	quadtree_normalize(qt);

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

	quadtree_node_entity_t* node_entities = qt->node_entities;
	quadtree_entity_t* entities = qt->entities;

	quadtree_node_entity_t* node_entity = node_entities;
	quadtree_node_entity_t* node_entities_end = node_entities + qt->node_entities_used - 1;

	do
	{
		++node_entity;

		if(!node_entity->next)
		{
			continue;
		}

		uint32_t entity_idx = node_entity->entity;
		quadtree_entity_t* entity = entities + entity_idx;
		rect_extent_t entity_extent = quadtree_get_entity_rect_extent(entity);

		quadtree_node_entity_t* other_node_entity = node_entity;
		while(1)
		{
			++other_node_entity;

			uint32_t other_entity_idx = other_node_entity->entity;
			quadtree_entity_t* other_entity = entities + other_entity_idx;

			if(!rect_extent_intersects(
				entity_extent,
				quadtree_get_entity_rect_extent(other_entity)
				))
			{
				goto goto_skip;
			}

#if QUADTREE_DEDUPE_COLLISIONS == 1
			if(!entity->fully_in_node || !other_entity->fully_in_node)
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

			collide_fn(qt, entity_idx, &entity->data, other_entity_idx, &other_entity->data);

			goto_skip:;

			if(!other_node_entity->next)
			{
				break;
			}
		}
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

	quadtree_normalize(qt);

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


#undef quadtree_descend_extentless
#undef quadtree_descend_all
#undef quadtree_descend
#undef quadtree_fill_node
#undef quadtree_fill_node_default
