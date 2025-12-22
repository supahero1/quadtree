/*
 * Copyright 2025 Franciszek Balcerak
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "heap.h"
#include "alloc/include/debug.h"
#include "alloc/include/alloc_ext.h"

#include <string.h>

#define HEAP_CHILDREN 4


void
heap_init(
	heap_t* heap
	)
{
	assert_not_null(heap);
	assert_not_null(heap->cmp_fn);
	assert_gt(heap->el_size, 0);

	heap->arr = NULL;
	heap->used = 0;
	heap->size = 0;

	heap->temp = alloc_malloc(heap->temp, heap->el_size);
	assert_not_null(heap->temp);
}


void
heap_free(
	heap_t* heap
	)
{
	assert_not_null(heap);

	alloc_free(heap->temp, heap->el_size);
	alloc_free(heap->arr, heap->size * heap->el_size);
}


private void
heap_resize(
	heap_t* heap,
	uint32_t count
	)
{
	assert_not_null(heap);

	uint32_t new_used = heap->used + count;

	if(new_used > heap->size || new_used < heap->size / 4)
	{
		uint32_t new_count = new_used > 0 ? new_used << 1 : 4;

		heap->arr = alloc_remalloc(heap->arr, heap->size * heap->el_size, new_count * heap->el_size);
		assert_not_null(heap->arr);

		heap->size = new_count;
	}
}


void
heap_push(
	heap_t* heap,
	void* el
	)
{
	assert_not_null(heap);
	assert_not_null(el);

	heap_resize(heap, 1);

	uint32_t idx = heap->used;
	uint32_t parent_idx;

	memcpy(heap->temp, el, heap->el_size);

	while(idx > 0)
	{
		parent_idx = (idx - 1) / HEAP_CHILDREN;

		if(heap->cmp_fn(heap->arr + parent_idx * heap->el_size, heap->temp) > 0)
		{
			memcpy(heap->arr + idx * heap->el_size, heap->arr + parent_idx * heap->el_size, heap->el_size);
			idx = parent_idx;
		}
		else
		{
			break;
		}
	}

	memcpy(heap->arr + idx * heap->el_size, heap->temp, heap->el_size);
	++heap->used;
}


private void
heap_down_common(
	heap_t* heap,
	void* el
	)
{
	assert_not_null(heap);
	assert_not_null(el);

	uint32_t idx = 0;

	while(1)
	{
		uint32_t child_idx = idx * HEAP_CHILDREN + 1;
		if(child_idx >= heap->used)
		{
			break;
		}

		uint32_t min_child = child_idx;
		uint32_t limit = MACRO_MIN(child_idx + HEAP_CHILDREN, heap->used);

		void* min_child_ptr = heap->arr + min_child * heap->el_size;
		void* current_ptr = min_child_ptr + heap->el_size;

		for(uint32_t k = child_idx + 1; k < limit; ++k)
		{
			if(heap->cmp_fn(min_child_ptr, current_ptr) > 0)
			{
				min_child = k;
				min_child_ptr = current_ptr;
			}
			current_ptr += heap->el_size;
		}

		if(heap->cmp_fn(el, min_child_ptr) > 0)
		{
			memcpy(heap->arr + idx * heap->el_size, min_child_ptr, heap->el_size);
			idx = min_child;
		}
		else
		{
			break;
		}
	}

	memcpy(heap->arr + idx * heap->el_size, el, heap->el_size);
}


void*
heap_pop(
	heap_t* heap
	)
{
	assert_not_null(heap);

	if(heap->used == 0)
	{
		return NULL;
	}

	if(--heap->used == 0)
	{
		return heap->arr;
	}

	memcpy(heap->temp, heap->arr, heap->el_size);

	void* last_el_ptr = heap->arr + heap->used * heap->el_size;
	heap_down_common(heap, last_el_ptr);
	heap_resize(heap, 0);

	return heap->temp;
}


void*
heap_peek(
	heap_t* heap
	)
{
	assert_not_null(heap);

	if(heap->used == 0)
	{
		return NULL;
	}

	return heap->arr;
}


void
heap_replace(
	heap_t* heap,
	void* new_root
	)
{
	assert_not_null(heap);
	assert_not_null(new_root);

	if(heap->used == 0)
	{
		return;
	}

	heap_down_common(heap, new_root);
}
