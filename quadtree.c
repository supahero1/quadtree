#include "quadtree.h"
#include "alloc/include/debug.h"
#include "alloc/include/alloc_ext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void
QuadtreeInit(
	Quadtree* QT
	)
{
	QT->Nodes = AllocMalloc(sizeof(*QT->Nodes));
	QT->NodeEntities = NULL;
	QT->Entities = NULL;
#if QUADTREE_DEDUPE_COLLISIONS == 1
	QT->HTEntries = NULL;
#endif
	QT->Removals = NULL;
	QT->NodeRemovals = NULL;
	QT->Insertions = NULL;
	QT->Reinsertions = NULL;

	QT->NodesUsed = 1;
	QT->NodesSize = 1;

	QT->NodeEntitiesUsed = 1;
	QT->NodeEntitiesSize = 0;

	QT->EntitiesUsed = 1;
	QT->EntitiesSize = 0;

#if QUADTREE_DEDUPE_COLLISIONS == 1
	QT->HTEntriesSize = 0;
#endif

	QT->RemovalsUsed = 0;
	QT->RemovalsSize = 0;

	QT->NodeRemovalsUsed = 0;
	QT->NodeRemovalsSize = 0;

	QT->InsertionsUsed = 0;
	QT->InsertionsSize = 0;

	QT->ReinsertionsUsed = 0;
	QT->ReinsertionsSize = 0;

	AssertNEQ(QT->Nodes, NULL);

	QT->Nodes[0].Head = 0;
	QT->Nodes[0].Count = 0;
	QT->Nodes[0].PositionFlags = 0b1111; /* TRBL */
}


