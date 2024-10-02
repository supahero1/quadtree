#pragma once

#include "quadtree_types.h"


typedef struct QuadtreeNode
{
	uint32_t Count;

	union
	{
		uint32_t Heads[4];

		struct
		{
			uint32_t Head;
			uint32_t PositionFlags;
		};
	};
}
QuadtreeNode;


typedef struct QuadtreeNodeEntity
{
	uint32_t Next;
	uint32_t Entity;
}
QuadtreeNodeEntity;


#ifndef QuadtreeEntityData


	typedef struct QuadtreeEntityDataT
	{
		QuadtreeRectExtent RectExtent;
	}
	QuadtreeEntityDataT;


	#define QuadtreeEntityData QuadtreeEntityDataT
	#define QuadtreeGetEntityDataRectExtent(Entity) (Entity).RectExtent
#endif


typedef struct QuadtreeEntity
{
	union
	{
		QuadtreeEntityData Data;
		uint32_t Next;
	};

	uint32_t QueryTick;
	uint8_t UpdateTick;
	uint8_t Invalid;
}
QuadtreeEntity;


#define QuadtreeGetEntityRectExtent(Entity)	\
QuadtreeGetEntityDataRectExtent((Entity)->Data)


typedef struct QuadtreeNodeInfo
{
	uint32_t NodeIdx;
	QuadtreeHalfExtent Extent;
}
QuadtreeNodeInfo;


typedef struct QuadtreeHTEntry
{
	uint32_t Next;
	uint32_t Idx[2];
}
QuadtreeHTEntry;


typedef struct QuadtreeRemoval
{
	uint32_t EntityIdx;
}
QuadtreeRemoval;


typedef struct QuadtreeNodeRemoval
{
	uint32_t NodeIdx;
	uint32_t NodeEntityIdx;
	uint32_t PrevNodeEntityIdx;
}
QuadtreeNodeRemoval;


typedef struct QuadtreeInsertion
{
	QuadtreeEntityData Data;
}
QuadtreeInsertion;


typedef struct QuadtreeReinsertion
{
	uint32_t EntityIdx;
}
QuadtreeReinsertion;


typedef struct Quadtree Quadtree;


typedef void
(*QuadtreeQueryT)(
	Quadtree* QT,
	uint32_t EntityIdx,
	QuadtreeEntityData* Entity
	);


typedef void
(*QuadtreeNodeQueryT)(
	Quadtree* QT,
	const QuadtreeNodeInfo* Info
	);


typedef void
(*QuadtreeCollideT)(
	const Quadtree* QT,
	QuadtreeEntityData* EntityA,
	QuadtreeEntityData* EntityB
	);


typedef bool
(*QuadtreeUpdateT)(
	Quadtree* QT,
	uint32_t EntityIdx,
	QuadtreeEntityData* Entity
	);


struct Quadtree
{
	QuadtreeNode* Nodes;
	QuadtreeNodeEntity* NodeEntities;
	QuadtreeEntity* Entities;

	QuadtreeHTEntry* HTEntries;
	QuadtreeRemoval* Removals;
	QuadtreeNodeRemoval* NodeRemovals;
	QuadtreeInsertion* Insertions;
	QuadtreeReinsertion* Reinsertions;

	int32_t* NodeMap;

	uint32_t FreeNode;
	uint32_t NodesUsed;
	uint32_t NodesSize;

	uint32_t FreeNodeEntity;
	uint32_t NodeEntitiesUsed;
	uint32_t NodeEntitiesSize;

	uint32_t FreeEntity;
	uint32_t EntitiesUsed;
	uint32_t EntitiesSize;

	uint32_t HTEntriesUsed;
	uint32_t HTEntriesSize;

	uint32_t RemovalsUsed;
	uint32_t RemovalsSize;

	uint32_t NodeRemovalsUsed;
	uint32_t NodeRemovalsSize;

	uint32_t InsertionsUsed;
	uint32_t InsertionsSize;

	uint32_t ReinsertionsUsed;
	uint32_t ReinsertionsSize;

	uint32_t QueryTick;
	uint8_t UpdateTick;

	QuadtreeRectExtent RectExtent;
	QuadtreeHalfExtent HalfExtent;

	QuadtreePosition MinSize;
};


extern void
QuadtreeInit(
	Quadtree* QT
	);


extern void
QuadtreeFree(
	Quadtree* QT
	);


extern void
QuadtreeInsert(
	Quadtree* QT,
	const QuadtreeEntityData* Data
	);


extern void
QuadtreeRemove(
	Quadtree* QT,
	uint32_t EntityIdx
	);


extern void
QuadtreeUpdate(
	Quadtree* QT,
	QuadtreeUpdateT Callback
	);


extern void
QuadtreeQuery(
	Quadtree* QT,
	QuadtreeRectExtent Extent,
	QuadtreeQueryT Callback
	);


extern void
QuadtreeQueryNodes(
	Quadtree* QT,
	QuadtreeRectExtent Extent,
	QuadtreeNodeQueryT Callback
	);


extern void
QuadtreeCollide(
	Quadtree* QT,
	QuadtreeCollideT Callback
	);
