#pragma once

#include <stdint.h>
#include <stdbool.h>

/* How many entities can exist in a node without it splitting */
#define QUADTREE_SPLIT_THRESHOLD 7

/* You might want to increase this if you get a lot of collisions per tick */
#define QUADTREE_HASH_TABLE_FACTOR 1

#define QUADTREE_DEDUPE_COLLISIONS 1

/* Do not modify unless you know what you are doing. Use Quadtree.MinSize. */
#define QUADTREE_MAX_DEPTH 20

/* Do not modify */
#define QUADTREE_DFS_LENGTH (QUADTREE_MAX_DEPTH * 3 + 1)


typedef float QuadtreePosition;


typedef struct QuadtreeRectExtent
{
	QuadtreePosition MinX, MaxX, MinY, MaxY;
}
QuadtreeRectExtent;


typedef struct QuadtreeHalfExtent
{
	QuadtreePosition X, Y, W, H;
}
QuadtreeHalfExtent;


static inline bool
QuadtreeIntersects(
	QuadtreeRectExtent A,
	QuadtreeRectExtent B
	)
{
	return
		A.MaxX >= B.MinX &&
		A.MaxY >= B.MinY &&
		B.MaxX >= A.MinX &&
		B.MaxY >= A.MinY;
}


static inline bool
QuadtreeIsInside(
	QuadtreeRectExtent A,
	QuadtreeRectExtent B
	)
{
	return
		A.MinX > B.MinX &&
		A.MinY > B.MinY &&
		B.MaxX > A.MaxX &&
		B.MaxY > A.MaxY;
}


static inline QuadtreeRectExtent
QuadtreeHalfToRectExtent(
	QuadtreeHalfExtent HalfExtent
	)
{
	return
	(QuadtreeRectExtent)
	{
		.MinX = HalfExtent.X - HalfExtent.W,
		.MaxX = HalfExtent.X + HalfExtent.W,
		.MinY = HalfExtent.Y - HalfExtent.H,
		.MaxY = HalfExtent.Y + HalfExtent.H
	};
}


static inline QuadtreeHalfExtent
QuadtreeRectToHalfExtent(
	QuadtreeRectExtent RectExtent
	)
{
	return
	(QuadtreeHalfExtent)
	{
		.X = (RectExtent.MaxX + RectExtent.MinX) * 0.5f,
		.Y = (RectExtent.MaxY + RectExtent.MinY) * 0.5f,
		.W = (RectExtent.MaxX - RectExtent.MinX) * 0.5f,
		.H = (RectExtent.MaxY - RectExtent.MinY) * 0.5f
	};
}
