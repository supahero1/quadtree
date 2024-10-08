#include "quadtree_types.h"


typedef struct ENTITY
{
	QuadtreeRectExtent Extent;
	QuadtreePosition VX, VY;
}
ENTITY;


#define QuadtreeEntityData ENTITY
#define QuadtreeGetEntityDataRectExtent(Entity) (Entity).Extent


#include "window.c"
#include "alloc/src/debug.c"
#include "alloc/src/alloc.c"
#include "quadtree.c"

#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <tgmath.h>

#define ITER UINT32_C(200000)
#define RADIUS_ODDS 2500.0f
#define RADIUS_MIN 16.0f
#define RADIUS_MAX 1024.0f
#define MIN_SIZE 32.0f
#define ARENA_WIDTH 100000.0f
#define ARENA_HEIGHT 100000.0f
#define MEASURE_TICKS 60
#define INITIAL_VELOCITY 0.9f
#define BOUNDS_VELOCITY_LOSS 0.99f
#define CANT_ESCAPE_AREA 1
#define DO_THEM_QUERIES 0
#define QUERIES_NUM 1000

static Quadtree QT = {0};

static double Start, End;
static double Time;

typedef struct MEASUREMENT
{
	double Times[MEASURE_TICKS];
	uint64_t Count;
}
MEASUREMENT;

static MEASUREMENT MeasureNormalize;
static MEASUREMENT MeasureCollide;
static MEASUREMENT MeasureUpdate;
#if DO_THEM_QUERIES == 1
static MEASUREMENT MeasureQuery;
#endif

static float
randf(
	void
	)
{
	return rand() / (float) RAND_MAX;
}

static double
Measure(
	MEASUREMENT* Measurement,
	double Time
	)
{
	Measurement->Times[Measurement->Count] = Time;
	Measurement->Count = (Measurement->Count + 1) % MEASURE_TICKS;
	if(Measurement->Count == 0)
	{
		double Total = 0;
		for(uint64_t i = 0; i < MEASURE_TICKS; ++i)
		{
			Total += Measurement->Times[i];
		}
		Total /= MEASURE_TICKS;
		return Total;
	}
	return 0;
}


static QuadtreeStatus
UpdateEntity(
	Quadtree* QT,
	uint32_t EntityIdx,
	ENTITY* Entity
	)
{
	Entity->Extent.MinX += Entity->VX;
	Entity->Extent.MaxX += Entity->VX;
	Entity->Extent.MinY += Entity->VY;
	Entity->Extent.MaxY += Entity->VY;

#if CANT_ESCAPE_AREA == 1
	if(Entity->Extent.MinX < QT->RectExtent.MinX)
	{
		QuadtreePosition Width = Entity->Extent.MaxX - Entity->Extent.MinX;
		Entity->Extent.MinX = QT->RectExtent.MinX;
		Entity->Extent.MaxX = QT->RectExtent.MinX + Width;
		Entity->VX = fabs(Entity->VX) * BOUNDS_VELOCITY_LOSS;
	}
	else if(Entity->Extent.MaxX > QT->RectExtent.MaxX)
	{
		QuadtreePosition Width = Entity->Extent.MaxX - Entity->Extent.MinX;
		Entity->Extent.MaxX = QT->RectExtent.MaxX;
		Entity->Extent.MinX = QT->RectExtent.MaxX - Width;
		Entity->VX = -fabs(Entity->VX) * BOUNDS_VELOCITY_LOSS;
	}

	if(Entity->Extent.MinY < QT->RectExtent.MinY)
	{
		QuadtreePosition Height = Entity->Extent.MaxY - Entity->Extent.MinY;
		Entity->Extent.MinY = QT->RectExtent.MinY;
		Entity->Extent.MaxY = QT->RectExtent.MinY + Height;
		Entity->VY = fabs(Entity->VY) * BOUNDS_VELOCITY_LOSS;
	}
	else if(Entity->Extent.MaxY > QT->RectExtent.MaxY)
	{
		QuadtreePosition Height = Entity->Extent.MaxY - Entity->Extent.MinY;
		Entity->Extent.MaxY = QT->RectExtent.MaxY;
		Entity->Extent.MinY = QT->RectExtent.MaxY - Height;
		Entity->VY = -fabs(Entity->VY) * BOUNDS_VELOCITY_LOSS;
	}
#else
	if(Entity->Extent.MinX < QT->RectExtent.MinX)
	{
		Entity->VX *= BOUNDS_VELOCITY_LOSS;
		++Entity->VX;
	}
	else if(Entity->Extent.MaxX > QT->RectExtent.MaxX)
	{
		Entity->VX *= BOUNDS_VELOCITY_LOSS;
		--Entity->VX;
	}

	if(Entity->Extent.MinY < QT->RectExtent.MinY)
	{
		Entity->VY *= BOUNDS_VELOCITY_LOSS;
		++Entity->VY;
	}
	else if(Entity->Extent.MaxY > QT->RectExtent.MaxY)
	{
		Entity->VY *= BOUNDS_VELOCITY_LOSS;
		--Entity->VY;
	}
#endif

	return QUADTREE_STATUS_CHANGED;
}

