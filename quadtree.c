#include "quadtree.h"

#include <stdlib.h>


static void
QuadtreeInvalidateNode(
	QUADTREE_NODE* Node
	)
{
	Node->Head = -1;
}


static int
QuadtreeIsNodeInvalid(
	QUADTREE_NODE* Node
	)
{
	return Node->Head == -1;
}


static void
QuadtreeInvalidateNodeEntity(
	QUADTREE_NODE_ENTITY* NodeEntity
	)
{
	NodeEntity->Entity = 0;
}


static int
QuadtreeIsNodeEntityInvalid(
	QUADTREE_NODE_ENTITY* NodeEntity
	)
{
	return NodeEntity->Entity == 0;
}


static void
QuadtreeInvalidateEntity(
	QUADTREE_ENTITY* Entity
	)
{
	Entity->Invalid = 1;
}


static int
QuadtreeIsEntityInvalid(
	QUADTREE_ENTITY* Entity
	)
{
	return Entity->Invalid;
}


static void
QuadtreeInvalidateHTEntry(
	QUADTREE_HT_ENTRY* HTEntry
	)
{
	(void) HTEntry;
}


static int
QuadtreeIsHTEntryInvalid(
	QUADTREE_HT_ENTRY* HTEntry
	)
{
	return 0;
}


static void
QuadtreeInvalidateRemoval(
	uint32_t* Removal
	)
{
	(void) Removal;
}


static int
QuadtreeIsRemovalInvalid(
	uint32_t* Removal
	)
{
	return 0;
}


