#include "quadtree.h"

#include <stdlib.h>


static void
QuadtreeInvalidateNode(
	QuadtreeNode* Node
	)
{
	Node->Head = -1;
}


static bool
QuadtreeIsNodeInvalid(
	QuadtreeNode* Node
	)
{
	return Node->Head == -1;
}


static void
QuadtreeInvalidateNodeEntity(
	QuadtreeNodeEntity* NodeEntity
	)
{
	NodeEntity->Entity = 0;
}


static bool
QuadtreeIsNodeEntityInvalid(
	QuadtreeNodeEntity* NodeEntity
	)
{
	return NodeEntity->Entity == 0;
}


static void
QuadtreeInvalidateEntity(
	QuadtreeEntity* Entity
	)
{
	Entity->Invalid = 1;
}


static bool
QuadtreeIsEntityInvalid(
	QuadtreeEntity* Entity
	)
{
	return Entity->Invalid;
}


static void
QuadtreeInvalidateHTEntry(
	QuadtreeHTEntry* HTEntry
	)
{
	(void) HTEntry;
}


static bool
QuadtreeIsHTEntryInvalid(
	QuadtreeHTEntry* HTEntry
	)
{
	return false;
}


static void
QuadtreeInvalidateRemoval(
	uint32_t* Removal
	)
{
	(void) Removal;
}


static bool
QuadtreeIsRemovalInvalid(
	uint32_t* Removal
	)
{
	return false;
}


