#include "window.h"
#include "quadtree.h"

#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>

#define ITER UINT32_C(100000)
#define RADIUS_ITER 100
#define RADIUS_MIN 4.0f
#define RADIUS_MAX 1000.0f
#define MIN_SIZE 8.0f
#define ARENA_WIDTH 65536.0f
#define ARENA_HEIGHT 65536.0f
#define MEASURE_TICKS 50
#define VELOCITY_LOSS 0.9995f
#define COLLISION_STRENGTH 1.0f
#define INITIAL_VELOCITY 1.0f
#define BOUNDS_VELOCITY_LOSS 0.99f
#define CANT_ESCAPE_AREA 1

typedef struct ENTITY
{
	QUADTREE_POSITION_T VX, VY;
}
ENTITY;

static QUADTREE Quadtree;
static ENTITY Entities[ITER + 1];
static uint32_t FollowingEntity = -1;
static QUADTREE_POSITION_T MaxVelocity;

static double Start, End;
static double Time;

typedef struct MEASUREMENT
{
	double Times[MEASURE_TICKS];
	uint64_t Count;
}
MEASUREMENT;

static MEASUREMENT MeasureCollide;
static MEASUREMENT MeasureUpdate;
static MEASUREMENT MeasureQuery;

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

static void
ClickQuery(
	QUADTREE* Quadtree,
	uint32_t EntityIdx
	)
{
	QUADTREE_POSITION_T Velocity = sqrt(
		Entities[EntityIdx].VX * Entities[EntityIdx].VX +
		Entities[EntityIdx].VY * Entities[EntityIdx].VY
		);
	if(Velocity > MaxVelocity)
	{
		MaxVelocity = Velocity;
		FollowingEntity = EntityIdx;
	}
}


void
Clicked(
	double X, double Y
	)
{
	FollowingEntity = -1;
	MaxVelocity = 0.0f;
	QUADTREE_QUERY_T OldQuery = Quadtree.Query;
	Quadtree.Query = ClickQuery;
	QuadtreeQuery(&Quadtree, X, Y, View.W, View.H);
	Quadtree.Query = OldQuery;
}

static uint8_t
UpdateEntity(
	QUADTREE* Quadtree,
	uint32_t EntityIdx
	)
{
	QUADTREE_ENTITY* QTEntity = Quadtree->Entities + EntityIdx;
	ENTITY* Entity = Entities + EntityIdx;

	Entity->VX *= VELOCITY_LOSS;
	Entity->VY *= VELOCITY_LOSS;

	QUADTREE_ENTITY Copy = *QTEntity;

	QTEntity->X += Entity->VX;
	QTEntity->Y += Entity->VY;

#if CANT_ESCAPE_AREA == 1
	if(QTEntity->X - QTEntity->W < Quadtree->X - Quadtree->W)
	{
		QTEntity->X = Quadtree->X - Quadtree->W + QTEntity->W;
		Entity->VX = fabs(Entity->VX) * BOUNDS_VELOCITY_LOSS;
	}
	else if(QTEntity->X + QTEntity->W > Quadtree->X + Quadtree->W)
	{
		QTEntity->X = Quadtree->X + Quadtree->W - QTEntity->W;
		Entity->VX = -fabs(Entity->VX) * BOUNDS_VELOCITY_LOSS;
	}

	if(QTEntity->Y - QTEntity->H < Quadtree->Y - Quadtree->H)
	{
		QTEntity->Y = Quadtree->Y - Quadtree->H + QTEntity->H;
		Entity->VY = fabs(Entity->VY) * BOUNDS_VELOCITY_LOSS;
	}
	else if(QTEntity->Y + QTEntity->H > Quadtree->X + Quadtree->H)
	{
		QTEntity->Y = Quadtree->Y + Quadtree->H - QTEntity->H;
		Entity->VY = -fabs(Entity->VY) * BOUNDS_VELOCITY_LOSS;
	}
#else
	if(QTEntity->X - QTEntity->W < Quadtree->X - Quadtree->W)
	{
		Entity->VX *= BOUNDS_VELOCITY_LOSS;
		++Entity->VX;
	}
	else if(QTEntity->X + QTEntity->W > Quadtree->X + Quadtree->W)
	{
		Entity->VX *= BOUNDS_VELOCITY_LOSS;
		--Entity->VX;
	}

	if(QTEntity->Y - QTEntity->H < Quadtree->Y - Quadtree->H)
	{
		Entity->VY *= BOUNDS_VELOCITY_LOSS;
		++Entity->VY;
	}
	else if(QTEntity->Y + QTEntity->H > Quadtree->X + Quadtree->H)
	{
		Entity->VY *= BOUNDS_VELOCITY_LOSS;
		--Entity->VY;
	}
#endif

	return (Copy.X != QTEntity->X || Copy.Y != QTEntity->Y ||
		Copy.W != QTEntity->W || Copy.H != QTEntity->H);
}