#define QUADTREE_DEF(name, names)										\
static void																\
QuadtreeResize##names (													\
	QUADTREE* Quadtree													\
	)																	\
{																		\
	if(Quadtree-> names##Used >= Quadtree-> names##Size )				\
	{																	\
		Quadtree-> names##Size =										\
			1 << (32 - __builtin_clz(Quadtree-> names##Used ));			\
		Quadtree-> names = realloc(										\
			Quadtree-> names ,											\
			sizeof(*Quadtree-> names) * Quadtree-> names##Size			\
			);															\
	}																	\
}																		\
																		\
static uint32_t															\
QuadtreeGet##name (														\
	QUADTREE* Quadtree													\
	)																	\
{																		\
	if(Quadtree-> Free##name )											\
	{																	\
		uint32_t Ret = Quadtree-> Free##name ;							\
		Quadtree-> Free##name = *(uint32_t*)&Quadtree-> names [Ret];	\
		return Ret;														\
	}																	\
																		\
	QuadtreeResize##names (Quadtree);									\
																		\
	return Quadtree-> names##Used ++;									\
}																		\
																		\
static void																\
QuadtreeRet##name (														\
	QUADTREE* Quadtree,													\
	uint32_t Idx														\
	)																	\
{																		\
	QuadtreeInvalidate##name (Quadtree-> names + Idx);					\
	*(uint32_t*)&Quadtree-> names [Idx] = Quadtree-> Free##name ;		\
	Quadtree-> Free##name = Idx;										\
}																		\
																		\
static void																\
QuadtreeReset##names (													\
	QUADTREE* Quadtree													\
	)																	\
{																		\
	Quadtree-> Free##name = 0;											\
	Quadtree-> names##Used = 1;											\
}																		\
																		\
static void																\
QuadtreeMaybeShrink##names (											\
	QUADTREE* Quadtree													\
	)																	\
{																		\
	if(Quadtree-> names##Used <= (Quadtree-> names##Size >> 2))			\
	{																	\
		uint32_t NewSize = Quadtree-> names##Size >> 1;					\
		void* Pointer = realloc(Quadtree-> names ,						\
			sizeof(*Quadtree-> names) * NewSize);						\
		if(Pointer)														\
		{																\
			Quadtree-> names = Pointer;									\
			Quadtree-> names##Size = NewSize;							\
		}																\
	}																	\
}

QUADTREE_DEF(Node, Nodes)
QUADTREE_DEF(NodeEntity, NodeEntities)
QUADTREE_DEF(Entity, Entities)
QUADTREE_DEF(HTEntry, HTEntries)
QUADTREE_DEF(Removal, Removals)

#undef QUADTREE_DEF


void
QuadtreeInit(
	QUADTREE* Quadtree
	)
{
#define QUADTREE_INIT(name, names)		\
	Quadtree-> names##Size = 1;			\
	QuadtreeReset##names (Quadtree);	\
	QuadtreeResize##names (Quadtree)

	QUADTREE_INIT(Node, Nodes);
	QUADTREE_INIT(NodeEntity, NodeEntities);
	QUADTREE_INIT(Entity, Entities);
	QUADTREE_INIT(HTEntry, HTEntries);
	QUADTREE_INIT(Removal, Removals);

#undef QUADTREE_INIT

	Quadtree->Nodes[0].Count = 0;
	Quadtree->Nodes[0].Head = 0;
	Quadtree->Nodes[0].PositionFlags = 0b1111; /* TRBL */
}


void
QuadtreeFree(
	QUADTREE* Quadtree
	)
{
#define QUADTREE_FREE(name, names)	\
	free(Quadtree-> names );		\
	Quadtree-> names = NULL;		\
	Quadtree-> names##Used = 0;		\
	Quadtree-> names##Size = 0;		\
	Quadtree-> Free##name = 0

	QUADTREE_FREE(Node, Nodes);
	QUADTREE_FREE(NodeEntity, NodeEntities);
	QUADTREE_FREE(Entity, Entities);
	QUADTREE_FREE(HTEntry, HTEntries);
	QUADTREE_FREE(Removal, Removals);

#undef QUADTREE_FREE
}


static int
QuadtreeIntersects(
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
QuadtreeIsInside(
	QUADTREE_POSITION_T x1, QUADTREE_POSITION_T y1,
	QUADTREE_POSITION_T w1, QUADTREE_POSITION_T h1,

	QUADTREE_POSITION_T x2, QUADTREE_POSITION_T y2,
	QUADTREE_POSITION_T w2, QUADTREE_POSITION_T h2
	)
{
	return
		x1 - w1 > x2 - w2 &&
		y1 - h1 > y2 - h2 &&
		x2 + w2 > x1 + w1 &&
		y2 + h2 > y1 + h1;
}


static void
QuadtreePostponeRemovals(
	QUADTREE* Quadtree
	)
{
	++Quadtree->PostponeRemovals;
}


static void
QuadtreeDoRemovals(
	QUADTREE* Quadtree
	)
{
	if(--Quadtree->PostponeRemovals != 0)
	{
		return;
	}

	for(uint32_t i = 1; i < Quadtree->RemovalsUsed; ++i)
	{
		QuadtreeRemove(Quadtree, Quadtree->Removals[i]);
	}

	QuadtreeResetRemovals(Quadtree);
}


static void
QuadtreeReinsertInternal(
	QUADTREE* Quadtree,
	QUADTREE_NODE* Node
	)
{
	uint32_t Idx = QuadtreeGetNodeEntity(Quadtree);
	QUADTREE_NODE_ENTITY* NodeEntity = Quadtree->NodeEntities + Idx;
	NodeEntity->Entity = Quadtree->Idx;
	NodeEntity->Next = Node->Head;
	Node->Head = Idx;
	++Node->Count;
}


static void
QuadtreeMaybeReinsertInternal(
	QUADTREE* Quadtree,
	QUADTREE_NODE* Node,
	QUADTREE_POSITION_T X, QUADTREE_POSITION_T Y,
	QUADTREE_POSITION_T W, QUADTREE_POSITION_T H
	)
{
	const QUADTREE_ENTITY* Entity = Quadtree->Entities + Quadtree->Idx;

	if(Entity->X - Entity->W <= X)
	{
		if(Entity->Y - Entity->H <= Y)
		{
			QuadtreeReinsertInternal(Quadtree, Quadtree->Nodes + Node->Heads[0]);
		}
		if(Entity->Y + Entity->H >= Y)
		{
			QuadtreeReinsertInternal(Quadtree, Quadtree->Nodes + Node->Heads[1]);
		}
	}
	if(Entity->X + Entity->W >= X)
	{
		if(Entity->Y - Entity->H <= Y)
		{
			QuadtreeReinsertInternal(Quadtree, Quadtree->Nodes + Node->Heads[2]);
		}
		if(Entity->Y + Entity->H >= Y)
		{
			QuadtreeReinsertInternal(Quadtree, Quadtree->Nodes + Node->Heads[3]);
		}
	}
}


static void
QuadtreeSplit(
	QUADTREE* Quadtree,
	uint32_t NodeIdx,
	QUADTREE_POSITION_T X, QUADTREE_POSITION_T Y,
	QUADTREE_POSITION_T W, QUADTREE_POSITION_T H
	)
{
	uint32_t Idx[4];
	QUADTREE_NODE* Nodes[4];
	for(int i = 0; i < 4; ++i)
	{
		Idx[i] = QuadtreeGetNode(Quadtree);
	}
	for(int i = 0; i < 4; ++i)
	{
		Nodes[i] = Quadtree->Nodes + Idx[i];
	}

	QUADTREE_NODE* Node = Quadtree->Nodes + NodeIdx;
	uint32_t NodeEntityIdx = Node->Head;

	for(int i = 0; i < 4; ++i)
	{
		Nodes[i]->Count = 0;
		Nodes[i]->Head = 0;
	}
	Nodes[0]->PositionFlags = Node->PositionFlags & 0b0011;
	Nodes[1]->PositionFlags = Node->PositionFlags & 0b1001;
	Nodes[2]->PositionFlags = Node->PositionFlags & 0b0110;
	Nodes[3]->PositionFlags = Node->PositionFlags & 0b1100;
	for(int i = 0; i < 4; ++i)
	{
		Node->Heads[i] = Idx[i];
	}

	uint32_t IdxSave = Quadtree->Idx;
	for(int i = 0; i < Node->Count; ++i)
	{
		QUADTREE_NODE_ENTITY* NodeEntity = Quadtree->NodeEntities + NodeEntityIdx;
		Quadtree->Idx = NodeEntity->Entity;
		uint32_t NextIdx = NodeEntity->Next;
		QuadtreeRetNodeEntity(Quadtree, NodeEntityIdx);
		QuadtreeMaybeReinsertInternal(Quadtree, Node, X, Y, W, H);
		NodeEntityIdx = NextIdx;
	}
	Quadtree->Idx = IdxSave;

	Node->Count = -1;
}


#define QUADTREE_DESCEND(_X, _Y, _W, _H)									\
do																			\
{																			\
	QUADTREE_POSITION_T InfoW = Info.W * 0.5f;								\
	QUADTREE_POSITION_T InfoH = Info.H * 0.5f;								\
	if(_X - _W <= Info.X)													\
	{																		\
		if(_Y - _H <= Info.Y)												\
			NodeInfos[NodeInfosUsed++] =									\
			(QUADTREE_NODE_INFO)											\
			{																\
				.NodeIdx = Node->Heads[0],									\
				.X = Info.X - InfoW,										\
				.Y = Info.Y - InfoH,										\
				.W = InfoW,													\
				.H = InfoH													\
			};																\
		if(_Y + _H >= Info.Y)												\
			NodeInfos[NodeInfosUsed++] =									\
			(QUADTREE_NODE_INFO)											\
			{																\
				.NodeIdx = Node->Heads[1],									\
				.X = Info.X - InfoW,										\
				.Y = Info.Y + InfoH,										\
				.W = InfoW,													\
				.H = InfoH													\
			};																\
	}																		\
	if(_X + _W >= Info.X)													\
	{																		\
		if(_Y - _H <= Info.Y)												\
			NodeInfos[NodeInfosUsed++] =									\
			(QUADTREE_NODE_INFO)											\
			{																\
				.NodeIdx = Node->Heads[2],									\
				.X = Info.X + InfoW,										\
				.Y = Info.Y - InfoH,										\
				.W = InfoW,													\
				.H = InfoH													\
			};																\
		if(_Y + _H >= Info.Y)												\
			NodeInfos[NodeInfosUsed++] =									\
			(QUADTREE_NODE_INFO)											\
			{																\
				.NodeIdx = Node->Heads[3],									\
				.X = Info.X + InfoW,										\
				.Y = Info.Y + InfoH,										\
				.W = InfoW,													\
				.H = InfoH													\
			};																\
	}																		\
}																			\
while(0)


static void
QuadtreeReinsert(
	QUADTREE* Quadtree,
	uint32_t EntityIdx
	)
{
	const QUADTREE_ENTITY* Entity = Quadtree->Entities + EntityIdx;
	Quadtree->Idx = EntityIdx;

	uint32_t NodeInfosUsed = 1;
	QUADTREE_NODE_INFO NodeInfos[QUADTREE_DFS_LENGTH];

	NodeInfos[0] =
	(QUADTREE_NODE_INFO)
	{
		.NodeIdx = 0,
		.X = Quadtree->X,
		.Y = Quadtree->Y,
		.W = Quadtree->W,
		.H = Quadtree->H
	};

	while(NodeInfosUsed)
	{
		QUADTREE_NODE_INFO Info = NodeInfos[--NodeInfosUsed];
		goto_insert:;
		QUADTREE_NODE* Node = Quadtree->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QUADTREE_DESCEND(Entity->X, Entity->Y, Entity->W, Entity->H);
			continue;
		}

		if(!Node->Head)
		{
			QuadtreeReinsertInternal(Quadtree, Node);
			continue;
		}

		const QUADTREE_NODE_ENTITY* NodeEntity = Quadtree->NodeEntities + Node->Head;

		while(1)
		{
			if(NodeEntity->Entity == EntityIdx)
			{
				goto goto_skip;
			}
			if(!NodeEntity->Next)
			{
				break;
			}
			NodeEntity = Quadtree->NodeEntities + NodeEntity->Next;
		}

		if(
			Node->Count < QUADTREE_SPLIT_THRESHOLD ||
			Info.W <= Quadtree->MinSize || Info.H <= Quadtree->MinSize
			)
		{
			QuadtreeReinsertInternal(Quadtree, Node);
		}
		else
		{
			QuadtreeSplit(Quadtree, Info.NodeIdx, Info.X, Info.Y, Info.W, Info.H);

			goto goto_insert;
		}

		goto_skip:;
	}
}


uint32_t
QuadtreeInsert(
	QUADTREE* Quadtree,
	const QUADTREE_ENTITY* Entity
	)
{
	const uint32_t Idx = QuadtreeGetEntity(Quadtree);
	QUADTREE_ENTITY* QTEntity = Quadtree->Entities + Idx;
	QTEntity->X = Entity->X;
	QTEntity->Y = Entity->Y;
	QTEntity->W = Entity->W;
	QTEntity->H = Entity->H;
	QTEntity->QueryTick = Quadtree->QueryTick;
	QTEntity->UpdateTick = Quadtree->UpdateTick;
	QTEntity->Changed = 0;
	QTEntity->Invalid = 0;

	Quadtree->Idx = Idx;

	uint32_t NodeInfosUsed = 1;
	QUADTREE_NODE_INFO NodeInfos[QUADTREE_DFS_LENGTH];

	NodeInfos[0] =
	(QUADTREE_NODE_INFO)
	{
		.NodeIdx = 0,
		.X = Quadtree->X,
		.Y = Quadtree->Y,
		.W = Quadtree->W,
		.H = Quadtree->H
	};

	while(NodeInfosUsed)
	{
		QUADTREE_NODE_INFO Info = NodeInfos[--NodeInfosUsed];
		goto_insert:;
		QUADTREE_NODE* Node = Quadtree->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QUADTREE_DESCEND(Entity->X, Entity->Y, Entity->W, Entity->H);
			continue;
		}

		if(
			Node->Count < QUADTREE_SPLIT_THRESHOLD ||
			Info.W <= Quadtree->MinSize || Info.H <= Quadtree->MinSize
			)
		{
			QuadtreeReinsertInternal(Quadtree, Node);
		}
		else
		{
			QuadtreeSplit(Quadtree, Info.NodeIdx, Info.X, Info.Y, Info.W, Info.H);

			goto goto_insert;
		}
	}

	return Idx;
}


