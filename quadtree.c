#include "quadtree.h"
#include "alloc/include/debug.h"
#include "alloc/include/alloc_ext.h"

#include <stdlib.h>
#include <string.h>


Static void
QuadtreeInvalidateNode(
	QuadtreeNode* Node
	)
{
	Node->Head = -1;
}


Static bool
QuadtreeIsNodeInvalid(
	QuadtreeNode* Node
	)
{
	return Node->Head == -1;
}


Static void
QuadtreeInvalidateNodeEntity(
	QuadtreeNodeEntity* NodeEntity
	)
{
	NodeEntity->Entity = 0;
}


Static bool
QuadtreeIsNodeEntityInvalid(
	QuadtreeNodeEntity* NodeEntity
	)
{
	return NodeEntity->Entity == 0;
}


Static void
QuadtreeInvalidateEntity(
	QuadtreeEntity* Entity
	)
{
	Entity->Invalid = 1;
}


Static bool
QuadtreeIsEntityInvalid(
	QuadtreeEntity* Entity
	)
{
	return Entity->Invalid;
}


#define QuadtreeResizeDef(Name, Names)									\
Static void																\
QuadtreeResize##Names (													\
	Quadtree* QT														\
	)																	\
{																		\
	if(QT-> Names##Used >= QT-> Names##Size )							\
	{																	\
		uint32_t NewSize = QT-> Names##Used << 1;						\
																		\
		QT-> Names = AllocRemalloc(										\
			sizeof(*QT-> Names ) * QT-> Names##Size ,					\
			QT-> Names ,												\
			sizeof(*QT-> Names) * NewSize								\
			);															\
		AssertNEQ(QT-> Names, NULL);									\
																		\
		QT-> Names##Size = NewSize;										\
	}																	\
}


#define QuadtreeDef(Name, Names)										\
																		\
QuadtreeResizeDef(Name, Names)											\
																		\
Static uint32_t															\
QuadtreeGet##Name (														\
	Quadtree* QT														\
	)																	\
{																		\
	if(QT-> Free##Name )												\
	{																	\
		uint32_t Ret = QT-> Free##Name ;								\
		(void) memcpy(													\
			&QT-> Free##Name ,											\
			QT-> Names + Ret,											\
			sizeof(QT-> Free##Name )); 									\
		return Ret;														\
	}																	\
																		\
	QuadtreeResize##Names (QT);											\
																		\
	return QT-> Names##Used ++;											\
}																		\
																		\
Static void																\
QuadtreeRet##Name (														\
	Quadtree* QT,														\
	uint32_t Idx														\
	)																	\
{																		\
	QuadtreeInvalidate##Name (QT-> Names + Idx);						\
	(void) memcpy(														\
		QT-> Names + Idx,												\
		&QT-> Free##Name ,												\
		sizeof(QT-> Free##Name ));										\
	QT-> Free##Name = Idx;												\
}																		\
																		\
Static void																\
QuadtreeFree##Names(													\
	Quadtree* QT														\
	)																	\
{																		\
	AllocFree(sizeof(*QT-> Names ) * QT-> Names##Size , QT-> Names );	\
	QT-> Names = NULL;													\
	QT-> Free##Name = 0;												\
	QT-> Names##Used = 1;												\
	QT-> Names##Size = 0;												\
}

QuadtreeDef(Node, Nodes)
QuadtreeDef(NodeEntity, NodeEntities)
QuadtreeDef(Entity, Entities)

#undef QuadtreeDef


#define QuadtreeDef(Name, Names)										\
																		\
QuadtreeResizeDef(Name, Names)											\
																		\
Static uint32_t															\
QuadtreeGet##Name (														\
	Quadtree* QT														\
	)																	\
{																		\
	QuadtreeResize##Names (QT);											\
																		\
	return QT-> Names##Used ++;											\
}																		\
																		\
Static void																\
QuadtreeFree##Names(													\
	Quadtree* QT														\
	)																	\
{																		\
	AllocFree(sizeof(*QT-> Names ) * QT-> Names##Size , QT-> Names );	\
	QT-> Names = NULL;													\
	QT-> Names##Used = 1;												\
	QT-> Names##Size = 0;												\
}

QuadtreeDef(HTEntry, HTEntries)
QuadtreeDef(Removal, Removals)
QuadtreeDef(NodeRemoval, NodeRemovals)
QuadtreeDef(Insertion, Insertions)
QuadtreeDef(Reinsertion, Reinsertions)

#undef QuadtreeDef
#undef QuadtreeResizeDef


void
QuadtreeInit(
	Quadtree* QT
	)
{
#define QuadtreeDef(Name, Names)	\
	QT-> Names = NULL;				\
	QT-> Free##Name = 0;			\
	QT-> Names##Used = 1;			\
	QT-> Names##Size = 0

	QuadtreeDef(Node, Nodes);
	QuadtreeDef(NodeEntity, NodeEntities);
	QuadtreeDef(Entity, Entities);
#undef QuadtreeDef

#define QuadtreeDef(Name, Names)	\
	QT-> Names = NULL;				\
	QT-> Names##Used = 1;			\
	QT-> Names##Size = 0

	QuadtreeDef(HTEntry, HTEntries);
	QuadtreeDef(Removal, Removals);
	QuadtreeDef(NodeRemoval, NodeRemovals);
	QuadtreeDef(Insertion, Insertions);
	QuadtreeDef(Reinsertion, Reinsertions);
#undef QuadtreeDef

	QuadtreeResizeNodes(QT);

	QT->Nodes[0].Count = 0;
	QT->Nodes[0].Head = 0;
	QT->Nodes[0].PositionFlags = 0b1111; /* TRBL */
}


void
QuadtreeFree(
	Quadtree* QT
	)
{
	QuadtreeFreeNodes(QT);
	QuadtreeFreeNodeEntities(QT);
	QuadtreeFreeEntities(QT);

	QuadtreeFreeHTEntries(QT);
	QuadtreeFreeRemovals(QT);
	QuadtreeFreeNodeRemovals(QT);
	QuadtreeFreeInsertions(QT);
	QuadtreeFreeReinsertions(QT);
}


Static void
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


Static void
QuadtreeMaybeReinsertInternal(
	Quadtree* QT,
	QuadtreeNode* Node,
	QuadtreeHalfExtent NodeExtent
	)
{
	const QuadtreeEntity* Entity = QT->Entities + QT->Idx;

	QuadtreeRectExtent Extent = QuadtreeGetEntityRectExtent(Entity);

	if(Extent.MinX <= NodeExtent.X)
	{
		if(Extent.MinY <= NodeExtent.Y)
		{
			QuadtreeReinsertInternal(QT, QT->Nodes + Node->Heads[0]);
		}
		if(Extent.MaxY >= NodeExtent.Y)
		{
			QuadtreeReinsertInternal(QT, QT->Nodes + Node->Heads[1]);
		}
	}
	if(Extent.MaxX >= NodeExtent.X)
	{
		if(Extent.MinY <= NodeExtent.Y)
		{
			QuadtreeReinsertInternal(QT, QT->Nodes + Node->Heads[2]);
		}
		if(Extent.MaxY >= NodeExtent.Y)
		{
			QuadtreeReinsertInternal(QT, QT->Nodes + Node->Heads[3]);
		}
	}
}


Static void
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


Static void
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
			QuadtreeDescend(QuadtreeGetEntityRectExtent(Entity));
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


void
QuadtreeInsert(
	Quadtree* QT,
	const QuadtreeEntityData* Data
	)
{
	uint32_t Idx = QuadtreeGetEntity(QT);
	QuadtreeEntity* Entity = QT->Entities + Idx;
	Entity->Data = *Data;
	Entity->QueryTick = QT->QueryTick;
	Entity->UpdateTick = QT->UpdateTick;
	Entity->Invalid = 0;

	QuadtreeRectExtent Extent = QuadtreeGetEntityRectExtent(Entity);

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
			QuadtreeDescend(Extent);
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
}


void
QuadtreeRemove(
	Quadtree* QT,
	const QuadtreeEntityData* Data
	)
{
	QuadtreeEntity* Entity = (void*) Data;
	QuadtreeRectExtent Extent = QuadtreeGetEntityRectExtent(Entity);
	uint32_t EntityIdx = Entity - QT->Entities;

	QuadtreeNode* Nodes = QT->Nodes;
	QuadtreeNodeEntity* NodeEntities = QT->NodeEntities;

	int32_t* NodeMap = QT->NodeMap;

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
		QuadtreeNode* Node = Nodes + Info.NodeIdx;

		if(Node->Count == -1)
		{
			QuadtreeDescend(Extent);
			continue;
		}

		uint32_t Idx = Node->Head;
		QuadtreeNodeEntity* PrevNodeEntity = NULL;

		while(1)
		{
			QuadtreeNodeEntity* NodeEntity = NodeEntities + Idx;

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
				--NodeMap[Info.NodeIdx];

				QuadtreeRetNodeEntity(QT, Idx);
				NodeEntities = QT->NodeEntities;

				break;
			}

			PrevNodeEntity = NodeEntity;
			Idx = NodeEntity->Next;
		}
	}
	while(NodeInfo != NodeInfos);

	QuadtreeRetEntity(QT, EntityIdx);
}


Static void
QuadtreeNormalize(
	Quadtree* QT
	)
{
	if(!QT->RemovalsUsed && !QT->NodeRemovalsUsed && !QT->InsertionsUsed)
	{
		return;
	}

	QuadtreeNode* Nodes = QT->Nodes;
	QuadtreeNodeEntity* NodeEntities = QT->NodeEntities;
	QuadtreeEntity* Entities = QT->Entities;

	QuadtreeReinsertion* Reinsertions = QT->Reinsertions;

	uint32_t FreeNode = QT->FreeNode;
	uint32_t NodesUsed = QT->NodesUsed;
	uint32_t NodesSize = QT->NodesSize;

	uint32_t FreeNodeEntity = QT->FreeNodeEntity;
	uint32_t NodeEntitiesUsed = QT->NodeEntitiesUsed;
	uint32_t NodeEntitiesSize = QT->NodeEntitiesSize;

	uint32_t FreeEntity = QT->FreeEntity;
	uint32_t EntitiesUsed = QT->EntitiesUsed;
	uint32_t EntitiesSize = QT->EntitiesSize;

	if(!QT->NodeMap)
	{
		QT->NodeMap = AllocCalloc(sizeof(*QT->NodeMap) * QT->NodesSize);
		AssertNEQ(QT->NodeMap, NULL);
	}

	int32_t* NodeMap = QT->NodeMap;

	QuadtreeNodeInfo NodeInfos[QUADTREE_DFS_LENGTH];
	QuadtreeNodeInfo* NodeInfo;


	{
		QuadtreeRemoval* Removals = QT->Removals;
		QuadtreeRemoval* Removal = Removals;
		QuadtreeRemoval* RemovalEnd = Removal + QT->RemovalsUsed;

		while(Removal != RemovalEnd)
		{
			NodeInfo = NodeInfos;

			*(NodeInfo++) =
			(QuadtreeNodeInfo)
			{
				.NodeIdx = 0,
				.Extent = QT->HalfExtent
			};

			uint32_t EntityIdx = Removal->EntityIdx;
			QuadtreeEntity* Entity = Entities + EntityIdx;
			QuadtreeRectExtent EntityExtent = QuadtreeGetEntityRectExtent(Entity);

			do
			{
				QuadtreeNodeInfo Info = *(--NodeInfo);
				QuadtreeNode* Node = Nodes + Info.NodeIdx;

				if(Node->Count == -1)
				{
					QuadtreeDescend(EntityExtent);
					continue;
				}

				QuadtreeNodeEntity* PrevNodeEntity = NULL;
				uint32_t NodeEntityIdx = Node->Head;
				QuadtreeNodeEntity* NodeEntity = NodeEntities + NodeEntityIdx;

				while(1)
				{
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
						--NodeMap[Info.NodeIdx];

						NodeEntity->Next = FreeNodeEntity;
						NodeEntity->Entity = 0;
						FreeNodeEntity = NodeEntityIdx;

						break;
					}

					PrevNodeEntity = NodeEntity;
					NodeEntityIdx = NodeEntity->Next;
					NodeEntity = NodeEntities + NodeEntityIdx;
				}
			}
			while(NodeInfo != NodeInfos);

			Entity->Next = FreeEntity;
			Entity->Invalid = 1;
			FreeEntity = EntityIdx;

			++Removal;
		}

		AllocFree(sizeof(*Removals) * QT->RemovalsSize, Removals);
		QT->Removals = NULL;
		QT->RemovalsUsed = 1;
		QT->RemovalsSize = 0;
	}


	{
		QuadtreeNodeRemoval* NodeRemovals = QT->NodeRemovals;
		QuadtreeNodeRemoval* NodeRemoval = NodeRemovals;
		QuadtreeNodeRemoval* NodeRemovalEnd = NodeRemoval + QT->NodeRemovalsUsed;

		while(NodeRemoval != NodeRemovalEnd)
		{
			QuadtreeNode* Node = Nodes + NodeRemoval->NodeIdx;

			uint32_t NodeEntityIdx = NodeRemoval->NodeEntityIdx;
			QuadtreeNodeEntity* NodeEntity = NodeEntities + NodeEntityIdx;

			uint32_t PrevNodeEntityIdx = NodeRemoval->PrevNodeEntityIdx;
			QuadtreeNodeEntity* PrevNodeEntity = NodeEntities + PrevNodeEntityIdx;

			if(PrevNodeEntity)
			{
				PrevNodeEntity->Next = NodeEntity->Next;
			}
			else
			{
				Node->Head = NodeEntity->Next;
			}

			--Node->Count;
			--NodeMap[NodeRemoval->NodeIdx];

			NodeEntity->Next = FreeNodeEntity;
			NodeEntity->Entity = 0;
			FreeNodeEntity = NodeEntityIdx;

			++NodeRemoval;
		}

		AllocFree(sizeof(*NodeRemovals) * QT->NodeRemovalsSize, NodeRemovals);
		QT->NodeRemovals = NULL;
		QT->NodeRemovalsUsed = 1;
		QT->NodeRemovalsSize = 0;
	}

	{
		QuadtreeInsertion* Insertions = QT->Insertions;
		QuadtreeInsertion* Insertion = Insertions;
		QuadtreeInsertion* InsertionEnd = Insertion + QT->InsertionsUsed;

		while(Insertion != InsertionEnd)
		{
			QuadtreeEntityData* Data = &Insertion->Data;

			uint32_t EntityIdx;
			QuadtreeEntity* Entity;

			if(FreeEntity)
			{
				EntityIdx = FreeEntity;
				Entity = Entities + EntityIdx;
				FreeEntity = Entity->Next;
			}
			else
			{
				if(EntitiesUsed >= EntitiesSize)
				{
					uint32_t NewSize = EntitiesUsed << 1;

					Entities = AllocRemalloc(sizeof(*Entities) * EntitiesSize,
						Entities, sizeof(*Entities) * NewSize);
					AssertNEQ(Entities, NULL);

					EntitiesSize = NewSize;
				}

				EntityIdx = EntitiesUsed++;
				Entity = Entities + EntityIdx;
			}

			Entity->Data = *Data;
			Entity->QueryTick = QT->QueryTick;
			Entity->UpdateTick = QT->UpdateTick;
			Entity->Invalid = 0;

			QuadtreeRectExtent EntityExtent = QuadtreeGetEntityRectExtent(Entity);

			NodeInfo = NodeInfos;

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
					QuadtreeDescend(EntityExtent);
					continue;
				}

				uint32_t NodeEntityIdx;
				QuadtreeNodeEntity* NodeEntity;

				if(FreeNodeEntity)
				{
					NodeEntityIdx = FreeNodeEntity;
					NodeEntity = NodeEntities + NodeEntityIdx;
					FreeNodeEntity = NodeEntity->Next;
				}
				else
				{
					if(NodeEntitiesUsed >= NodeEntitiesSize)
					{
						uint32_t NewSize = NodeEntitiesUsed << 1;

						NodeEntities = AllocRemalloc(sizeof(*NodeEntities) * NodeEntitiesSize,
							NodeEntities, sizeof(*NodeEntities) * NewSize);
						AssertNEQ(NodeEntities, NULL);

						NodeEntitiesSize = NewSize;
					}

					NodeEntityIdx = NodeEntitiesUsed++;
					NodeEntity = NodeEntities + NodeEntityIdx;
				}

				NodeEntity->Next = Node->Head;
				NodeEntity->Entity = EntityIdx;
				Node->Head = NodeEntityIdx;

				++Node->Count;
				++NodeMap[Info.NodeIdx];
			}
			while(NodeInfo != NodeInfos);

			++Insertion;
		}

		AllocFree(sizeof(*Insertions) * QT->InsertionsSize, Insertions);
		QT->Insertions = NULL;
		QT->InsertionsUsed = 1;
		QT->InsertionsSize = 0;
	}


	{
		QuadtreeReinsertion* Reinsertion = Reinsertions;
		QuadtreeReinsertion* ReinsertionEnd = Reinsertion + QT->ReinsertionsUsed;

		while(Reinsertion != ReinsertionEnd)
		{
			uint32_t EntityIdx = Reinsertion->EntityIdx;
			QuadtreeEntity* Entity = Entities + EntityIdx;

			QuadtreeRectExtent EntityExtent = QuadtreeGetEntityRectExtent(Entity);

			NodeInfo = NodeInfos;

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
					QuadtreeDescend(EntityExtent);
					continue;
				}

				QuadtreeNodeEntity* PrevNodeEntity = NULL;

				uint32_t NodeEntityIdx = Node->Head;
				QuadtreeNodeEntity* NodeEntity = NodeEntities + NodeEntityIdx;

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

					PrevNodeEntity = NodeEntity;
					NodeEntityIdx = NodeEntity->Next;
					NodeEntity = NodeEntities + NodeEntityIdx;
				}

				// copy paste from insert

				goto_skip:;
			}
			while(NodeInfo != NodeInfos);
		}
	}
}


