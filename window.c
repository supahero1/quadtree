#include <GLFW/glfw3.h>

#include "window.h"
#include "alloc/include/alloc_ext.h"

#include <assert.h>
#include <string.h>

#define WIDTH 1600
#define HEIGHT 900
#define SCROLL_FACTOR 0.1f
#define LERP_FACTOR 0.2f

#define MIN(a, b)               \
({                              \
    __typeof__ (a) _a = (a);    \
    __typeof__ (b) _b = (b);    \
    _a > _b ? _b : _a;          \
})

#define MAX(a, b)               \
({                              \
    __typeof__ (a) _a = (a);    \
    __typeof__ (b) _b = (b);    \
    _a > _b ? _a : _b;          \
})

typedef struct position_t
{
	double x, y;
}
position_t;

static uint32_t* pixels;
static GLFWwindow* window;
static position_t mouse = {0};
static position_t camera = {0};
static position_t target_camera = {0};
static double fov = 1;
static double target_fov = 1;
static int pressing = 0;
static int mouse_moved = 0;

half_extent_t view = {0};

static int error;

double
get_time(
	void
	)
{
	return glfwGetTime() * 1000.0f;
}

static double
lerp(
	double start,
	double end
	)
{
	return start + (end - start) * LERP_FACTOR;
}

void
set_camera_pos(
	float x, float y
	)
{
	target_camera.x = x;
	target_camera.y = y;
}

rect_t
to_screen(
	rect_extent_t extent
	)
{
	rect_t rect =
	(rect_t)
	{
		.min_x = (extent.min_x - camera.x) * fov + WIDTH * 0.5f,
		.min_y = (extent.min_y - camera.y) * fov + HEIGHT * 0.5f,
		.max_x = (extent.max_x - camera.x) * fov + WIDTH * 0.5f,
		.max_y = (extent.max_y - camera.y) * fov + HEIGHT * 0.5f
	};

	rect.min_x = MAX(-1, MIN(rect.min_x, WIDTH ));
	rect.max_x = MAX(-1, MIN(rect.max_x, WIDTH ));

	rect.min_y = MAX(-1, MIN(rect.min_y, HEIGHT));
	rect.max_y = MAX(-1, MIN(rect.max_y, HEIGHT));

	return rect;
}

void
paint_pixel(
	int x, int y,
	uint32_t color
	)
{
	if(x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
	{
		pixels[x + y * WIDTH] = color;
	}
}

static void
mouse_button_callback(
	GLFWwindow* window,
	int button,
	int action,
	int mods
	)
{
	if(action == GLFW_PRESS)
	{
		pressing = 1;
		mouse_moved = 0;
	}
	else if(action == GLFW_RELEASE)
	{
		pressing = 0;
	}
}

static void
mouse_move_callback(
	GLFWwindow* window,
	double x,
	double y
	)
{
	mouse_moved = 1;
	y = HEIGHT - y;

	double diff_x = x - mouse.x;
	double diff_y = y - mouse.y;

	mouse.x = x;
	mouse.y = y;

	if(pressing)
	{
		target_camera.x -= diff_x / fov;
		target_camera.y -= diff_y / fov;
	}
}

static void
mouse_scroll_callback(
	GLFWwindow* window,
	double offset_x,
	double offset_y
	)
{
	target_fov += offset_y * SCROLL_FACTOR * target_fov;
}

void
draw_init(
	void
	)
{
	error = glfwInit();
	assert(error == GLFW_TRUE);

	window = glfwCreateWindow(WIDTH, HEIGHT, "Quadtree", NULL, NULL);
	assert(window);

	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, mouse_move_callback);
	glfwSetScrollCallback(window, mouse_scroll_callback);

	glfwSetWindowSizeLimits(window, WIDTH, HEIGHT, WIDTH, HEIGHT);

	glfwMakeContextCurrent(window);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, WIDTH, 0, HEIGHT, -1, 1);
	glMatrixMode(GL_MODELVIEW);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	pixels = alloc_malloc(pixels, WIDTH * HEIGHT);
	assert(pixels);
}

enum draw_state_t
draw_start(
	void
	)
{
	memset(pixels, 0xff, sizeof(*pixels) * WIDTH * HEIGHT);

	if(glfwWindowShouldClose(window))
	{
		return DRAW_STATE_STOP;
	}

	glfwPollEvents();

	fov = lerp(fov, target_fov);
	camera =
	(position_t)
	{
		.x = lerp(camera.x, target_camera.x),
		.y = lerp(camera.y, target_camera.y)
	};
	view =
	(half_extent_t)
	{
		.x = camera.x,
		.y = camera.y,
		.w = WIDTH  * 0.5f / fov,
		.h = HEIGHT * 0.5f / fov
	};

	return DRAW_STATE_OK;
}

void
draw_end(
	void
	)
{
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawPixels(WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glFlush();
	glfwSwapBuffers(window);
}

void
draw_free(
	void
	)
{
	alloc_free(pixels, WIDTH * HEIGHT);

	glDisable(GL_BLEND);

	glfwDestroyWindow(window);
	// glfwTerminate();
}