void
QuadtreeRemove(
	QUADTREE* Quadtree,
	uint32_t EntityIdx
	)
{
	QUADTREE_ENTITY* Entity = Quadtree->Entities + EntityIdx;

	if(Quadtree->PostponeRemovals)
	{
		uint32_t Idx = QuadtreeGetRemoval(Quadtree);
		Quadtree->Removals[Idx] = EntityIdx;
		return;
	}

	uint32_t NodeInfosUsed = 1;
	QUADTREE_NODE_INFO NodeInfos[QUADTREE_DFS_LENGTH];

	NodeInfos[0] =
	(QUADTREE_NODE_INFO)
	{
		.NodeIdx = 0,
		.X = Quadtree->X,
		.Y = Quadtree->Y,
		.W = Quadtree->W,
		.H = Quadtree->H
	};

	while(NodeInfosUsed)
	{
		QUADTREE_NODE_INFO Info = NodeInfos[--NodeInfosUsed];
		QUADTREE_NODE* Node = Quadtree->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QUADTREE_DESCEND(Entity->X, Entity->Y, Entity->W, Entity->H);
			continue;
		}

		uint32_t Idx = Node->Head;
		QUADTREE_NODE_ENTITY* PrevNodeEntity = NULL;

		while(1)
		{
			QUADTREE_NODE_ENTITY* NodeEntity = Quadtree->NodeEntities + Idx;
			if(NodeEntity->Entity == EntityIdx)
			{
				if(PrevNodeEntity)
				{
					PrevNodeEntity->Next = NodeEntity->Next;
				}
				else
				{
					Node->Head = NodeEntity->Next;
				}
				--Node->Count;
				QuadtreeRetNodeEntity(Quadtree, Idx);
				break;
			}
			PrevNodeEntity = NodeEntity;
			Idx = NodeEntity->Next;
		}
	}

	QuadtreeRetEntity(Quadtree, EntityIdx);
}