static void
CollideEntities(
	const Quadtree* QT,
	ENTITY* EntityA,
	ENTITY* EntityB
	)
{
	QuadtreeHalfExtent ExtentA = QuadtreeRectToHalfExtent(EntityA->Extent);
	QuadtreeHalfExtent ExtentB = QuadtreeRectToHalfExtent(EntityB->Extent);

	QuadtreePosition DiffX = ExtentA.X - ExtentB.X;
	QuadtreePosition DiffY = ExtentA.Y - ExtentB.Y;
	QuadtreePosition OverlapX = (ExtentA.W + ExtentB.W) - fabs(DiffX);
	QuadtreePosition OverlapY = (ExtentA.H + ExtentB.H) - fabs(DiffY);

	if(OverlapX > 0 && OverlapY > 0)
	{
		QuadtreePosition SizeA = ExtentA.W * ExtentA.H * 4.0f;
		QuadtreePosition SizeB = ExtentB.W * ExtentB.H * 4.0f;
		QuadtreePosition TotalSize = SizeA + SizeB;

		if(OverlapX < OverlapY)
		{
			QuadtreePosition PushA = OverlapX * (SizeB / TotalSize);
			QuadtreePosition PushB = OverlapX * (SizeA / TotalSize);

			if(DiffX > 0)
			{
				EntityA->Extent.MinX += PushA;
				EntityA->Extent.MaxX += PushA;
				EntityB->Extent.MinX -= PushB;
				EntityB->Extent.MaxX -= PushB;
			}
			else
			{
				EntityA->Extent.MinX -= PushA;
				EntityA->Extent.MaxX -= PushA;
				EntityB->Extent.MinX += PushB;
				EntityB->Extent.MaxX += PushB;
			}

			QuadtreePosition TempVX = EntityA->VX;
			EntityA->VX = (EntityA->VX * (SizeA - SizeB) + 2 * SizeB * EntityB->VX) / TotalSize;
			EntityB->VX = (EntityB->VX * (SizeB - SizeA) + 2 * SizeA * TempVX) / TotalSize;
		}
		else
		{
			QuadtreePosition PushA = OverlapY * (SizeB / TotalSize);
			QuadtreePosition PushB = OverlapY * (SizeA / TotalSize);

			if(DiffY > 0)
			{
				EntityA->Extent.MinY += PushA;
				EntityA->Extent.MaxY += PushA;
				EntityB->Extent.MinY -= PushB;
				EntityB->Extent.MaxY -= PushB;
			}
			else
			{
				EntityA->Extent.MinY -= PushA;
				EntityA->Extent.MaxY -= PushA;
				EntityB->Extent.MinY += PushB;
				EntityB->Extent.MaxY += PushB;
			}

			QuadtreePosition TempVY = EntityA->VY;
			EntityA->VY = (EntityA->VY * (SizeA - SizeB) + 2 * SizeB * EntityB->VY) / TotalSize;
			EntityB->VY = (EntityB->VY * (SizeB - SizeA) + 2 * SizeA * TempVY) / TotalSize;
		}
	}
}

static void
DrawNode(
	Quadtree* QT,
	const QuadtreeNodeInfo* Info
	)
{
	RECT Rect = ToScreen(QuadtreeHalfToRectExtent(Info->Extent));

	for(int X = Rect.MinX; X <= Rect.MaxX; ++X)
	{
		if(X & 1)
		{
			PaintPixel(X, Rect.MinY, 0x40000000);
			PaintPixel(X, Rect.MaxY, 0x40000000);
		}
	}

	for(int Y = Rect.MinY; Y <= Rect.MaxY; ++Y)
	{
		if(Y & 1)
		{
			PaintPixel(Rect.MinX, Y, 0x40000000);
			PaintPixel(Rect.MaxX, Y, 0x40000000);
		}
	}
}

static void
DrawEntity(
	Quadtree* QT,
	uint32_t EntityIdx,
	ENTITY* Entity
	)
{
	RECT Rect = ToScreen(Entity->Extent);

	for(int X = Rect.MinX; X <= Rect.MaxX; ++X)
	{
		PaintPixel(X, Rect.MinY, 0xFF000000);
		PaintPixel(X, Rect.MaxY, 0xFF000000);
	}

	for(int Y = Rect.MinY; Y <= Rect.MaxY; ++Y)
	{
		PaintPixel(Rect.MinX, Y, 0xFF000000);
		PaintPixel(Rect.MaxX, Y, 0xFF000000);
	}
}

float
GenRadius(
	void
	)
{
	float R = randf() * RADIUS_ODDS;
	float Len = RADIUS_MAX - RADIUS_MIN;
	R = (1.0f / R) * Len + RADIUS_MIN;
	R = fminf(R, RADIUS_MAX);
	return R;
}