#define Quadtree_DEF(Name, Names)										\
static void																\
QuadtreeResize##Names (													\
	Quadtree* QT														\
	)																	\
{																		\
	if(QT-> Names##Used >= QT-> Names##Size )							\
	{																	\
		QT-> Names##Size =												\
			1 << (32 - __builtin_clz(QT-> Names##Used ));				\
		QT-> Names = realloc(											\
			QT-> Names ,												\
			sizeof(*QT-> Names) * QT-> Names##Size						\
			);															\
	}																	\
}																		\
																		\
static uint32_t															\
QuadtreeGet##Name (														\
	Quadtree* QT														\
	)																	\
{																		\
	if(QT-> Free##Name )												\
	{																	\
		uint32_t Ret = QT-> Free##Name ;								\
		QT-> Free##Name = *(uint32_t*)&QT-> Names [Ret];				\
		return Ret;														\
	}																	\
																		\
	QuadtreeResize##Names (QT);											\
																		\
	return QT-> Names##Used ++;											\
}																		\
																		\
static void																\
QuadtreeRet##Name (														\
	Quadtree* QT,														\
	uint32_t Idx														\
	)																	\
{																		\
	QuadtreeInvalidate##Name (QT-> Names + Idx);						\
	*(uint32_t*)&QT-> Names [Idx] = QT-> Free##Name ;					\
	QT-> Free##Name = Idx;												\
}																		\
																		\
static void																\
QuadtreeReset##Names (													\
	Quadtree* QT														\
	)																	\
{																		\
	QT-> Free##Name = 0;												\
	QT-> Names##Used = 1;												\
}																		\
																		\
static void																\
QuadtreeMaybeShrink##Names (											\
	Quadtree* QT														\
	)																	\
{																		\
	if(QT-> Names##Used <= (QT-> Names##Size >> 2))						\
	{																	\
		uint32_t NewSize = QT-> Names##Size >> 1;						\
		void* Pointer = realloc(QT-> Names ,							\
			sizeof(*QT-> Names) * NewSize);								\
		if(Pointer)														\
		{																\
			QT-> Names = Pointer;										\
			QT-> Names##Size = NewSize;									\
		}																\
	}																	\
}

Quadtree_DEF(Node, Nodes)
Quadtree_DEF(NodeEntity, NodeEntities)
Quadtree_DEF(Entity, Entities)
Quadtree_DEF(HTEntry, HTEntries)
Quadtree_DEF(Removal, Removals)

#undef Quadtree_DEF


void
QuadtreeInit(
	Quadtree* QT
	)
{
#define Quadtree_INIT(Name, Names)		\
	QT-> Names##Size = 1;			\
	QuadtreeReset##Names (QT);	\
	QuadtreeResize##Names (QT)

	Quadtree_INIT(Node, Nodes);
	Quadtree_INIT(NodeEntity, NodeEntities);
	Quadtree_INIT(Entity, Entities);
	Quadtree_INIT(HTEntry, HTEntries);
	Quadtree_INIT(Removal, Removals);

#undef Quadtree_INIT

	QT->Nodes[0].Count = 0;
	QT->Nodes[0].Head = 0;
	QT->Nodes[0].PositionFlags = 0b1111; /* TRBL */
}


void
QuadtreeFree(
	Quadtree* QT
	)
{
#define Quadtree_FREE(Name, Names)	\
	free(QT-> Names );		\
	QT-> Names = NULL;		\
	QT-> Names##Used = 0;		\
	QT-> Names##Size = 0;		\
	QT-> Free##Name = 0

	Quadtree_FREE(Node, Nodes);
	Quadtree_FREE(NodeEntity, NodeEntities);
	Quadtree_FREE(Entity, Entities);
	Quadtree_FREE(HTEntry, HTEntries);
	Quadtree_FREE(Removal, Removals);

#undef Quadtree_FREE
}


static void
QuadtreePostponeRemovals(
	Quadtree* QT
	)
{
	++QT->PostponeRemovals;
}


static void
QuadtreeDoRemovals(
	Quadtree* QT
	)
{
	if(--QT->PostponeRemovals != 0)
	{
		return;
	}

	for(uint32_t i = 1; i < QT->RemovalsUsed; ++i)
	{
		QuadtreeRemove(QT, QT->Removals[i]);
	}

	QuadtreeResetRemovals(QT);
}


static void
QuadtreeReinsertInternal(
	Quadtree* QT,
	QuadtreeNode* Node
	)
{
	uint32_t Idx = QuadtreeGetNodeEntity(QT);
	QuadtreeNodeEntity* NodeEntity = QT->NodeEntities + Idx;
	NodeEntity->Entity = QT->Idx;
	NodeEntity->Next = Node->Head;
	Node->Head = Idx;
	++Node->Count;
}


static void
QuadtreeMaybeReinsertInternal(
	Quadtree* QT,
	QuadtreeNode* Node,
	QuadtreeHalfExtent NodeExtent
	)
{
	const QuadtreeEntity* Entity = QT->Entities + QT->Idx;

	if(Entity->Extent.MinX <= NodeExtent.X)
	{
		if(Entity->Extent.MinY <= NodeExtent.Y)
		{
			QuadtreeReinsertInternal(QT, QT->Nodes + Node->Heads[0]);
		}
		if(Entity->Extent.MaxY >= NodeExtent.Y)
		{
			QuadtreeReinsertInternal(QT, QT->Nodes + Node->Heads[1]);
		}
	}
	if(Entity->Extent.MaxX >= NodeExtent.X)
	{
		if(Entity->Extent.MinY <= NodeExtent.Y)
		{
			QuadtreeReinsertInternal(QT, QT->Nodes + Node->Heads[2]);
		}
		if(Entity->Extent.MaxY >= NodeExtent.Y)
		{
			QuadtreeReinsertInternal(QT, QT->Nodes + Node->Heads[3]);
		}
	}
}


static void
QuadtreeSplit(
	Quadtree* Quadtree,
	QuadtreeNodeInfo Info
	)
{
	uint32_t Idx[4];
	QuadtreeNode* Nodes[4];
	for(int i = 0; i < 4; ++i)
	{
		Idx[i] = QuadtreeGetNode(Quadtree);
	}
	for(int i = 0; i < 4; ++i)
	{
		Nodes[i] = Quadtree->Nodes + Idx[i];
	}

	QuadtreeNode* Node = Quadtree->Nodes + Info.NodeIdx;
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
	while(NodeEntityIdx)
	{
		QuadtreeNodeEntity* NodeEntity = Quadtree->NodeEntities + NodeEntityIdx;
		Quadtree->Idx = NodeEntity->Entity;
		uint32_t NextIdx = NodeEntity->Next;
		QuadtreeRetNodeEntity(Quadtree, NodeEntityIdx);
		QuadtreeMaybeReinsertInternal(Quadtree, Node, Info.Extent);
		NodeEntityIdx = NextIdx;
	}
	Quadtree->Idx = IdxSave;

	Node->Count = -1;
}


#define QuadtreeDescend(_Extent)											\
do																			\
{																			\
	QuadtreePosition HalfW = Info.Extent.W * 0.5f;							\
	QuadtreePosition HalfH = Info.Extent.H * 0.5f;							\
																			\
	if(_Extent.MinX <= Info.Extent.X)										\
	{																		\
		if(_Extent.MinY <= Info.Extent.Y)									\
		{																	\
			*(NodeInfo++) =													\
			(QuadtreeNodeInfo)												\
			{																\
				.NodeIdx = Node->Heads[0],									\
				.Extent =													\
				(QuadtreeHalfExtent)										\
				{															\
					.X = Info.Extent.X - HalfW,								\
					.Y = Info.Extent.Y - HalfH,								\
					.W = HalfW,												\
					.H = HalfH												\
				}															\
			};																\
		}																	\
		if(_Extent.MaxY >= Info.Extent.Y)									\
		{																	\
			*(NodeInfo++) =													\
			(QuadtreeNodeInfo)												\
			{																\
				.NodeIdx = Node->Heads[1],									\
				.Extent =													\
				(QuadtreeHalfExtent)										\
				{															\
					.X = Info.Extent.X - HalfW,								\
					.Y = Info.Extent.Y + HalfH,								\
					.W = HalfW,												\
					.H = HalfH												\
				}															\
			};																\
		}																	\
	}																		\
	if(_Extent.MaxX >= Info.Extent.X)										\
	{																		\
		if(_Extent.MinY <= Info.Extent.Y)									\
		{																	\
			*(NodeInfo++) =													\
			(QuadtreeNodeInfo)												\
			{																\
				.NodeIdx = Node->Heads[2],									\
				.Extent =													\
				(QuadtreeHalfExtent)										\
				{															\
					.X = Info.Extent.X + HalfW,								\
					.Y = Info.Extent.Y - HalfH,								\
					.W = HalfW,												\
					.H = HalfH												\
				}															\
			};																\
		}																	\
		if(_Extent.MaxY >= Info.Extent.Y)									\
		{																	\
			*(NodeInfo++) =													\
			(QuadtreeNodeInfo)												\
			{																\
				.NodeIdx = Node->Heads[3],									\
				.Extent =													\
				(QuadtreeHalfExtent)										\
				{															\
					.X = Info.Extent.X + HalfW,								\
					.Y = Info.Extent.Y + HalfH,								\
					.W = HalfW,												\
					.H = HalfH												\
				}															\
			};																\
		}																	\
	}																		\
}																			\
while(0)


static void
QuadtreeReinsert(
	Quadtree* QT,
	uint32_t EntityIdx
	)
{
	const QuadtreeEntity* Entity = QT->Entities + EntityIdx;
	QT->Idx = EntityIdx;

	QuadtreeNodeInfo NodeInfos[QUADTREE_DFS_LENGTH];
	QuadtreeNodeInfo* NodeInfo = NodeInfos;

	*(NodeInfo++) =
	(QuadtreeNodeInfo)
	{
		.NodeIdx = 0,
		.Extent = QT->HalfExtent
	};

	do
	{
		QuadtreeNodeInfo Info = *(--NodeInfo);
		goto_insert:;
		QuadtreeNode* Node = QT->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QuadtreeDescend(Entity->Extent);
			continue;
		}

		if(!Node->Head)
		{
			QuadtreeReinsertInternal(QT, Node);
			continue;
		}

		const QuadtreeNodeEntity* NodeEntity = QT->NodeEntities + Node->Head;

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
			NodeEntity = QT->NodeEntities + NodeEntity->Next;
		}

		if(
			Node->Count < QUADTREE_SPLIT_THRESHOLD ||
			Info.Extent.W <= QT->MinSize || Info.Extent.H <= QT->MinSize
			)
		{
			QuadtreeReinsertInternal(QT, Node);
		}
		else
		{
			QuadtreeSplit(QT, Info);

			goto goto_insert;
		}

		goto_skip:;
	}
	while(NodeInfo != NodeInfos);
}