void
QuadtreeUpdate(
	QUADTREE* Quadtree
	)
{
	Quadtree->UpdateTick ^= 1;
	QuadtreePostponeRemovals(Quadtree);

	uint32_t NodeInfosUsed = 1;
	QUADTREE_NODE_INFO NodeInfos[QUADTREE_DFS_LENGTH];

	NodeInfos[0] =
	(QUADTREE_NODE_INFO)
	{
		.NodeIdx = 0,
		.X = Quadtree->X,
		.Y = Quadtree->Y,
		.W = Quadtree->W,
		.H = Quadtree->H
	};

	while(NodeInfosUsed)
	{
		QUADTREE_NODE_INFO Info = NodeInfos[--NodeInfosUsed];
		QUADTREE_NODE* Node = Quadtree->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QUADTREE_DESCEND(Info.X, Info.Y, 0, 0);
			continue;
		}

		if(!Node->Head)
		{
			continue;
		}

		uint32_t Idx = Node->Head;
		uint32_t PrevIdx = 0;
		while(1)
		{
			QUADTREE_NODE_ENTITY* NodeEntity = Quadtree->NodeEntities + Idx;
			uint32_t EntityIdx = NodeEntity->Entity;
			QUADTREE_ENTITY* Entity = Quadtree->Entities + EntityIdx;

			if(Entity->UpdateTick != Quadtree->UpdateTick)
			{
				Entity->UpdateTick = Quadtree->UpdateTick;
				Entity->Changed = Quadtree->Update(Quadtree, EntityIdx);

				Node = Quadtree->Nodes + Info.NodeIdx;
				NodeEntity = Quadtree->NodeEntities + Idx;
				Entity = Quadtree->Entities + EntityIdx;

				if(Entity->Changed &&
					!QuadtreeIsInside(
						Entity->X, Entity->Y, Entity->W, Entity->H,
						Info.X, Info.Y, Info.W, Info.H
					))
				{
					QuadtreeReinsert(Quadtree, EntityIdx);
					Node = Quadtree->Nodes + Info.NodeIdx;
					NodeEntity = Quadtree->NodeEntities + Idx;
				}
			}

			if(
				(Entity->X + Entity->W < Info.X - Info.W && !(Node->PositionFlags & 0b0001)) ||
				(Entity->Y + Entity->H < Info.Y - Info.H && !(Node->PositionFlags & 0b0010)) ||
				(Info.X + Info.W < Entity->X - Entity->W && !(Node->PositionFlags & 0b0100)) ||
				(Info.Y + Info.H < Entity->Y - Entity->H && !(Node->PositionFlags & 0b1000))
				)
			{
				if(PrevIdx)
				{
					Quadtree->NodeEntities[PrevIdx].Next = NodeEntity->Next;
				}
				else
				{
					Node->Head = NodeEntity->Next;
				}
				--Node->Count;
				uint32_t NextIdx = NodeEntity->Next;
				QuadtreeRetNodeEntity(Quadtree, Idx);
				Idx = NextIdx;
				if(!Idx)
				{
					break;
				}
				continue;
			}

			if(!NodeEntity->Next)
			{
				break;
			}
			PrevIdx = Idx;
			Idx = NodeEntity->Next;
		}
	}

	QuadtreeDoRemovals(Quadtree);

	NodeInfosUsed = 1;
	NodeInfos[0].NodeIdx = 0;

	while(NodeInfosUsed)
	{
		QUADTREE_NODE_INFO Info = NodeInfos[--NodeInfosUsed];
		QUADTREE_NODE* Node = Quadtree->Nodes + Info.NodeIdx;

		if(Node->Count != -1)
		{
			continue;
		}

		QUADTREE_NODE* Children[4];
		for(int i = 0; i < 4; ++i)
		{
			Children[i] = Quadtree->Nodes + Node->Heads[i];
		}
		uint32_t Total = 0;
		int Possible = 1;

		for(int i = 0; i < 4; ++i)
		{
			if(Children[i]->Count == -1)
			{
				NodeInfos[NodeInfosUsed++].NodeIdx = Node->Heads[i];
				Possible = 0;
			}
			else
			{
				Total += Children[i]->Count;
			}
		}

		if(!Possible || Total > QUADTREE_SPLIT_THRESHOLD)
		{
			continue;
		}

		uint32_t Heads[4];
		for(int i = 0; i < 4; ++i)
		{
			Heads[i] = Node->Heads[i];
		}
		Node->Head = Children[0]->Head;
		Node->Count = Children[0]->Count;
		Node->PositionFlags = Children[0]->PositionFlags;

		for(int i = 1; i < 4; ++i)
		{
			Node->PositionFlags |= Children[i]->PositionFlags;
			uint32_t Idx = Children[i]->Head;
			while(Idx)
			{
				QUADTREE_NODE_ENTITY* NodeEntity = Quadtree->NodeEntities + Idx;

				if(!Node->Head)
				{
					Node->Head = Idx;
					Node->Count = 1;
					Idx = NodeEntity->Next;
					NodeEntity->Next = 0;
				}
				else
				{
					uint32_t Idx2 = Node->Head;
					while(1)
					{
						QUADTREE_NODE_ENTITY* NodeEntity2 = Quadtree->NodeEntities + Idx2;

						if(NodeEntity->Entity == NodeEntity2->Entity)
						{
							uint32_t SaveIdx = Idx;
							Idx = NodeEntity->Next;
							QuadtreeRetNodeEntity(Quadtree, SaveIdx);
							break;
						}

						if(!NodeEntity2->Next)
						{
							uint32_t SaveIdx = Idx;
							Idx = NodeEntity->Next;

							NodeEntity2->Next = SaveIdx;
							NodeEntity->Next = 0;

							++Node->Count;
							break;
						}
						Idx2 = NodeEntity2->Next;
					}
				}
			}
		}

		for(int i = 0; i < 4; ++i)
		{
			QuadtreeRetNode(Quadtree, Heads[i]);
		}
	}

	QUADTREE_NODE_ENTITY* NodeEntities = malloc(sizeof(*NodeEntities) * Quadtree->NodeEntitiesSize);
	uint32_t NodeEntityIdx = 1;

	QUADTREE_NODE* Nodes = malloc(sizeof(*Nodes) * Quadtree->NodesSize);
	uint32_t NodeIdx = 0;

	NodeInfosUsed = 1;
	NodeInfos[0] =
	(QUADTREE_NODE_INFO)
	{
		.NodeIdx = 0,
		.ParentIndexSlot = NULL
	};

	while(NodeInfosUsed)
	{
		QUADTREE_NODE_INFO Info = NodeInfos[--NodeInfosUsed];
		QUADTREE_NODE* Node = Quadtree->Nodes + Info.NodeIdx;
		uint32_t NewNodeIdx = NodeIdx++;
		QUADTREE_NODE* NewNode = Nodes + NewNodeIdx;

		if(Info.ParentIndexSlot)
		{
			*Info.ParentIndexSlot = NewNodeIdx;
		}

		if(Node->Count == -1)
		{
			for(int i = 0; i < 4; ++i)
			{
				NodeInfos[NodeInfosUsed++] =
				(QUADTREE_NODE_INFO)
				{
					.NodeIdx = Node->Heads[i],
					.ParentIndexSlot = &NewNode->Heads[i]
				};
			}
			NewNode->Count = -1;
			continue;
		}

		NewNode->PositionFlags = Node->PositionFlags;

		if(!Node->Head)
		{
			NewNode->Head = 0;
			NewNode->Count = 0;

			continue;
		}

		QUADTREE_NODE_ENTITY* NodeEntity = Quadtree->NodeEntities + Node->Head;
		NewNode->Head = NodeEntityIdx;
		NewNode->Count = Node->Count;

		while(1)
		{
			QUADTREE_NODE_ENTITY* NewNodeEntity = NodeEntities + NodeEntityIdx;

			NewNodeEntity->Entity = NodeEntity->Entity;
			++NodeEntityIdx;

			if(NodeEntity->Next)
			{
				NodeEntity = Quadtree->NodeEntities + NodeEntity->Next;
				NewNodeEntity->Next = NodeEntityIdx;
			}
			else
			{
				NewNodeEntity->Next = 0;
				break;
			}
		}
	}

	free(Quadtree->Nodes);
	Quadtree->Nodes = Nodes;
	Quadtree->NodesUsed = NodeIdx;
	Quadtree->FreeNode = 0;

	free(Quadtree->NodeEntities);
	Quadtree->NodeEntities = NodeEntities;
	Quadtree->NodeEntitiesUsed = NodeEntityIdx;
	Quadtree->FreeNodeEntity = 0;

	QuadtreeMaybeShrinkNodes(Quadtree);
	QuadtreeMaybeShrinkNodeEntities(Quadtree);
	QuadtreeMaybeShrinkEntities(Quadtree);
	QuadtreeMaybeShrinkHTEntries(Quadtree);
	QuadtreeMaybeShrinkRemovals(Quadtree);
}