void
QuadtreeFree(
	Quadtree* QT
	)
{
	AllocFree(sizeof(*QT->Nodes) * QT->NodesSize, QT->Nodes);
	AllocFree(sizeof(*QT->NodeEntities) * QT->NodeEntitiesSize, QT->NodeEntities);
	AllocFree(sizeof(*QT->Entities) * QT->EntitiesSize, QT->Entities);
#if QUADTREE_DEDUPE_COLLISIONS == 1
	AllocFree(sizeof(*QT->HTEntries) * QT->HTEntriesSize, QT->HTEntries);
#endif
	AllocFree(sizeof(*QT->Removals) * QT->RemovalsSize, QT->Removals);
	AllocFree(sizeof(*QT->NodeRemovals) * QT->NodeRemovalsSize, QT->NodeRemovals);
	AllocFree(sizeof(*QT->Insertions) * QT->InsertionsSize, QT->Insertions);
	AllocFree(sizeof(*QT->Reinsertions) * QT->ReinsertionsSize, QT->Reinsertions);
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


void
QuadtreeInsert(
	Quadtree* QT,
	const QuadtreeEntityData* Data
	)
{
	if(QT->InsertionsUsed >= QT->InsertionsSize)
	{
		uint32_t NewSize = (QT->InsertionsUsed << 1) | 1;

		QT->Insertions = AllocRemalloc(sizeof(*QT->Insertions) * QT->InsertionsSize,
			QT->Insertions, sizeof(*QT->Insertions) * NewSize);
		AssertNEQ(QT->Insertions, NULL);

		QT->InsertionsSize = NewSize;
	}

	uint32_t InsertionIdx = QT->InsertionsUsed++;
	QuadtreeInsertion* Insertion = QT->Insertions + InsertionIdx;

	Insertion->Data = *Data;
}


void
QuadtreeRemove(
	Quadtree* QT,
	uint32_t EntityIdx
	)
{
	if(QT->RemovalsUsed >= QT->RemovalsSize)
	{
		uint32_t NewSize = (QT->RemovalsUsed << 1) | 1;

		QT->Removals = AllocRemalloc(sizeof(*QT->Removals) * QT->RemovalsSize,
			QT->Removals, sizeof(*QT->Removals) * NewSize);
		AssertNEQ(QT->Removals, NULL);

		QT->RemovalsSize = NewSize;
	}

	uint32_t RemovalIdx = QT->RemovalsUsed++;
	QuadtreeRemoval* Removal = QT->Removals + RemovalIdx;

	Removal->EntityIdx = EntityIdx;
}


void
QuadtreeNormalize(
	Quadtree* QT
	)
{
	if(!QT->InsertionsUsed && !QT->ReinsertionsUsed && !QT->RemovalsUsed && !QT->NodeRemovalsUsed)
	{
		return;
	}

	QuadtreeNode* Nodes = QT->Nodes;
	QuadtreeNodeEntity* NodeEntities = QT->NodeEntities;
	QuadtreeEntity* Entities = QT->Entities;

	uint32_t FreeNodeEntity = 0; /* Not reflected in QT->FreeNodeEntity */
	uint32_t NodeEntitiesUsed = QT->NodeEntitiesUsed;
	uint32_t NodeEntitiesSize = QT->NodeEntitiesSize;

	uint32_t FreeEntity = 0; /* Not reflected in QT->FreeEntity */
	uint32_t EntitiesUsed = QT->EntitiesUsed;
	uint32_t EntitiesSize = QT->EntitiesSize;

	QuadtreeNodeInfo NodeInfos[QUADTREE_DFS_LENGTH];
	QuadtreeNodeInfo* NodeInfo;


	/* Node removals -> removals -> reinsertions -> insertions (this order is important) */


	if(QT->NodeRemovalsUsed)
	{
		/* First of all, this is safe, because if an entity is inserted, the actual insertion is delayed until this
		 * function, therefore it cannot be iterated over during the same update, so it can never be node removed.
		 *
		 * Second of all, this is necessary, because PrevNodeEntityIdx may no longer be true after insertions
		 * and removals are performed.
		 *
		 * Third of all, need to traverse the array backwards, because Next indexes will get corrupted otherwise.
		 *
		 * Sketchy code, but it works after couple hours of debugging.
		 */

		QuadtreeNodeRemoval* NodeRemovals = QT->NodeRemovals;
		QuadtreeNodeRemoval* NodeRemoval = NodeRemovals + QT->NodeRemovalsUsed - 1;

		while(NodeRemoval >= NodeRemovals)
		{
			uint32_t NodeIdx = NodeRemoval->NodeIdx;
			QuadtreeNode* Node = Nodes + NodeIdx;

			uint32_t NodeEntityIdx = NodeRemoval->NodeEntityIdx;
			QuadtreeNodeEntity* NodeEntity = NodeEntities + NodeEntityIdx;

			uint32_t PrevNodeEntityIdx = NodeRemoval->PrevNodeEntityIdx;
			QuadtreeNodeEntity* PrevNodeEntity = NodeEntities + PrevNodeEntityIdx;

			if(PrevNodeEntityIdx)
			{
				PrevNodeEntity->Next = NodeEntity->Next;
			}
			else
			{
				Node->Head = NodeEntity->Next;
			}

			--Node->Count;

			NodeEntity->Next = FreeNodeEntity;
			FreeNodeEntity = NodeEntityIdx;

			--NodeRemoval;
		}

		AllocFree(sizeof(*NodeRemovals) * QT->NodeRemovalsSize, NodeRemovals);
		QT->NodeRemovals = NULL;
		QT->NodeRemovalsUsed = 0;
		QT->NodeRemovalsSize = 0;
	}


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

				while(NodeEntityIdx)
				{
					QuadtreeNodeEntity* NodeEntity = NodeEntities + NodeEntityIdx;

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

						NodeEntity->Next = FreeNodeEntity;
						FreeNodeEntity = NodeEntityIdx;

						break;
					}

					PrevNodeEntity = NodeEntity;
					NodeEntityIdx = NodeEntity->Next;
				}
			}
			while(NodeInfo != NodeInfos);

			Entity->Next = FreeEntity;
			FreeEntity = EntityIdx;

			++Removal;
		}

		AllocFree(sizeof(*Removals) * QT->RemovalsSize, Removals);
		QT->Removals = NULL;
		QT->RemovalsUsed = 0;
		QT->RemovalsSize = 0;
	}


	{
		QuadtreeReinsertion* Reinsertions = QT->Reinsertions;
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
				QuadtreeNode* Node = Nodes + Info.NodeIdx;

				if(Node->Count == -1)
				{
					QuadtreeDescend(EntityExtent);
					continue;
				}

				uint32_t NodeEntityIdx = Node->Head;
				QuadtreeNodeEntity* NodeEntity;

				while(NodeEntityIdx)
				{
					NodeEntity = NodeEntities + NodeEntityIdx;

					if(NodeEntity->Entity == EntityIdx)
					{
						goto goto_skip;
					}

					NodeEntityIdx = NodeEntity->Next;
				}

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
						uint32_t NewSize = (NodeEntitiesUsed << 1) | 1;

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

				goto_skip:;
			}
			while(NodeInfo != NodeInfos);

			++Reinsertion;
		}

		AllocFree(sizeof(*Reinsertions) * QT->ReinsertionsSize, Reinsertions);
		QT->Reinsertions = NULL;
		QT->ReinsertionsUsed = 0;
		QT->ReinsertionsSize = 0;
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
					uint32_t NewSize = (EntitiesUsed << 1) | 1;

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
				QuadtreeNode* Node = Nodes + Info.NodeIdx;

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
						uint32_t NewSize = (NodeEntitiesUsed << 1) | 1;

						NodeEntities = AllocRemalloc(sizeof(*NodeEntities) * NodeEntitiesSize,
							NodeEntities, sizeof(*NodeEntities) * NewSize);
						AssertNEQ(NodeEntities, NULL);

						NodeEntitiesSize = NewSize;
					}

					NodeEntityIdx = NodeEntitiesUsed++;
					NodeEntity = NodeEntities + NodeEntityIdx;
				}

				NodeEntityIdx = NodeEntitiesUsed++;
				NodeEntity = NodeEntities + NodeEntityIdx;

				NodeEntity->Next = Node->Head;
				NodeEntity->Entity = EntityIdx;
				Node->Head = NodeEntityIdx;

				++Node->Count;
			}
			while(NodeInfo != NodeInfos);

			++Insertion;
		}

		AllocFree(sizeof(*Insertions) * QT->InsertionsSize, Insertions);
		QT->Insertions = NULL;
		QT->InsertionsUsed = 0;
		QT->InsertionsSize = 0;
	}


	{
		uint32_t FreeNode = 0; /* Not reflected in QT->FreeNode */
		uint32_t NodesUsed = QT->NodesUsed;
		uint32_t NodesSize = QT->NodesSize;

		QuadtreeNode* NewNodes;
		QuadtreeNodeEntity* NewNodeEntities;
		QuadtreeEntity* NewEntities;

		uint32_t NewNodesUsed = 0;
		uint32_t NewNodesSize;

		if(NodesSize >> 2 < NodesUsed)
		{
			NewNodesSize = NodesSize;
		}
		else
		{
			NewNodesSize = NodesSize >> 1;
		}

		uint32_t NewNodeEntitiesUsed = 1;
		uint32_t NewNodeEntitiesSize;

		if(NodeEntitiesSize >> 2 < NodeEntitiesUsed)
		{
			NewNodeEntitiesSize = NodeEntitiesSize;
		}
		else
		{
			NewNodeEntitiesSize = NodeEntitiesSize >> 1;
		}

		uint32_t NewEntitiesUsed = 1;
		uint32_t NewEntitiesSize;

		if(EntitiesSize >> 2 < EntitiesUsed)
		{
			NewEntitiesSize = EntitiesSize;
		}
		else
		{
			NewEntitiesSize = EntitiesSize >> 1;
		}

		NewNodes = AllocMalloc(sizeof(*NewNodes) * NewNodesSize);
		AssertNEQ(NewNodes, NULL);

		NewNodeEntities = AllocMalloc(sizeof(*NewNodeEntities) * NewNodeEntitiesSize);
		AssertNEQ(NewNodeEntities, NULL);

		NewEntities = AllocMalloc(sizeof(*NewEntities) * NewEntitiesSize);
		AssertNEQ(NewEntities, NULL);

		uint32_t* EntityMap = AllocCalloc(sizeof(*EntityMap) * EntitiesSize);
		AssertNEQ(EntityMap, NULL);


		typedef struct QuadtreeNodeReorderInfo
		{
			uint32_t NodeIdx;
			QuadtreeHalfExtent Extent;
			uint32_t ParentNodeIdx;
			uint32_t HeadIdx;
		}
		QuadtreeNodeReorderInfo;

		QuadtreeNodeReorderInfo NodeInfos[QUADTREE_DFS_LENGTH];
		QuadtreeNodeReorderInfo* NodeInfo = NodeInfos;

		*(NodeInfo++) =
		(QuadtreeNodeReorderInfo)
		{
			.NodeIdx = 0,
			.Extent = QT->HalfExtent,
			.ParentNodeIdx = 0,
			.HeadIdx = 0
		};

		do
		{
			QuadtreeNodeReorderInfo Info = *(--NodeInfo);
			QuadtreeNode* Node = Nodes + Info.NodeIdx;

			uint32_t NewNodeIdx = NewNodesUsed++;
			QuadtreeNode* NewNode = NewNodes + NewNodeIdx;

			NewNode->PositionFlags = Node->PositionFlags;
			NewNodes[Info.ParentNodeIdx].Heads[Info.HeadIdx] = NewNodeIdx;

			if(Node->Count == -1)
			{//TODO instead of node entity removals do full removal followed by fresh insertion, then update count at every parent
				uint32_t Total = 0;
				uint32_t MaxIdx = 0;
				uint32_t MaxCount = 0;
				bool Possible = true;

				for(int i = 0; i < 4; ++i)
				{
					uint32_t NodeIdx = Node->Heads[i];
					QuadtreeNode* Node = Nodes + NodeIdx;

					if(Node->Count == -1)
					{
						Possible = false;
						break;
					}

					if(Node->Count > MaxCount)
					{
						MaxCount = Node->Count;
						MaxIdx = i;
					}

					Total += Node->Count;
				}

				if(!Possible || Total > QUADTREE_MERGE_THRESHOLD)
				{
					goto goto_parent;
				}

				uint32_t Heads[4];
				memcpy(Heads, Node->Heads, sizeof(Heads));

				QuadtreeNode* Children[4];
				for(int i = 0; i < 4; ++i)
				{
					Children[i] = Nodes + Heads[i];
				}

				Node->PositionFlags = Children[0]->PositionFlags;
				Node->PositionFlags |= Children[1]->PositionFlags;
				Node->PositionFlags |= Children[2]->PositionFlags;
				Node->PositionFlags |= Children[3]->PositionFlags;

				QuadtreeNode* MaxChild = Nodes + Heads[MaxIdx];
				Node->Count = MaxChild->Count;
				Node->Head = MaxChild->Head;

				if(Node->Count != 0 && Node->Count != Total)
				{
					for(int i = 0; i < 4; ++i)
					{
						if(i == MaxIdx)
						{
							continue;
						}

						uint32_t ChildIdx = Heads[i];
						QuadtreeNode* Child = Children[i];

						Node->PositionFlags |= Child->PositionFlags;

						uint32_t NodeEntityIdx = Child->Head;
						while(NodeEntityIdx)
						{
							QuadtreeNodeEntity* NodeEntity = NodeEntities + NodeEntityIdx;

							QuadtreeNodeEntity* OtherNodeEntity = NodeEntities + Node->Head;
							while(1)
							{
								if(NodeEntity->Entity == OtherNodeEntity->Entity)
								{
									//FreeNodeEntity = NodeEntityIdx;
									NodeEntityIdx = NodeEntity->Next;
									//NodeEntity->Next = FreeNodeEntity;

									break;
								}

								if(!OtherNodeEntity->Next)
								{
									OtherNodeEntity->Next = NodeEntityIdx;
									NodeEntityIdx = NodeEntity->Next;
									NodeEntity->Next = 0;

									++Node->Count;

									break;
								}

								OtherNodeEntity = NodeEntities + OtherNodeEntity->Next;
							}
						}

						FreeNode = ChildIdx;
						Child->Next = FreeNode;
					}
				}

				goto goto_leaf;

				goto_parent:;

				float HalfW = Info.Extent.W * 0.5f;
				float HalfH = Info.Extent.H * 0.5f;

				*(NodeInfo++) =
				(QuadtreeNodeReorderInfo)
				{
					.NodeIdx = Node->Heads[0],
					.Extent =
					(QuadtreeHalfExtent)
					{
						.X = Info.Extent.X - HalfW,
						.Y = Info.Extent.Y - HalfH,
						.W = HalfW,
						.H = HalfH
					},
					.ParentNodeIdx = NewNodeIdx,
					.HeadIdx = 0
				};

				*(NodeInfo++) =
				(QuadtreeNodeReorderInfo)
				{
					.NodeIdx = Node->Heads[1],
					.Extent =
					(QuadtreeHalfExtent)
					{
						.X = Info.Extent.X - HalfW,
						.Y = Info.Extent.Y + HalfH,
						.W = HalfW,
						.H = HalfH
					},
					.ParentNodeIdx = NewNodeIdx,
					.HeadIdx = 1
				};

				*(NodeInfo++) =
				(QuadtreeNodeReorderInfo)
				{
					.NodeIdx = Node->Heads[2],
					.Extent =
					(QuadtreeHalfExtent)
					{
						.X = Info.Extent.X + HalfW,
						.Y = Info.Extent.Y - HalfH,
						.W = HalfW,
						.H = HalfH
					},
					.ParentNodeIdx = NewNodeIdx,
					.HeadIdx = 2
				};

				*(NodeInfo++) =
				(QuadtreeNodeReorderInfo)
				{
					.NodeIdx = Node->Heads[3],
					.Extent =
					(QuadtreeHalfExtent)
					{
						.X = Info.Extent.X + HalfW,
						.Y = Info.Extent.Y + HalfH,
						.W = HalfW,
						.H = HalfH
					},
					.ParentNodeIdx = NewNodeIdx,
					.HeadIdx = 3
				};

				NewNode->Count = -1;

				continue;
			}

			if(
				Node->Count >= QUADTREE_SPLIT_THRESHOLD &&
				Info.Extent.W >= QT->MinSize &&
				Info.Extent.H >= QT->MinSize
				)
			{
				QuadtreeNode* NodeHeads[4];

				uint32_t BaseIdx;

				if(FreeNode)
				{
					BaseIdx = FreeNode;
					FreeNode = Nodes[BaseIdx].Next;
				}
				else
				{
					if(NodesUsed + 4 > NodesSize)
					{
						uint32_t NewSize = (NodesUsed << 1) | 1;
						if(NewSize < 7) /* If Used was 1 */
						{
							NewSize = 7;
						}

						Nodes = AllocRemalloc(sizeof(*Nodes) * NodesSize,
							Nodes, sizeof(*Nodes) * NewSize);
						AssertNEQ(Nodes, NULL);

						NodesSize = NewSize;

						Node = Nodes + Info.NodeIdx;


						if(NewSize > NewNodesSize)
						{
							NewNodes = AllocRemalloc(sizeof(*Nodes) * NewNodesSize,
								NewNodes, sizeof(*Nodes) * NewSize);
							AssertNEQ(NewNodes, NULL);

							NewNodesSize = NewSize;

							NewNode = NewNodes + NewNodeIdx;
						}
					}

					BaseIdx = NodesUsed;
					NodesUsed += 4;
				}

				uint32_t Head = Node->Head;

				for(int i = 0; i < 4; ++i)
				{
					uint32_t NewNodeIdx = BaseIdx + i;
					QuadtreeNode* NewNode = Nodes + NewNodeIdx;

					Node->Heads[i] = NewNodeIdx;
					NodeHeads[i] = NewNode;

					NewNode->Head = 0;
					NewNode->Count = 0;

					static const uint32_t PositionFlags[4] =
					{
						0b0011, /* TR */
						0b1001, /* TL */
						0b0110, /* BR */
						0b1100  /* BL */
					};

					NewNode->PositionFlags = Node->PositionFlags & PositionFlags[i];
				}

				uint32_t NodeEntityIdx = Head;
				while(NodeEntityIdx)
				{
					QuadtreeNodeEntity* NodeEntity = NodeEntities + NodeEntityIdx;

					uint32_t EntityIdx = NodeEntity->Entity;
					QuadtreeEntity* Entity = Entities + EntityIdx;

					QuadtreeRectExtent EntityExtent = QuadtreeGetEntityRectExtent(Entity);

					uint32_t TargetNodeIdxs[4];
					uint32_t* CurrentTargetNodeIdx = TargetNodeIdxs;

					if(EntityExtent.MinX <= Info.Extent.X)
					{
						if(EntityExtent.MinY <= Info.Extent.Y)
						{
							*(CurrentTargetNodeIdx++) = 0;
						}
						if(EntityExtent.MaxY >= Info.Extent.Y)
						{
							*(CurrentTargetNodeIdx++) = 1;
						}
					}
					if(EntityExtent.MaxX >= Info.Extent.X)
					{
						if(EntityExtent.MinY <= Info.Extent.Y)
						{
							*(CurrentTargetNodeIdx++) = 2;
						}
						if(EntityExtent.MaxY >= Info.Extent.Y)
						{
							*(CurrentTargetNodeIdx++) = 3;
						}
					}

					for(uint32_t* TargetNodeIdx = TargetNodeIdxs; TargetNodeIdx != CurrentTargetNodeIdx; ++TargetNodeIdx)
					{
						QuadtreeNode* TargetNode = NodeHeads[*TargetNodeIdx];

						uint32_t NewNodeEntityIdx;
						QuadtreeNodeEntity* NewNodeEntity;

						if(FreeNodeEntity)
						{
							NewNodeEntityIdx = FreeNodeEntity;
							NewNodeEntity = NodeEntities + NewNodeEntityIdx;
							FreeNodeEntity = NewNodeEntity->Next;
						}
						else
						{
							if(NodeEntitiesUsed >= NodeEntitiesSize)
							{
								uint32_t NewSize = (NodeEntitiesUsed << 1) | 1;

								NodeEntities = AllocRemalloc(sizeof(*NodeEntities) * NodeEntitiesSize,
									NodeEntities, sizeof(*NodeEntities) * NewSize);
								AssertNEQ(NodeEntities, NULL);

								NodeEntitiesSize = NewSize;

								NodeEntity = NodeEntities + NodeEntityIdx;


								if(NewSize > NewNodeEntitiesSize)
								{
									NewNodeEntities = AllocRemalloc(sizeof(*NewNodeEntities) * NewNodeEntitiesSize,
										NewNodeEntities, sizeof(*NewNodeEntities) * NewSize);
									AssertNEQ(NewNodeEntities, NULL);

									NewNodeEntitiesSize = NewSize;
								}
							}

							NewNodeEntityIdx = NodeEntitiesUsed++;
							NewNodeEntity = NodeEntities + NewNodeEntityIdx;
						}

						NewNodeEntity->Next = TargetNode->Head;
						NewNodeEntity->Entity = EntityIdx;
						TargetNode->Head = NewNodeEntityIdx;

						++TargetNode->Count;
					}

					uint32_t NextNodeEntityIdx = NodeEntity->Next;

					NodeEntity->Next = FreeNodeEntity;
					FreeNodeEntity = NodeEntityIdx;

					NodeEntityIdx = NextNodeEntityIdx;
				}

				Node->Count = -1;

				goto goto_parent;
			}

			goto_leaf:

			if(!Node->Head)
			{
				NewNode->Head = 0;
				NewNode->Count = 0;

				continue;
			}

			uint32_t NodeEntityIdx = Node->Head;

			NewNode->Head = NewNodeEntitiesUsed;
			NewNode->Count = Node->Count;

			while(1)
			{
				QuadtreeNodeEntity* NodeEntity = NodeEntities + NodeEntityIdx;
				QuadtreeNodeEntity* NewNodeEntity = NewNodeEntities + NewNodeEntitiesUsed;
				++NewNodeEntitiesUsed;

				uint32_t EntityIdx = NodeEntity->Entity;
				if(!EntityMap[EntityIdx])
				{
					uint32_t NewEntityIdx = NewEntitiesUsed++;
					EntityMap[EntityIdx] = NewEntityIdx;
					NewEntities[NewEntityIdx] = Entities[EntityIdx];
				}

				NewNodeEntity->Entity = EntityMap[EntityIdx];

				if(NodeEntity->Next)
				{
					NodeEntityIdx = NodeEntity->Next;
					NewNodeEntity->Next = NewNodeEntitiesUsed;
				}
				else
				{
					NewNodeEntity->Next = 0;
					break;
				}
			}
		}
		while(NodeInfo != NodeInfos);

		AllocFree(sizeof(*Nodes) * NodesSize, Nodes);
		QT->Nodes = NewNodes;
		QT->NodesUsed = NewNodesUsed;
		QT->NodesSize = NewNodesSize;

		AllocFree(sizeof(*NodeEntities) * NodeEntitiesSize, NodeEntities);
		QT->NodeEntities = NewNodeEntities;
		QT->NodeEntitiesUsed = NewNodeEntitiesUsed;
		QT->NodeEntitiesSize = NewNodeEntitiesSize;

		AllocFree(sizeof(*Entities) * EntitiesSize, Entities);
		QT->Entities = NewEntities;
		QT->EntitiesUsed = NewEntitiesUsed;
		QT->EntitiesSize = NewEntitiesSize;

		AllocFree(sizeof(*EntityMap) * EntitiesSize, EntityMap);
	}
}