QUADTREE_ENTITY_DATA_TYPE*
QuadtreeInsert(
	Quadtree* QT,
	const QuadtreeRectExtent* Extent
	)
{
	const uint32_t Idx = QuadtreeGetEntity(QT);
	QuadtreeEntity* Entity = QT->Entities + Idx;
	Entity->Extent = *Extent;
	Entity->QueryTick = QT->QueryTick;
	Entity->UpdateTick = QT->UpdateTick;
	Entity->Invalid = 0;

	QT->Idx = Idx;

	QuadtreeNodeInfo NodeInfos[QUADTREE_DFS_LENGTH];
	QuadtreeNodeInfo* NodeInfo = NodeInfos;

	*(NodeInfo++) =
	(QuadtreeNodeInfo)
	{
		.NodeIdx = 0,
		.Extent = QT->HalfExtent
	};

	do
	{
		QuadtreeNodeInfo Info = *(--NodeInfo);
		goto_insert:;
		QuadtreeNode* Node = QT->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QuadtreeDescend(Entity->Extent);
			continue;
		}

		if(
			Node->Count < QUADTREE_SPLIT_THRESHOLD ||
			Info.Extent.W <= QT->MinSize || Info.Extent.H <= QT->MinSize
			)
		{
			QuadtreeReinsertInternal(QT, Node);
		}
		else
		{
			QuadtreeSplit(QT, Info);

			goto goto_insert;
		}
	}
	while(NodeInfo != NodeInfos);

	return &Entity->Data;
}