void
QuadtreeQuery(
	QUADTREE* Quadtree,
	QUADTREE_POSITION_T X, QUADTREE_POSITION_T Y,
	QUADTREE_POSITION_T W, QUADTREE_POSITION_T H
	)
{
	++Quadtree->QueryTick;
	QuadtreePostponeRemovals(Quadtree);

	uint32_t NodeInfosUsed = 1;
	QUADTREE_NODE_INFO NodeInfos[QUADTREE_DFS_LENGTH];

	NodeInfos[0] =
	(QUADTREE_NODE_INFO)
	{
		.NodeIdx = 0,
		.X = Quadtree->X,
		.Y = Quadtree->Y,
		.W = Quadtree->W,
		.H = Quadtree->H
	};

	while(NodeInfosUsed)
	{
		QUADTREE_NODE_INFO Info = NodeInfos[--NodeInfosUsed];
		QUADTREE_NODE* Node = Quadtree->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QUADTREE_DESCEND(X, Y, W, H);
			continue;
		}

		if(!Node->Head)
		{
			continue;
		}

		uint32_t Idx = Node->Head;
		QUADTREE_NODE_ENTITY* NodeEntity = Quadtree->NodeEntities + Idx;
		while(1)
		{
			QUADTREE_ENTITY* Entity = Quadtree->Entities + NodeEntity->Entity;
			if(Entity->QueryTick != Quadtree->QueryTick)
			{
				Entity->QueryTick = Quadtree->QueryTick;
				if(QuadtreeIntersects(
					Entity->X, Entity->Y, Entity->W, Entity->H,
					X, Y, W, H
					))
				{
					Quadtree->Query(Quadtree, NodeEntity->Entity);
				}
			}
			if(!NodeEntity->Next)
			{
				break;
			}
			NodeEntity = Quadtree->NodeEntities + NodeEntity->Next;
		}
	}

	QuadtreeDoRemovals(Quadtree);
}