void
QuadtreeUpdate(
	Quadtree* QT,
	QuadtreeUpdateT Callback
	)
{
	QT->UpdateTick ^= 1;
	uint32_t UpdateTick = QT->UpdateTick;

	QuadtreeNode* Nodes = QT->Nodes;
	QuadtreeNodeEntity* NodeEntities = QT->NodeEntities;
	QuadtreeEntity* Entities = QT->Entities;
	QuadtreeReinsertion* Reinsertions = QT->Reinsertions;
	QuadtreeNodeRemoval* NodeRemovals = QT->NodeRemovals;

	uint32_t ReinsertionsUsed = QT->ReinsertionsUsed;
	uint32_t ReinsertionsSize = QT->ReinsertionsSize;

	uint32_t NodeRemovalsUsed = QT->NodeRemovalsUsed;
	uint32_t NodeRemovalsSize = QT->NodeRemovalsSize;

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
			QuadtreeDescend(((QuadtreeRectExtent){ Info.Extent.X, Info.Extent.X, Info.Extent.Y, Info.Extent.Y }));
			continue;
		}

		QuadtreeRectExtent NodeExtent = QuadtreeHalfToRectExtent(Info.Extent);

		uint32_t PrevIdx = 0;
		uint32_t Idx = Node->Head;

		while(Idx)
		{
			QuadtreeNodeEntity* NodeEntity = NodeEntities + Idx;

			uint32_t EntityIdx = NodeEntity->Entity;
			QuadtreeEntity* Entity = Entities + EntityIdx;

			QuadtreeRectExtent Extent;

			if(Entity->UpdateTick != UpdateTick)
			{
				Entity->UpdateTick = UpdateTick;
				QuadtreeStatus Status = Callback(QT, EntityIdx, &Entity->Data);
				Extent = QuadtreeGetEntityRectExtent(Entity);

				if(Status == QUADTREE_STATUS_CHANGED && !QuadtreeIsInside(Extent, NodeExtent))
				{
					uint32_t ReinsertionIdx;
					QuadtreeReinsertion* Reinsertion;

					if(ReinsertionsUsed >= ReinsertionsSize)
					{
						uint32_t NewSize = (ReinsertionsUsed << 1) | 1;

						Reinsertions = AllocRemalloc(sizeof(*Reinsertions) * ReinsertionsSize,
							Reinsertions, sizeof(*Reinsertions) * NewSize);
						AssertNEQ(Reinsertions, NULL);

						ReinsertionsSize = NewSize;
					}

					ReinsertionIdx = ReinsertionsUsed++;
					Reinsertion = Reinsertions + ReinsertionIdx;

					Reinsertion->EntityIdx = EntityIdx;
				}
			}
			else
			{
				Extent = QuadtreeGetEntityRectExtent(Entity);
			}

			if(
				(Extent.MaxX < NodeExtent.MinX && !(Node->PositionFlags & 0b0001)) ||
				(Extent.MaxY < NodeExtent.MinY && !(Node->PositionFlags & 0b0010)) ||
				(NodeExtent.MaxX < Extent.MinX && !(Node->PositionFlags & 0b0100)) ||
				(NodeExtent.MaxY < Extent.MinY && !(Node->PositionFlags & 0b1000))
				)
			{
				uint32_t NodeRemovalIdx;
				QuadtreeNodeRemoval* NodeRemoval;

				if(NodeRemovalsUsed >= NodeRemovalsSize)
				{
					uint32_t NewSize = (NodeRemovalsUsed << 1) | 1;

					NodeRemovals = AllocRemalloc(sizeof(*NodeRemovals) * NodeRemovalsSize,
						NodeRemovals, sizeof(*NodeRemovals) * NewSize);
					AssertNEQ(NodeRemovals, NULL);

					NodeRemovalsSize = NewSize;
				}

				NodeRemovalIdx = NodeRemovalsUsed++;
				NodeRemoval = NodeRemovals + NodeRemovalIdx;

				NodeRemoval->NodeIdx = Info.NodeIdx;
				NodeRemoval->NodeEntityIdx = Idx;
				NodeRemoval->PrevNodeEntityIdx = PrevIdx;
			}

			PrevIdx = Idx;
			Idx = NodeEntity->Next;
		}
	}
	while(NodeInfo != NodeInfos);

	QT->Reinsertions = Reinsertions;
	QT->ReinsertionsUsed = ReinsertionsUsed;
	QT->ReinsertionsSize = ReinsertionsSize;

	QT->NodeRemovals = NodeRemovals;
	QT->NodeRemovalsUsed = NodeRemovalsUsed;
	QT->NodeRemovalsSize = NodeRemovalsSize;
}


