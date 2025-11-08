/*
 *   Copyright 2024-2025 Franciszek Balcerak
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


typedef union pair
{
	struct
	{
		float x;
		float y;
	};

	struct
	{
		float w;
		float h;
	};
}
pair_t;


typedef union ipair
{
	struct
	{
		int x;
		int y;
	};

	struct
	{
		int w;
		int h;
	};
}
ipair_t;


typedef union half_extent
{
	struct
	{
		union
		{
			pair_t pos;

			struct
			{
				float x;
				float y;
			};
		};

		union
		{
			pair_t size;

			struct
			{
				float w;
				float h;
			};
		};
	};

	struct
	{
		float top;
		float left;
		float right;
		float bottom;
	};
}
half_extent_t;


typedef struct rect_extent
{
	union
	{
		pair_t min;

		struct
		{
			float min_x;
			float min_y;
		};
	};

	union
	{
		pair_t max;

		struct
		{
			float max_x;
			float max_y;
		};
	};
}
rect_extent_t;


extern bool
rect_extent_intersects(
	rect_extent_t a,
	rect_extent_t b
	);


extern bool
rect_extent_is_inside(
	rect_extent_t a,
	rect_extent_t b
	);


extern rect_extent_t
half_to_rect_extent(
	half_extent_t extent
	);


extern half_extent_t
rect_to_half_extent(
	rect_extent_t extent
	);