void
QuadtreeQueryNodes(
	QUADTREE* Quadtree,
	QUADTREE_POSITION_T X, QUADTREE_POSITION_T Y,
	QUADTREE_POSITION_T W, QUADTREE_POSITION_T H
	)
{
	uint32_t NodeInfosUsed = 1;
	QUADTREE_NODE_INFO NodeInfos[QUADTREE_DFS_LENGTH];

	NodeInfos[0] =
	(QUADTREE_NODE_INFO)
	{
		.NodeIdx = 0,
		.X = Quadtree->X,
		.Y = Quadtree->Y,
		.W = Quadtree->W,
		.H = Quadtree->H
	};

	while(NodeInfosUsed)
	{
		QUADTREE_NODE_INFO Info = NodeInfos[--NodeInfosUsed];
		QUADTREE_NODE* Node = Quadtree->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QUADTREE_DESCEND(X, Y, W, H);
			continue;
		}

		Quadtree->NodeQuery(Quadtree, &Info);
	}
}


static int
QuadtreeHash(
	QUADTREE* Quadtree,
	uint32_t IndexA,
	uint32_t IndexB
	)
{
	if(IndexA > IndexB)
	{
		uint32_t Temp = IndexA;
		IndexA = IndexB;
		IndexB = Temp;
	}

	uint32_t Hash = IndexA * 48611 + IndexB * 50261;
	Hash %= Quadtree->HashTableSize;

	uint32_t Index = Quadtree->HashTable[Hash];
	if(!Index)
	{
		goto goto_put;
	}

	QUADTREE_HT_ENTRY* Entry = Quadtree->HTEntries + Index;
	while(1)
	{
		if(Entry->Idx[0] == IndexA && Entry->Idx[1] == IndexB)
		{
			return 1; /* Hash exists */
		}

		if(!Entry->Next)
		{
			goto goto_put;
		}
		Entry = Quadtree->HTEntries + Entry->Next;
	}

	goto_put:;

	uint32_t EntryIdx = QuadtreeGetHTEntry(Quadtree);
	Entry = Quadtree->HTEntries + EntryIdx;
	Entry->Idx[0] = IndexA;
	Entry->Idx[1] = IndexB;
	Entry->Next = Index;
	Quadtree->HashTable[Hash] = EntryIdx;

	return 0;
}


