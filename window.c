#include <GLFW/glfw3.h>

#include "window.h"

#include <assert.h>
#include <stdlib.h>
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

typedef struct POSITION
{
	double X, Y;
}
POSITION;

static uint32_t* Pixels;
static GLFWwindow* Window;
static POSITION Mouse = {0};
static POSITION Camera = {0};
static POSITION TargetCamera = {0};
static double FoV = 1;
static double TargetFoV = 1;
static int Pressing = 0;
static int MouseMoved = 0;

QuadtreeHalfExtent View = {0};

static int Error;


double
GetTime(
	void
	)
{
	return glfwGetTime() * 1000.0f;
}


static double
Lerp(
	double Start,
	double End
	)
{
	return Start + (End - Start) * LERP_FACTOR;
}


void
SetCameraPos(
	float X, float Y
	)
{
	TargetCamera.X = X;
	TargetCamera.Y = Y;
}


RECT
ToScreen(
	QuadtreeRectExtent Extent
	)
{
	RECT Rect =
	(RECT)
	{
		.MinX = (Extent.MinX - Camera.X) * FoV + WIDTH * 0.5f,
		.MinY = (Extent.MinY - Camera.Y) * FoV + HEIGHT * 0.5f,
		.MaxX = (Extent.MaxX - Camera.X) * FoV + WIDTH * 0.5f,
		.MaxY = (Extent.MaxY - Camera.Y) * FoV + HEIGHT * 0.5f
	};

	Rect.MinX = MAX(-1, MIN(Rect.MinX, WIDTH ));
	Rect.MaxX = MAX(-1, MIN(Rect.MaxX, WIDTH ));

	Rect.MinY = MAX(-1, MIN(Rect.MinY, HEIGHT));
	Rect.MaxY = MAX(-1, MIN(Rect.MaxY, HEIGHT));

	return Rect;
}


void
PaintPixel(
	int X, int Y,
	uint32_t Color
	)
{
	if(X >= 0 && X < WIDTH && Y >= 0 && Y < HEIGHT)
	{
		Pixels[X + Y * WIDTH] = Color;
	}
}


static void
MouseButtonCallback(
	GLFWwindow* Window,
	int Button,
	int Action,
	int Mods
	)
{
	if(Button != 0)
	{
		Clicked(10000000000.0f, 10000000000.0f);
		return;
	}

	if(Action == GLFW_PRESS)
	{
		Pressing = 1;
		MouseMoved = 0;
	}
	else if(Action == GLFW_RELEASE)
	{
		Pressing = 0;
		if(!MouseMoved)
		{
			Clicked(
				(Mouse.X - WIDTH * 0.5f) / FoV + Camera.X,
				(Mouse.Y - HEIGHT * 0.5f) / FoV + Camera.Y
			);
		}
	}
}


static void
MouseMoveCallback(
	GLFWwindow* Window,
	double X,
	double Y
	)
{
	MouseMoved = 1;
	Y = HEIGHT - Y;

	double DiffX = X - Mouse.X;
	double DiffY = Y - Mouse.Y;

	Mouse.X = X;
	Mouse.Y = Y;

	if(Pressing)
	{
		TargetCamera.X -= DiffX / FoV;
		TargetCamera.Y -= DiffY / FoV;
	}
}


static void
MouseScrollCallback(
	GLFWwindow* Window,
	double OffsetX,
	double OffsetY
	)
{
	TargetFoV += OffsetY * SCROLL_FACTOR * TargetFoV;
}


void
DrawInit(
	void
	)
{
	Error = glfwInit();
	assert(Error == GLFW_TRUE);

	Window = glfwCreateWindow(WIDTH, HEIGHT, "Quadtree", NULL, NULL);
	assert(Window);

	glfwSetMouseButtonCallback(Window, MouseButtonCallback);
	glfwSetCursorPosCallback(Window, MouseMoveCallback);
	glfwSetScrollCallback(Window, MouseScrollCallback);

	glfwSetWindowSizeLimits(Window, WIDTH, HEIGHT, WIDTH, HEIGHT);

	glfwMakeContextCurrent(Window);

	Pixels = malloc(sizeof(*Pixels) * WIDTH * HEIGHT);
	assert(Pixels);
}


enum DRAW_STATE
DrawStart(
	void
	)
{
	memset(Pixels, 0xff, sizeof(*Pixels) * WIDTH * HEIGHT);

	if(glfwWindowShouldClose(Window))
	{
		return DRAW_STOP;
	}

	glfwPollEvents();

	FoV = Lerp(FoV, TargetFoV);
	Camera =
	(POSITION)
	{
		.X = Lerp(Camera.X, TargetCamera.X),
		.Y = Lerp(Camera.Y, TargetCamera.Y)
	};
	View =
	(QuadtreeHalfExtent)
	{
		.X = Camera.X,
		.Y = Camera.Y,
		.W = WIDTH  * 0.5f / FoV,
		.H = HEIGHT * 0.5f / FoV
	};

	return DRAW_OK;
}


void
DrawEnd(
	void
	)
{
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawPixels(WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, Pixels);
	glFlush();
	glfwSwapBuffers(Window);
}


void
DrawFree(
	void
	)
{
	free(Pixels);

	glfwDestroyWindow(Window);
	glfwTerminate();
}
