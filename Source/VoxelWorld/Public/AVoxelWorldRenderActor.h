// AVoxelWorldRenderActor.h
//
// Purpose:
//   Every UVoxelMeshComponent needs an owning AActor - UE components can't
//   exist without one. Per-chunk-as-Actor is explicitly forbidden by
//   ADR-001, but that ADR is about the CHUNK DATA representation
//   (FVoxelChunk is plain C++, never an Actor) - it says nothing about the
//   rendering attachment point. One shared, lightweight host actor per
//   world, holding one UVoxelMeshComponent per loaded chunk as dynamically
//   added subobjects, is the same pattern VoxelDebug's AVoxelDebugVisualizer
//   already uses (and documents as an ADR-001-adjacent exception) - this
//   formalizes it for production use instead of debug-only use.
//
// Responsibilities: exist, hold components. No gameplay logic, no tick.
// Thread ownership: Game Thread only, created/managed by UVoxelWorldSubsystem.
// Dependencies: Engine (AActor) only.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AVoxelWorldRenderActor.generated.h"

UCLASS(NotBlueprintable, NotPlaceable)
class VOXELWORLD_API AVoxelWorldRenderActor : public AActor
{
	GENERATED_BODY()

public:
	AVoxelWorldRenderActor();
};