void
QuadtreeCollide(
	QUADTREE* Quadtree
	)
{
	QUADTREE_NODE* Node = Quadtree->Nodes;
	QUADTREE_NODE* EndNode = Quadtree->Nodes + Quadtree->NodesUsed;

#if QUADTREE_DEDUPE_COLLISIONS == 1
	Quadtree->HashTableSize = Quadtree->EntitiesUsed * QUADTREE_HASH_TABLE_FACTOR;
	Quadtree->HashTable = calloc(Quadtree->HashTableSize, sizeof(*Quadtree->HashTable));
	QuadtreeResetHTEntries(Quadtree);
#endif

	while(Node != EndNode)
	{
		if(Node->Count == -1 || !Node->Head || QuadtreeIsNodeInvalid(Node))
		{
			++Node;
			continue;
		}

		QUADTREE_NODE_ENTITY* NodeEntity = Quadtree->NodeEntities + Node->Head;
		QUADTREE_NODE_ENTITY* OtherNodeEntitySave = Quadtree->NodeEntities + Node->Head;
		++Node;

		while(1)
		{
			QUADTREE_NODE_ENTITY* OtherNodeEntity = OtherNodeEntitySave;

			while(1)
			{
				if(
					NodeEntity->Entity != OtherNodeEntity->Entity &&
					Quadtree->IsColliding(Quadtree, NodeEntity->Entity, OtherNodeEntity->Entity)
#if QUADTREE_DEDUPE_COLLISIONS == 1
					&& !QuadtreeHash(Quadtree, NodeEntity->Entity, OtherNodeEntity->Entity)
#endif
					)
				{
					Quadtree->Collide(Quadtree, NodeEntity->Entity, OtherNodeEntity->Entity);
				}

				if(!OtherNodeEntity->Next)
				{
					break;
				}
				OtherNodeEntity = Quadtree->NodeEntities + OtherNodeEntity->Next;
			}

			if(!NodeEntity->Next)
			{
				break;
			}
			NodeEntity = Quadtree->NodeEntities + NodeEntity->Next;
		}
	}

#if QUADTREE_DEDUPE_COLLISIONS == 1
	free(Quadtree->HashTable);
#endif
}


#undef QUADTREE_DESCEND

