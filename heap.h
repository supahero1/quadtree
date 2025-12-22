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

#include <stdint.h>


typedef int
(*heap_cmp_fn_t)(
	const void* a,
	const void* b
	);


typedef struct heap
{
	void* arr;
	void* temp;
	uint32_t used;
	uint32_t size;
	uint32_t el_size;
	heap_cmp_fn_t cmp_fn;
}
heap_t;


extern void
heap_init(
	heap_t* heap
	);


extern void
heap_free(
	heap_t* heap
	);


extern void
heap_push(
	heap_t* heap,
	void* el
	);


extern void*
heap_pop(
	heap_t* heap
	);


extern void*
heap_peek(
	heap_t* heap
	);


extern void
heap_replace(
	heap_t* heap,
	void* new_root
	);