static int
Intersects(
	QUADTREE_POSITION_T x1, QUADTREE_POSITION_T y1,
	QUADTREE_POSITION_T w1, QUADTREE_POSITION_T h1,

	QUADTREE_POSITION_T x2, QUADTREE_POSITION_T y2,
	QUADTREE_POSITION_T w2, QUADTREE_POSITION_T h2
	)
{
	return
		x1 + w1 >= x2 - w2 &&
		y1 + h1 >= y2 - h2 &&
		x2 + w2 >= x1 - w1 &&
		y2 + h2 >= y1 - h1;
}

static int
AreEntitiesColliding(
	const QUADTREE* Quadtree,
	uint32_t EntityAIdx,
	uint32_t EntityBIdx
	)
{
	QUADTREE_ENTITY* EntityA = Quadtree->Entities + EntityAIdx;
	QUADTREE_ENTITY* EntityB = Quadtree->Entities + EntityBIdx;

	return Intersects(
		EntityA->X, EntityA->Y, EntityA->W, EntityA->H,
		EntityB->X, EntityB->Y, EntityB->W, EntityB->H
	);
}

static void
CollideEntities(
	QUADTREE* Quadtree,
	uint32_t EntityAIdx,
	uint32_t EntityBIdx
	)
{
	QUADTREE_ENTITY* QTEntityA = Quadtree->Entities + EntityAIdx;
	QUADTREE_ENTITY* QTEntityB = Quadtree->Entities + EntityBIdx;
	ENTITY* EntityA = Entities + EntityAIdx;
	ENTITY* EntityB = Entities + EntityBIdx;

	QUADTREE_POSITION_T DiffX = QTEntityA->X - QTEntityB->X;
	QUADTREE_POSITION_T DiffY = QTEntityA->Y - QTEntityB->Y;
	QUADTREE_POSITION_T Length = sqrt(DiffX * DiffX + DiffY * DiffY) / COLLISION_STRENGTH;
	if(Length == 0)
	{
		DiffX = 1;
		DiffY = 1;
		Length = 2;
	}
	Length = 1 / Length;
	DiffX *= Length;
	DiffY *= Length;

	QUADTREE_POSITION_T MassA = QTEntityA->W * QTEntityA->H;
	QUADTREE_POSITION_T MassB = QTEntityB->W * QTEntityB->H;
	QUADTREE_POSITION_T MassTotal = MassA + MassB;
	QUADTREE_POSITION_T MultiplierA = (MassTotal - MassA) / MassTotal;
	QUADTREE_POSITION_T MultiplierB = (MassTotal - MassB) / MassTotal;

	EntityA->VX += DiffX * MultiplierA;
	EntityA->VY += DiffY * MultiplierA;

	EntityB->VX -= DiffX * MultiplierB;
	EntityB->VY -= DiffY * MultiplierB;
}

