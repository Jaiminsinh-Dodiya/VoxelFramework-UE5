// AVoxelWorldRenderActor.cpp

#include "AVoxelWorldRenderActor.h"

AVoxelWorldRenderActor::AVoxelWorldRenderActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITOR
	SetActorLabel(TEXT("VoxelWorldRenderActor"));
#endif
}