void
QuadtreeQuery(
	Quadtree* QT,
	QuadtreeRectExtent Extent,
	QuadtreeQueryT Callback
	)
{
	++QT->QueryTick;
	uint32_t QueryTick = QT->QueryTick;

	QuadtreeNode* Nodes = QT->Nodes;
	QuadtreeNodeEntity* NodeEntities = QT->NodeEntities;
	QuadtreeEntity* Entities = QT->Entities;

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
		while(Idx)
		{
			QuadtreeNodeEntity* NodeEntity = NodeEntities + Idx;
			uint32_t EntityIdx = NodeEntity->Entity;
			QuadtreeEntity* Entity = Entities + EntityIdx;

			if(Entity->QueryTick != QueryTick)
			{
				Entity->QueryTick = QueryTick;

				if(QuadtreeIntersects(QuadtreeGetEntityRectExtent(Entity), Extent))
				{
					Callback(QT, EntityIdx, &Entity->Data);
				}
			}

			Idx = NodeEntity->Next;
		}
	}
	while(NodeInfo != NodeInfos);
}


void
QuadtreeQueryNodes(
	Quadtree* QT,
	QuadtreeRectExtent Extent,
	QuadtreeNodeQueryT Callback
	)
{
	QuadtreeNode* Nodes = QT->Nodes;

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

		Callback(QT, &Info);
	}
	while(NodeInfo != NodeInfos);
}