void
QuadtreeRemove(
	Quadtree* QT,
	uint32_t EntityIdx
	)
{
	QuadtreeEntity* Entity = QT->Entities + EntityIdx;

	if(QT->PostponeRemovals)
	{
		uint32_t Idx = QuadtreeGetRemoval(QT);
		QT->Removals[Idx] = EntityIdx;
		return;
	}

	QuadtreeNodeInfo NodeInfos[QUADTREE_DFS_LENGTH];
	QuadtreeNodeInfo* NodeInfo = NodeInfos;

	*(NodeInfo++) =
	(QuadtreeNodeInfo)
	{
		.NodeIdx = 0,
		.Extent = QT->HalfExtent
	};

	do
	{
		QuadtreeNodeInfo Info = *(--NodeInfo);
		QuadtreeNode* Node = QT->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QuadtreeDescend(Entity->Extent);
			continue;
		}

		uint32_t Idx = Node->Head;
		QuadtreeNodeEntity* PrevNodeEntity = NULL;

		while(1)
		{
			QuadtreeNodeEntity* NodeEntity = QT->NodeEntities + Idx;
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
				QuadtreeRetNodeEntity(QT, Idx);
				break;
			}
			PrevNodeEntity = NodeEntity;
			Idx = NodeEntity->Next;
		}
	}
	while(NodeInfo != NodeInfos);

	QuadtreeRetEntity(QT, EntityIdx);
}
// todo qt->nodes is getting  reloaded everytime loop runs, cache it