void
QuadtreeUpdate(
	Quadtree* QT,
	QuadtreeUpdateT Callback
	)
{
	{
		QT->UpdateTick ^= 1;
		++QT->Postpone;

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

			QuadtreeRectExtent NodeExtent = QuadtreeHalfToRectExtent(Info.Extent);

			uint32_t Idx = Node->Head;
			uint32_t PrevIdx = 0;
			while(1)
			{
				QuadtreeNodeEntity* NodeEntity = QT->NodeEntities + Idx;
				uint32_t EntityIdx = NodeEntity->Entity;
				QuadtreeEntity* Entity = QT->Entities + EntityIdx;

				QuadtreeRectExtent Extent = QuadtreeGetEntityRectExtent(Entity);

				if(Entity->UpdateTick != QT->UpdateTick)
				{
					Entity->UpdateTick = QT->UpdateTick;
					uint8_t Changed = Callback(QT, &Entity->Data);

					Node = QT->Nodes + Info.NodeIdx;
					NodeEntity = QT->NodeEntities + Idx;
					Entity = QT->Entities + EntityIdx;

					if(Changed && !QuadtreeIsInside(Extent, NodeExtent))
					{
						QuadtreeReinsert(QT, EntityIdx);
						Node = QT->Nodes + Info.NodeIdx;
						NodeEntity = QT->NodeEntities + Idx;
					}
				}

				if(
					(Extent.MaxX < NodeExtent.MinX && !(Node->PositionFlags & 0b0001)) ||
					(Extent.MaxY < NodeExtent.MinY && !(Node->PositionFlags & 0b0010)) ||
					(NodeExtent.MaxX < Extent.MinX && !(Node->PositionFlags & 0b0100)) ||
					(NodeExtent.MaxY < Extent.MinY && !(Node->PositionFlags & 0b1000))
					)
				{
					uint32_t NodeRemovalIdx = QuadtreeGetNodeRemoval(QT);
					QuadtreeNodeRemoval* NodeRemoval = QT->NodeRemovals + NodeRemovalIdx;
					NodeRemoval->NodeIdx = Info.NodeIdx;
					NodeRemoval->NodeEntityIdx = Idx;
					NodeRemoval->PrevNodeEntityIdx = PrevIdx;
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

		--QT->Postpone;
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
}


void
QuadtreeQuery(
	Quadtree* QT,
	QuadtreeRectExtent Extent,
	QuadtreeQueryT Callback
	)
{
	++QT->QueryTick;
	++QT->Postpone;

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
				if(QuadtreeIntersects(QuadtreeGetEntityRectExtent(Entity), Extent))
				{
					Callback(QT, &Entity->Data);
				}
			}

			Idx = NodeEntity->Next;
		}
	}
	while(NodeInfo != NodeInfos);

	--QT->Postpone;
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


void
QuadtreeCollide(
	Quadtree* QT,
	QuadtreeCollideT Callback
	)
{
	QuadtreeNode* Node = QT->Nodes;
	QuadtreeNode* EndNode = QT->Nodes + QT->NodesUsed;

#if QUADTREE_DEDUPE_COLLISIONS == 1
	uint32_t HashTableSize = QT->EntitiesUsed * QUADTREE_HASH_TABLE_FACTOR;
	uint32_t* HashTable = calloc(HashTableSize, sizeof(*HashTable));
	QT->HTEntriesUsed = 1;
#endif

	QuadtreeNodeEntity* NodeEntities = QT->NodeEntities;
	QuadtreeEntity* Entities = QT->Entities;

	QuadtreeHTEntry* HTEntries = QT->HTEntries;

	while(Node != EndNode)
	{
		if(Node->Count == -1 || !Node->Head || QuadtreeIsNodeInvalid(Node))
		{
			++Node;
			continue;
		}

		QuadtreeNodeEntity* NodeEntity = NodeEntities + Node->Head;
		++Node;

		QuadtreeEntity* Entity = Entities + NodeEntity->Entity;

		while(NodeEntity->Next)
		{
			QuadtreeNodeEntity* OtherNodeEntity = NodeEntities + NodeEntity->Next;

			while(1)
			{
				uint32_t IndexA = NodeEntity->Entity;
				uint32_t IndexB = OtherNodeEntity->Entity;

				if(NodeEntity->Entity == OtherNodeEntity->Entity)
				{
					goto goto_skip;
				}

				QuadtreeEntity* OtherEntity = Entities + OtherNodeEntity->Entity;

				if(!QuadtreeIntersects(
					QuadtreeGetEntityRectExtent(Entity),
					QuadtreeGetEntityRectExtent(OtherEntity)
					))
				{
					goto goto_skip;
				}

#if QUADTREE_DEDUPE_COLLISIONS == 1
				if(IndexA > IndexB)
				{
					uint32_t Temp = IndexA;
					IndexA = IndexB;
					IndexB = Temp;
				}

				uint32_t Hash = IndexA * 48611 + IndexB * 50261;
				Hash %= HashTableSize;

				uint32_t Index = HashTable[Hash];
				QuadtreeHTEntry* Entry;

				while(Index)
				{
					Entry = QT->HTEntries + Index;

					if(Entry->Idx[0] == IndexA && Entry->Idx[1] == IndexB)
					{
						goto goto_skip;
					}

					Index = Entry->Next;
				}

				uint32_t EntryIdx = QuadtreeGetHTEntry(QT);
				HTEntries = QT->HTEntries;
				Entry = QT->HTEntries + EntryIdx;
				Entry->Idx[0] = IndexA;
				Entry->Idx[1] = IndexB;
				Entry->Next = Index;
				HashTable[Hash] = EntryIdx;
#endif

				Callback(QT, &Entity->Data, &OtherEntity->Data);


				goto_skip:

				if(!OtherNodeEntity->Next)
				{
					break;
				}

				OtherNodeEntity = NodeEntities + OtherNodeEntity->Next;
			}

			if(!NodeEntity->Next)
			{
				break;
			}

			NodeEntity = NodeEntities + NodeEntity->Next;
			Entity = Entities + NodeEntity->Entity;
		}
	}

#if QUADTREE_DEDUPE_COLLISIONS == 1
	free(HashTable);
#endif
}


#undef QuadtreeDescend

