#ifndef _quadtree_h_
#define _quadtree_h_ 1

#include <stdint.h>

typedef float QUADTREE_POSITION_T;

typedef struct QUADTREE_NODE
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
QUADTREE_NODE;

typedef struct QUADTREE_NODE_ENTITY
{
	uint32_t Next;
	uint32_t Entity;
}
QUADTREE_NODE_ENTITY;

typedef struct QUADTREE_ENTITY
{
	QUADTREE_POSITION_T X, Y, W, H;
	uint32_t QueryTick;
	uint8_t UpdateTick;
	uint8_t Changed;
	uint8_t Invalid;
}
QUADTREE_ENTITY;

typedef struct QUADTREE_NODE_INFO
{
	uint32_t NodeIdx;
	union
	{
		struct
		{
			QUADTREE_POSITION_T X, Y, W, H;
		};
		uint32_t* ParentIndexSlot;
	};
}
QUADTREE_NODE_INFO;

typedef struct QUADTREE_HT_ENTRY
{
	uint32_t Next;
	uint32_t Idx[2];
}
QUADTREE_HT_ENTRY;

typedef struct QUADTREE QUADTREE;

typedef void
(*QUADTREE_QUERY_T)(
	QUADTREE*,
	uint32_t
	);

typedef void
(*QUADTREE_NODE_QUERY_T)(
	QUADTREE*,
	const QUADTREE_NODE_INFO*
	);

typedef void
(*QUADTREE_COLLIDE_T)(
	QUADTREE*,
	uint32_t,
	uint32_t
	);

typedef int
(*QUADTREE_IS_COLLIDING_T)(
	const QUADTREE*,
	uint32_t,
	uint32_t
	);

typedef uint8_t
(*QUADTREE_UPDATE_T)(
	QUADTREE*,
	uint32_t
	);

/* How many entities can exist in a node without it splitting */
#define QUADTREE_SPLIT_THRESHOLD 6

/* You might want to increase this if you get a lot of collisions per tick */
#define QUADTREE_HASH_TABLE_FACTOR 1

#define QUADTREE_DEDUPE_COLLISIONS 1

/* Do not modify unless you know what you are doing. Use Quadtree.MinSize. */
#define QUADTREE_MAX_DEPTH 20

/* Do not modify */
#define QUADTREE_DFS_LENGTH (QUADTREE_MAX_DEPTH * 3 + 1)

struct QUADTREE
{
	QUADTREE_NODE* Nodes;
	QUADTREE_NODE_ENTITY* NodeEntities;
	QUADTREE_ENTITY* Entities;
	QUADTREE_HT_ENTRY* HTEntries;
	uint32_t* Removals;

	QUADTREE_QUERY_T Query;
	QUADTREE_NODE_QUERY_T NodeQuery;
	QUADTREE_COLLIDE_T Collide;
	QUADTREE_IS_COLLIDING_T IsColliding;
	QUADTREE_UPDATE_T Update;

	uint32_t* HashTable;
	uint32_t HashTableSize;

	uint32_t Idx;
	uint32_t QueryTick;
	uint8_t UpdateTick;
	uint32_t PostponeRemovals;

	QUADTREE_POSITION_T X, Y, W, H;

	QUADTREE_POSITION_T MinSize;

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
	QUADTREE*
	);

extern void
QuadtreeFree(
	QUADTREE*
	);

extern uint32_t
QuadtreeInsert(
	QUADTREE*,
	const QUADTREE_ENTITY*
	);

extern void
QuadtreeRemove(
	QUADTREE*,
	uint32_t
	);

extern void
QuadtreeUpdate(
	QUADTREE*
	);

extern void
QuadtreeQuery(
	QUADTREE*,
	QUADTREE_POSITION_T, QUADTREE_POSITION_T,
	QUADTREE_POSITION_T, QUADTREE_POSITION_T
	);

extern void
QuadtreeQueryNodes(
	QUADTREE*,
	QUADTREE_POSITION_T, QUADTREE_POSITION_T,
	QUADTREE_POSITION_T, QUADTREE_POSITION_T
	);

extern void
QuadtreeCollide(
	QUADTREE*
	);

#endif /* _quadtree_h_ */
