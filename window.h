#pragma once

#include "extent.h"

double
get_time(
	void
	);

extern half_extent_t view;

extern void
set_camera_pos(
	float x, float y
	);

typedef struct rect_t
{
	int min_x, min_y, max_x, max_y;
}
rect_t;

extern rect_t
to_screen(
	rect_extent_t extent
	);

extern void
paint_pixel(
	int x, int y,
	uint32_t color
	);

extern void
draw_init(
	void
	);

enum draw_state_t
{
	DRAW_STATE_OK,
	DRAW_STATE_STOP
};

extern enum draw_state_t
draw_start(
	void
	);

extern void
draw_end(
	void
	);

extern void
draw_free(
	void
	);
