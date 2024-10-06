#pragma once

#include "quadtree_types.h"


typedef struct QuadtreeNode
{
	int32_t Count;

	union
	{
		uint32_t Next;

		struct
		{
			uint32_t Head;
			uint32_t PositionFlags;
		};

		uint32_t Heads[4];
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


typedef enum QuadtreeStatus
{
	QUADTREE_STATUS_CHANGED,
	QUADTREE_STATUS_NOT_CHANGED,
	kQUADTREE_STATUS
}
QuadtreeStatus;


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


typedef QuadtreeStatus
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
#if QUADTREE_DEDUPE_COLLISIONS == 1
	QuadtreeHTEntry* HTEntries;
#endif
	QuadtreeRemoval* Removals;
	QuadtreeNodeRemoval* NodeRemovals;
	QuadtreeInsertion* Insertions;
	QuadtreeReinsertion* Reinsertions;

	uint32_t NodesUsed;
	uint32_t NodesSize;

	uint32_t NodeEntitiesUsed;
	uint32_t NodeEntitiesSize;

	uint32_t EntitiesUsed;
	uint32_t EntitiesSize;

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
QuadtreeNormalize(
	Quadtree* QT
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
QuadtreeCollideSlow(
	Quadtree* QT,
	QuadtreeCollideT Callback
	);


extern void
QuadtreeCollideFast(
	Quadtree* QT,
	QuadtreeCollideT Callback
	);