static void
Init(
	void
	)
{
	printf(
		"Simulation settings:\n"
		"Initial count:    %d\n"
		"Arena size:       %.01Lf x %.01Lf\n"
		"Initial Radius:   From %.01f to %.01f\n"
		, ITER, (long double) QT.HalfExtent.W * 2, (long double) QT.HalfExtent.H * 2, RADIUS_MIN, RADIUS_MAX
		);

	ENTITY* Rands = malloc(sizeof(*Rands) * ITER);
	assert(Rands);

	for(int i = 0; i < ITER; ++i)
	{
		float Dim[2];
		int Idx = i & 1;

		float W = GenRadius();
		float Temp = GenRadius();
		float H = W + (Temp - Temp * 0.5f);
		H = fminf(H, RADIUS_MAX);
		H = fmaxf(H, RADIUS_MIN);

		Dim[Idx] = W;
		Dim[!Idx] = H;

		W = Dim[0];
		H = Dim[1];

		float QTW = QT.HalfExtent.W * 2.0f - W;
		float QTH = QT.HalfExtent.H * 2.0f - H;

		Rands[i].Extent.MinX = QT.RectExtent.MinX + QTW * randf();
		Rands[i].Extent.MaxX = Rands[i].Extent.MinX + W;

		Rands[i].Extent.MinY = QT.RectExtent.MinY + QTH * randf();
		Rands[i].Extent.MaxY = Rands[i].Extent.MinY + H;

		Rands[i].VX = (1 - 2 * randf()) * INITIAL_VELOCITY;
		Rands[i].VY = (1 - 2 * randf()) * INITIAL_VELOCITY;
	}

	Start = GetTime();
	for(int i = 0; i < ITER; ++i)
	{
		QuadtreeInsert(&QT, Rands + i);
	}
	End = GetTime();
	printf("Insertion: %.02lfms\n", End - Start);

	free(Rands);
}

static void
QueryIgnore(
	Quadtree* QT,
	QuadtreeEntity* Entity
	)
{
	(void) QT;
	(void) Entity;
}

static void
Tick(
	void
	)
{
	Start = GetTime();
	QuadtreeNormalize(&QT);
	End = GetTime();
	Time = Measure(&MeasureNormalize, End - Start);
	if(Time)
	{
		printf("\nNormalize: %.02lfms\n", Time);
	}

	Start = GetTime();
	QuadtreeCollideSlow(&QT, CollideEntities);
	End = GetTime();
	Time = Measure(&MeasureCollide, End - Start);
	if(Time)
	{
		printf("Collide: %.02lfms\n", Time);
	}

	Start = GetTime();
	QuadtreeUpdate(&QT, UpdateEntity);
	End = GetTime();
	Time = Measure(&MeasureUpdate, End - Start);
	if(Time)
	{
		printf("Update: %.02lfms\n", Time);
		printf("Nodes: %u\n", QT.NodesUsed);
		printf("Node entities: %u\n", QT.NodeEntitiesUsed);
	}

#if DO_THEM_QUERIES == 1
	Start = GetTime();
	for(int i = 1; i <= QUERIES_NUM; ++i)
	{
		QuadtreeHalfExtent Extent = QuadtreeRectToHalfExtent(QT.Entities[i].Extent);
		QuadtreeQuery(
			&QT, QuadtreeHalfToRectExtent((QuadtreeHalfExtent){ Extent.X, Extent.Y, 1920 * 0.5f, 1080 * 0.5f }), QueryIgnore);
	}
	End = GetTime();
	Time = Measure(&MeasureQuery, End - Start);
	if(Time)
	{
		printf("1k Queries: %.02lfms\n", Time);
	}
#endif
}

int
main()
{
	DrawInit();

	uint64_t Seed = GetTime() * 100000;
	printf("Seed: %lu\n", Seed);
	srand(Seed);

	QT.RectExtent =
	(QuadtreeRectExtent)
	{
		.MinX = -ARENA_WIDTH * 0.5f,
		.MaxX = ARENA_WIDTH * 0.5f,
		.MinY = -ARENA_HEIGHT * 0.5f,
		.MaxY = ARENA_HEIGHT * 0.5f
	};

	QT.HalfExtent =
	(QuadtreeHalfExtent)
	{
		.X = 0,
		.Y = 0,
		.W = ARENA_WIDTH * 0.5f,
		.H = ARENA_HEIGHT * 0.5f
	};

	QT.MinSize = MIN_SIZE;

	QuadtreeInit(&QT);

	Init();

	while(1)
	{
		if(DrawStart() != DRAW_OK)
		{
			break;
		}

		Tick();

		QuadtreeRectExtent RectView = QuadtreeHalfToRectExtent(View);

		QuadtreeQueryNodes(&QT, RectView, DrawNode);
		QuadtreeQuery(&QT, RectView, DrawEntity);

		DrawEnd();
	}

	DrawFree();

	QuadtreeFree(&QT);

	return 0;
}
