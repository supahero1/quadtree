#ifndef _quadtree_window_h_
#define _quadtree_window_h_

#include <stdint.h>

typedef struct VIEW
{
	float X, Y, W, H;
}
VIEW;

extern VIEW View;

extern void
SetCameraPos(
	float X, float Y
	);

typedef struct RECT
{
	int MinX, MinY, MaxX, MaxY;
}
RECT;

extern RECT
ToScreen(
	float X, float Y, float W, float H
	);

extern void
PaintPixel(
	int X, int Y,
	uint32_t Color
	);

extern void
Clicked(
	float X, float Y
	);

extern void
DrawInit(
	void
	);

enum DRAW_STATE
{
	DRAW_OK,
	DRAW_STOP
};

extern enum DRAW_STATE
DrawStart(
	void
	);

extern void
DrawEnd(
	void
	);

extern void
DrawFree(
	void
	);

#endif /* _quadtree_window_h_ */
