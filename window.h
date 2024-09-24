#pragma once

#include "quadtree_types.h"


double
GetTime(
	void
	);


extern QuadtreeHalfExtent View;


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
	QuadtreeRectExtent Extent
	);


extern void
PaintPixel(
	int X, int Y,
	uint32_t Color
	);


extern void
Clicked(
	double X, double Y
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
