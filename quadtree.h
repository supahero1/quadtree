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


#ifndef QUADTREE_ENTITY_DATA_TYPE


	typedef struct QuadtreeEntityData
	{
		uint8_t _Reserved;
	}
	QuadtreeEntityData;


	#define QUADTREE_ENTITY_DATA_TYPE QuadtreeEntityData
#endif


typedef struct QuadtreeEntity
{
	QuadtreeRectExtent Extent;

	QUADTREE_ENTITY_DATA_TYPE Data;

	uint32_t QueryTick;
	uint8_t UpdateTick;
	uint8_t Invalid;
}
QuadtreeEntity;


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


typedef struct Quadtree Quadtree;


typedef void
(*QuadtreeQueryT)(
	Quadtree* QT,
	QuadtreeEntity* Entity
	);


typedef void
(*QuadtreeNodeQueryT)(
	Quadtree* QT,
	const QuadtreeNodeInfo* Info
	);


typedef void
(*QuadtreeCollideT)(
	const Quadtree* QT,
	QuadtreeEntity* EntityA,
	QuadtreeEntity* EntityB
	);


typedef bool
(*QuadtreeUpdateT)(
	Quadtree* QT,
	QuadtreeEntity* Entity
	);


struct Quadtree
{
	QuadtreeNode* Nodes;
	QuadtreeNodeEntity* NodeEntities;
	QuadtreeEntity* Entities;
	QuadtreeHTEntry* HTEntries;
	uint32_t* Removals;

	uint32_t* HashTable;
	uint32_t HashTableSize;

	uint32_t Idx;
	uint32_t QueryTick;
	uint8_t UpdateTick;
	uint32_t PostponeRemovals;

	QuadtreeRectExtent RectExtent;
	QuadtreeHalfExtent HalfExtent;

	QuadtreePosition MinSize;

	uint32_t FreeNode;
	uint32_t NodesUsed;
	uint32_t NodesSize;

	uint32_t FreeNodeEntity;
	uint32_t NodeEntitiesUsed;
	uint32_t NodeEntitiesSize;

	uint32_t FreeEntity;
	uint32_t EntitiesUsed;
	uint32_t EntitiesSize;

	uint32_t FreeHTEntry;
	uint32_t HTEntriesUsed;
	uint32_t HTEntriesSize;

	uint32_t FreeRemoval;
	uint32_t RemovalsUsed;
	uint32_t RemovalsSize;
};


extern void
QuadtreeInit(
	Quadtree* QT
	);


extern void
QuadtreeFree(
	Quadtree* QT
	);


extern QUADTREE_ENTITY_DATA_TYPE*
QuadtreeInsert(
	Quadtree* QT,
	const QuadtreeRectExtent* Extent
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