void
QuadtreeCollideSlow(
	Quadtree* QT,
	QuadtreeCollideT Callback
	)
{
	QuadtreeNode* Node = QT->Nodes;
	QuadtreeNode* EndNode = QT->Nodes + QT->NodesUsed;

#if QUADTREE_DEDUPE_COLLISIONS == 1
	uint32_t HashTableSize = QT->EntitiesUsed * QUADTREE_HASH_TABLE_FACTOR;
	uint32_t* HashTable = AllocCalloc(sizeof(*HashTable) * HashTableSize);
	AssertNEQ(HashTable, NULL);

	QuadtreeHTEntry* HTEntries = QT->HTEntries;

	uint32_t HTEntriesUsed = 1;
	uint32_t HTEntriesSize = QT->HTEntriesSize;
#endif

	QuadtreeNodeEntity* NodeEntities = QT->NodeEntities;
	QuadtreeEntity* Entities = QT->Entities;

	while(Node != EndNode)
	{
		if(Node->Count == -1 || !Node->Head)
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
					Entry = HTEntries + Index;

					if(Entry->Idx[0] == IndexA && Entry->Idx[1] == IndexB)
					{
						goto goto_skip;
					}

					Index = Entry->Next;
				}

				if(HTEntriesUsed >= HTEntriesSize)
				{
					uint32_t NewSize = (HTEntriesUsed << 1) | 1;

					HTEntries = AllocRemalloc(sizeof(*HTEntries) * HTEntriesSize,
						HTEntries, sizeof(*HTEntries) * NewSize);
					AssertNEQ(HTEntries, NULL);

					HTEntriesSize = NewSize;
				}

				uint32_t EntryIdx = HTEntriesUsed++;
				Entry = HTEntries + EntryIdx;

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
	QT->HTEntries = HTEntries;
	QT->HTEntriesSize = HTEntriesSize;

	AllocFree(sizeof(*HashTable) * HashTableSize, HashTable);
#endif
}