static void
DrawNode(
	QUADTREE* Quadtree,
	const QUADTREE_NODE_INFO* Info
	)
{
	RECT Rect = ToScreen(Info->X, Info->Y, Info->W, Info->H);

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
	QUADTREE* Quadtree,
	uint32_t EntityIdx
	)
{
	const QUADTREE_ENTITY* Entity = Quadtree->Entities + EntityIdx;
	RECT Rect = ToScreen(Entity->X, Entity->Y, Entity->W, Entity->H);

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
		, ITER, (long double) Quadtree.W * 2, (long double) Quadtree.H * 2, RADIUS_MIN, RADIUS_MAX
		);

	struct
	{
		QUADTREE_POSITION_T X, Y, R;
	}
	*Rands = malloc(sizeof(*Rands) * ITER);
	assert(Rands);

	for(int i = 0; i < ITER; ++i)
	{
		Rands[i].R = RADIUS_MAX;
		for(int j = 0; j < RADIUS_ITER; ++j)
		{
			QUADTREE_POSITION_T Radius = RADIUS_MIN + (RADIUS_MAX - RADIUS_MIN) * randf();
			if(Radius < Rands[i].R)
			{
				Rands[i].R = Radius;
			}
		}
		Rands[i].X = Quadtree.X - Quadtree.W + Rands[i].R +
			randf() * (Quadtree.W - Rands[i].R) * 2;
		Rands[i].Y = Quadtree.Y - Quadtree.H + Rands[i].R +
			randf() * (Quadtree.H - Rands[i].R) * 2;
	}

	Start = GetTime();
	for(int i = 0; i < ITER; ++i)
	{
		uint32_t Idx = QuadtreeInsert(&Quadtree, &(
			(QUADTREE_ENTITY)
			{
				.X = Rands[i].X,
				.Y = Rands[i].Y,
				.W = Rands[i].R,
				.H = Rands[i].R
			}
			));
		Entities[Idx] =
		(ENTITY)
		{
			.VX = (1 - 2 * randf()) * INITIAL_VELOCITY,
			.VY = (1 - 2 * randf()) * INITIAL_VELOCITY
		};
	}
	End = GetTime();
	printf("Insertion: %.02lfms\n", End - Start);

	free(Rands);
}

static void
QueryIgnore(
	QUADTREE* Quadtree,
	uint32_t EntityIndex
	)
{
	(void) Quadtree;
	(void) EntityIndex;
}

static void
Tick(
	void
	)
{
	Start = GetTime();
	QuadtreeCollide(&Quadtree);
	End = GetTime();
	Time = Measure(&MeasureCollide, End - Start);
	if(Time)
	{
		printf("Collide: %.02lfms\n", Time);
	}

	Start = GetTime();
	QuadtreeUpdate(&Quadtree);
	End = GetTime();
	Time = Measure(&MeasureUpdate, End - Start);
	if(Time)
	{
		printf("Update: %.02lfms\n", Time);
	}

	QUADTREE_QUERY_T OldQuery = Quadtree.Query;
	Quadtree.Query = QueryIgnore;
	Start = GetTime();
	for(int i = 1; i <= 1000; ++i)
	{
		QuadtreeQuery(
			&Quadtree,
			Quadtree.Entities[i].X,
			Quadtree.Entities[i].Y,
			1920 * 0.5f,
			1080 * 0.5f
			);
	}
	End = GetTime();
	Quadtree.Query = OldQuery;
	Time = Measure(&MeasureQuery, End - Start);
	if(Time)
	{
		printf("1k Queries: %.02lfms\n", Time);
	}
}

int
main()
{
	DrawInit();

	uint64_t Seed = GetTime() * 100000;
	printf("Seed: %lu\n", Seed);
	srand(Seed);

	Quadtree.X = 0;
	Quadtree.Y = 0;
	Quadtree.W = ARENA_WIDTH * 0.5f;
	Quadtree.H = ARENA_HEIGHT * 0.5f;

	Quadtree.Query = DrawEntity;
	Quadtree.NodeQuery = DrawNode;
	Quadtree.Update = UpdateEntity;
	Quadtree.IsColliding = AreEntitiesColliding;
	Quadtree.Collide = CollideEntities;

	Quadtree.MinSize = MIN_SIZE;

	QuadtreeInit(&Quadtree);

	Init();

	while(1)
	{
		if(FollowingEntity != -1)
		{
			SetCameraPos(Quadtree.Entities[FollowingEntity].X, Quadtree.Entities[FollowingEntity].Y);
		}

		if(DrawStart() != DRAW_OK)
		{
			break;
		}

		Tick();

		QuadtreeQueryNodes(&Quadtree, View.X, View.Y, View.W, View.H);
		QuadtreeQuery(&Quadtree, View.X, View.Y, View.W, View.H);

		DrawEnd();
	}

	DrawFree();

	QuadtreeFree(&Quadtree);

	return 0;
}
