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

#include "extent.h"


bool
rect_extent_intersects(
	rect_extent_t a,
	rect_extent_t b
	)
{
	return
		a.max_x >= b.min_x &&
		a.max_y >= b.min_y &&
		b.max_x >= a.min_x &&
		b.max_y >= a.min_y;
}


bool
rect_extent_is_inside(
	rect_extent_t a,
	rect_extent_t b
	)
{
	return
		a.min_x > b.min_x &&
		a.min_y > b.min_y &&
		b.max_x > a.max_x &&
		b.max_y > a.max_y;
}


rect_extent_t
half_to_rect_extent(
	half_extent_t extent
	)
{
	return
	(rect_extent_t)
	{
		.min_x = extent.x - extent.w,
		.min_y = extent.y - extent.h,
		.max_x = extent.x + extent.w,
		.max_y = extent.y + extent.h
	};
}


half_extent_t
rect_to_half_extent(
	rect_extent_t extent
	)
{
	float half_w = (extent.max_x - extent.min_x) * 0.5f;
	float half_h = (extent.max_y - extent.min_y) * 0.5f;

	return
	(half_extent_t)
	{
		.x = extent.min_x + half_w,
		.y = extent.min_y + half_h,
		.w = half_w,
		.h = half_h
	};
}