void
QuadtreeCollideFast(
	Quadtree* QT,
	QuadtreeCollideT Callback
	)
{
	QuadtreeNode* Node = QT->Nodes;
	QuadtreeNode* EndNode = QT->Nodes + QT->NodesUsed;

#if QUADTREE_DEDUPE_COLLISIONS == 1
	uint32_t HashTableSize = QT->EntitiesUsed * QUADTREE_HASH_TABLE_FACTOR;
	uint32_t* HashTable = AllocCalloc(sizeof(*HashTable) * HashTableSize);
	AssertNEQ(HashTable, NULL);

	QuadtreeHTEntry* HTEntries = QT->HTEntries;

	uint32_t HTEntriesUsed = 1;
	uint32_t HTEntriesSize = QT->HTEntriesSize;
#endif

	QuadtreeNodeEntity* NodeEntities = QT->NodeEntities;
	QuadtreeEntity* Entities = QT->Entities;

	while(Node != EndNode)
	{
		if(Node->Count == -1 || !Node->Head)
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
					Entry = HTEntries + Index;

					if(Entry->Idx[0] == IndexA && Entry->Idx[1] == IndexB)
					{
						goto goto_skip;
					}

					Index = Entry->Next;
				}

				if(HTEntriesUsed >= HTEntriesSize)
				{
					uint32_t NewSize = (HTEntriesUsed << 1) | 1;

					HTEntries = AllocRemalloc(sizeof(*HTEntries) * HTEntriesSize,
						HTEntries, sizeof(*HTEntries) * NewSize);
					AssertNEQ(HTEntries, NULL);

					HTEntriesSize = NewSize;
				}

				uint32_t EntryIdx = HTEntriesUsed++;
				Entry = HTEntries + EntryIdx;

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
	QT->HTEntries = HTEntries;
	QT->HTEntriesSize = HTEntriesSize;

	AllocFree(sizeof(*HashTable) * HashTableSize, HashTable);
#endif
}


#undef QuadtreeDescend