void
QuadtreeUpdate(
	Quadtree* QT,
	QuadtreeUpdateT Callback
	)
{
	{
		QT->UpdateTick ^= 1;
		QuadtreePostponeRemovals(QT);

		QuadtreeNodeInfo NodeInfos[QUADTREE_DFS_LENGTH];
		QuadtreeNodeInfo* NodeInfo = NodeInfos;

		*(NodeInfo++) =
		(QuadtreeNodeInfo)
		{
			.NodeIdx = 0,
			.Extent = QT->HalfExtent
		};

		do
		{
			QuadtreeNodeInfo Info = *(--NodeInfo);
			QuadtreeNode* Node = QT->Nodes + Info.NodeIdx;

			if(Node->Count == -1)
			{
				QuadtreeDescend(((QuadtreeRectExtent){ Info.Extent.X, Info.Extent.X, Info.Extent.Y, Info.Extent.Y }));
				continue;
			}

			if(!Node->Head)
			{
				continue;
			}

			QuadtreeRectExtent Extent = QuadtreeHalfToRectExtent(Info.Extent);

			uint32_t Idx = Node->Head;
			uint32_t PrevIdx = 0;
			while(1)
			{
				QuadtreeNodeEntity* NodeEntity = QT->NodeEntities + Idx;
				uint32_t EntityIdx = NodeEntity->Entity;
				QuadtreeEntity* Entity = QT->Entities + EntityIdx;

				if(Entity->UpdateTick != QT->UpdateTick)
				{
					Entity->UpdateTick = QT->UpdateTick;
					uint8_t Changed = Callback(QT, Entity);

					Node = QT->Nodes + Info.NodeIdx;
					NodeEntity = QT->NodeEntities + Idx;
					Entity = QT->Entities + EntityIdx;

					if(Changed &&!QuadtreeIsInside(Entity->Extent, Extent))
					{
						QuadtreeReinsert(QT, EntityIdx);
						Node = QT->Nodes + Info.NodeIdx;
						NodeEntity = QT->NodeEntities + Idx;
					}
				}

				if(
					(Entity->Extent.MaxX < Extent.MinX && !(Node->PositionFlags & 0b0001)) ||
					(Entity->Extent.MaxY < Extent.MinY && !(Node->PositionFlags & 0b0010)) ||
					(Extent.MaxX < Entity->Extent.MinX && !(Node->PositionFlags & 0b0100)) ||
					(Extent.MaxY < Entity->Extent.MinY && !(Node->PositionFlags & 0b1000))
					)
				{
					if(PrevIdx)
					{
						QT->NodeEntities[PrevIdx].Next = NodeEntity->Next;
					}
					else
					{
						Node->Head = NodeEntity->Next;
					}
					--Node->Count;
					uint32_t NextIdx = NodeEntity->Next;
					QuadtreeRetNodeEntity(QT, Idx);
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
		while(NodeInfo != NodeInfos);

		QuadtreeDoRemovals(QT);
	}

	{
		uint32_t NodeInfos[QUADTREE_DFS_LENGTH];
		uint32_t* NodeInfo = NodeInfos;

		*(NodeInfo++) = 0;

		do
		{
			uint32_t NodeIdx = *(--NodeInfo);
			QuadtreeNode* Node = QT->Nodes + NodeIdx;

			if(Node->Count != -1)
			{
				continue;
			}

			QuadtreeNode* Children[4];
			for(int i = 0; i < 4; ++i)
			{
				Children[i] = QT->Nodes + Node->Heads[i];
			}
			uint32_t Total = 0;
			int Possible = 1;

			for(int i = 0; i < 4; ++i)
			{
				if(Children[i]->Count == -1)
				{
					*(NodeInfo++) = Node->Heads[i];
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
					QuadtreeNodeEntity* NodeEntity = QT->NodeEntities + Idx;

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
							QuadtreeNodeEntity* NodeEntity2 = QT->NodeEntities + Idx2;

							if(NodeEntity->Entity == NodeEntity2->Entity)
							{
								uint32_t SaveIdx = Idx;
								Idx = NodeEntity->Next;
								QuadtreeRetNodeEntity(QT, SaveIdx);
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
				QuadtreeRetNode(QT, Heads[i]);
			}
		}
		while(NodeInfo != NodeInfos);
	}

	{// todo here first shrink sizes but not arrays then use the shrunken sizes here
		QuadtreeEntity* Entities = malloc(sizeof(*Entities) * QT->EntitiesSize);
		uint32_t EntityIdx = 1;

		uint32_t* EntityMap = calloc(QT->EntitiesSize, sizeof(*EntityMap));

		QuadtreeNodeEntity* NodeEntities = malloc(sizeof(*NodeEntities) * QT->NodeEntitiesSize);
		uint32_t NodeEntityIdx = 1;

		QuadtreeNode* Nodes = malloc(sizeof(*Nodes) * QT->NodesSize);
		uint32_t NodeIdx = 0;

		typedef struct QuadtreeNodeReorderInfo
		{
			uint32_t NodeIdx;
			uint32_t* ParentIndexSlot;
		}
		QuadtreeNodeReorderInfo;

		QuadtreeNodeReorderInfo NodeInfos[QUADTREE_DFS_LENGTH];
		QuadtreeNodeReorderInfo* NodeInfo = NodeInfos;

		*(NodeInfo++) =
		(QuadtreeNodeReorderInfo)
		{
			.NodeIdx = 0,
			.ParentIndexSlot = NULL
		};

		do
		{
			QuadtreeNodeReorderInfo Info = *(--NodeInfo);
			QuadtreeNode* Node = QT->Nodes + Info.NodeIdx;
			uint32_t NewNodeIdx = NodeIdx++;
			QuadtreeNode* NewNode = Nodes + NewNodeIdx;

			if(Info.ParentIndexSlot)
			{
				*Info.ParentIndexSlot = NewNodeIdx;
			}

			if(Node->Count == -1)
			{
				for(int i = 0; i < 4; ++i)
				{
					*(NodeInfo++) =
					(QuadtreeNodeReorderInfo)
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

			QuadtreeNodeEntity* NodeEntity = QT->NodeEntities + Node->Head;
			NewNode->Head = NodeEntityIdx;
			NewNode->Count = Node->Count;

			while(1)
			{
				QuadtreeNodeEntity* NewNodeEntity = NodeEntities + NodeEntityIdx;
				++NodeEntityIdx;

				uint32_t OldEntityIdx = NodeEntity->Entity;
				if(!EntityMap[OldEntityIdx])
				{
					uint32_t NewEntityIdx = EntityIdx++;
					EntityMap[OldEntityIdx] = NewEntityIdx;
					QuadtreeEntity* NewEntity = Entities + NewEntityIdx;
					*NewEntity = QT->Entities[OldEntityIdx];
				}
				NewNodeEntity->Entity = EntityMap[OldEntityIdx];

				if(NodeEntity->Next)
				{
					NodeEntity = QT->NodeEntities + NodeEntity->Next;
					NewNodeEntity->Next = NodeEntityIdx;
				}
				else
				{
					NewNodeEntity->Next = 0;
					break;
				}
			}
		}
		while(NodeInfo != NodeInfos);

		free(QT->Nodes);
		QT->Nodes = Nodes;
		QT->NodesUsed = NodeIdx;
		QT->FreeNode = 0;

		free(QT->NodeEntities);
		QT->NodeEntities = NodeEntities;
		QT->NodeEntitiesUsed = NodeEntityIdx;
		QT->FreeNodeEntity = 0;

		free(QT->Entities);
		QT->Entities = Entities;
		QT->EntitiesUsed = EntityIdx;
		QT->FreeEntity = 0;

		free(EntityMap);
	}

	QuadtreeMaybeShrinkNodes(QT);
	QuadtreeMaybeShrinkNodeEntities(QT);
	QuadtreeMaybeShrinkEntities(QT);
	QuadtreeMaybeShrinkHTEntries(QT);
	QuadtreeMaybeShrinkRemovals(QT);
}


void
QuadtreeQuery(
	Quadtree* QT,
	QuadtreeRectExtent Extent,
	QuadtreeQueryT Callback
	)
{
	++QT->QueryTick;
	QuadtreePostponeRemovals(QT);

	QuadtreeNodeInfo NodeInfos[QUADTREE_DFS_LENGTH];
	QuadtreeNodeInfo* NodeInfo = NodeInfos;

	*(NodeInfo++) =
	(QuadtreeNodeInfo)
	{
		.NodeIdx = 0,
		.Extent = QT->HalfExtent
	};

	do
	{
		QuadtreeNodeInfo Info = *(--NodeInfo);
		QuadtreeNode* Node = QT->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QuadtreeDescend(Extent);
			continue;
		}

		uint32_t Idx = Node->Head;
		while(Idx)
		{
			QuadtreeNodeEntity* NodeEntity = QT->NodeEntities + Idx;
			QuadtreeEntity* Entity = QT->Entities + NodeEntity->Entity;

			if(Entity->QueryTick != QT->QueryTick)
			{
				Entity->QueryTick = QT->QueryTick;
				if(QuadtreeIntersects(Entity->Extent, Extent))
				{
					Callback(QT, Entity);
				}
			}

			Idx = NodeEntity->Next;
		}
	}
	while(NodeInfo != NodeInfos);

	QuadtreeDoRemovals(QT);
}


void
QuadtreeQueryNodes(
	Quadtree* QT,
	QuadtreeRectExtent Extent,
	QuadtreeNodeQueryT Callback
	)
{
	QuadtreeNodeInfo NodeInfos[QUADTREE_DFS_LENGTH];
	QuadtreeNodeInfo* NodeInfo = NodeInfos;

	*(NodeInfo++) =
	(QuadtreeNodeInfo)
	{
		.NodeIdx = 0,
		.Extent = QT->HalfExtent
	};

	do
	{
		QuadtreeNodeInfo Info = *(--NodeInfo);
		QuadtreeNode* Node = QT->Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QuadtreeDescend(Extent);
			continue;
		}

		Callback(QT, &Info);
	}
	while(NodeInfo != NodeInfos);
}


static int
QuadtreeHash(
	Quadtree* QT,
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
	Hash %= QT->HashTableSize;

	uint32_t Index = QT->HashTable[Hash];
	QuadtreeHTEntry* Entry;

	while(Index)
	{
		Entry = QT->HTEntries + Index;

		if(Entry->Idx[0] == IndexA && Entry->Idx[1] == IndexB)
		{
			return 1; /* Hash exists */
		}

		Index = Entry->Next;
	}

	uint32_t EntryIdx = QuadtreeGetHTEntry(QT);
	Entry = QT->HTEntries + EntryIdx;
	Entry->Idx[0] = IndexA;
	Entry->Idx[1] = IndexB;
	Entry->Next = Index;
	QT->HashTable[Hash] = EntryIdx;

	return 0;
}


void
QuadtreeCollide(
	Quadtree* QT,
	QuadtreeCollideT Callback
	)
{
	QuadtreeNode* Node = QT->Nodes;
	QuadtreeNode* EndNode = QT->Nodes + QT->NodesUsed;

#if QUADTREE_DEDUPE_COLLISIONS == 1
	QT->HashTableSize = QT->EntitiesUsed * QUADTREE_HASH_TABLE_FACTOR;
	QT->HashTable = calloc(QT->HashTableSize, sizeof(*QT->HashTable));
	QuadtreeResetHTEntries(QT);
#endif

	while(Node != EndNode)
	{
		if(Node->Count == -1 || !Node->Head || QuadtreeIsNodeInvalid(Node))
		{
			++Node;
			continue;
		}

		QuadtreeNodeEntity* NodeEntity = QT->NodeEntities + Node->Head;
		QuadtreeEntity* Entity = QT->Entities + NodeEntity->Entity;

		QuadtreeNodeEntity* OtherNodeEntitySave = QT->NodeEntities + Node->Head;
		++Node;

		while(1) // todo this stupid cuz wastes one loop cuz we start from the same node entity for some stupid reason
		{
			QuadtreeNodeEntity* OtherNodeEntity = OtherNodeEntitySave;
			QuadtreeEntity* OtherEntity = QT->Entities + OtherNodeEntity->Entity;

			while(1)
			{
				if(
					NodeEntity->Entity != OtherNodeEntity->Entity &&
					QuadtreeIntersects(Entity->Extent, OtherEntity->Extent)
#if QUADTREE_DEDUPE_COLLISIONS == 1
					&& !QuadtreeHash(QT, NodeEntity->Entity, OtherNodeEntity->Entity)
#endif
					)
				{
					Callback(QT, Entity, OtherEntity);
				}

				if(!OtherNodeEntity->Next)
				{
					break;
				}

				OtherNodeEntity = QT->NodeEntities + OtherNodeEntity->Next;
				OtherEntity = QT->Entities + OtherNodeEntity->Entity;
			}

			if(!NodeEntity->Next)
			{
				break;
			}

			NodeEntity = QT->NodeEntities + NodeEntity->Next;
			Entity = QT->Entities + NodeEntity->Entity;
		}
	}

#if QUADTREE_DEDUPE_COLLISIONS == 1
	free(QT->HashTable);
#endif
}


#undef QuadtreeDescend

