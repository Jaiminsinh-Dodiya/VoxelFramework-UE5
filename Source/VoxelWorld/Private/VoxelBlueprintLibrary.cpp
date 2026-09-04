// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelBlueprintLibrary.h"
#include "VoxelWorldSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UVoxelWorldSubsystem* UVoxelBlueprintLibrary::GetVoxelWorldSubsystem(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UVoxelWorldSubsystem>();
}

FIntVector UVoxelBlueprintLibrary::WorldPositionToChunkCoordinate(const UObject* WorldContextObject, const FVector& WorldPosition)
{
	if (const UVoxelWorldSubsystem* Subsystem = GetVoxelWorldSubsystem(WorldContextObject))
	{
		return Subsystem->WorldPositionToChunkCoordinate(WorldPosition);
	}
	return FIntVector::ZeroValue;
}

bool UVoxelBlueprintLibrary::TryGetBlockAtWorldPosition(const UObject* WorldContextObject, const FVector& WorldPosition, int32& OutBlockId)
{
	OutBlockId = 0;
	if (const UVoxelWorldSubsystem* Subsystem = GetVoxelWorldSubsystem(WorldContextObject))
	{
		return Subsystem->TryGetBlockAtWorldPosition(WorldPosition, OutBlockId);
	}
	return false;
}

bool UVoxelBlueprintLibrary::TryIsSolidAtWorldPosition(const UObject* WorldContextObject, const FVector& WorldPosition, bool& bOutIsSolid)
{
	bOutIsSolid = false;
	if (const UVoxelWorldSubsystem* Subsystem = GetVoxelWorldSubsystem(WorldContextObject))
	{
		return Subsystem->TryIsSolidAtWorldPosition(WorldPosition, bOutIsSolid);
	}
	return false;
}

bool UVoxelBlueprintLibrary::IsChunkLoaded(const UObject* WorldContextObject, const FIntVector& ChunkCoord)
{
	if (const UVoxelWorldSubsystem* Subsystem = GetVoxelWorldSubsystem(WorldContextObject))
	{
		return Subsystem->IsChunkLoaded(ChunkCoord);
	}
	return false;
}

bool UVoxelBlueprintLibrary::IsChunkCollisionReady(const UObject* WorldContextObject, const FIntVector& ChunkCoord)
{
	if (const UVoxelWorldSubsystem* Subsystem = GetVoxelWorldSubsystem(WorldContextObject))
	{
		return Subsystem->IsChunkCollisionReady(ChunkCoord);
	}
	return false;
}

int32 UVoxelBlueprintLibrary::GetChunkSize(const UObject* WorldContextObject)
{
	if (const UVoxelWorldSubsystem* Subsystem = GetVoxelWorldSubsystem(WorldContextObject))
	{
		return Subsystem->GetChunkSize();
	}
	return 32;
}

int32 UVoxelBlueprintLibrary::GetWorldSeed(const UObject* WorldContextObject)
{
	if (const UVoxelWorldSubsystem* Subsystem = GetVoxelWorldSubsystem(WorldContextObject))
	{
		return Subsystem->GetWorldSeed();
	}
	return 0;
}

float UVoxelBlueprintLibrary::GetVoxelWorldSize(const UObject* WorldContextObject)
{
	if (const UVoxelWorldSubsystem* Subsystem = GetVoxelWorldSubsystem(WorldContextObject))
	{
		return Subsystem->GetVoxelWorldSize();
	}
	return 100.0f;
}

bool UVoxelBlueprintLibrary::IsWorldInitialized(const UObject* WorldContextObject)
{
	if (const UVoxelWorldSubsystem* Subsystem = GetVoxelWorldSubsystem(WorldContextObject))
	{
		return Subsystem->IsWorldInitialized();
	}
	return false;
}

void UVoxelBlueprintLibrary::ApplyWorldDefinition(const UObject* WorldContextObject, const UVoxelWorldDefinition* WorldDefinition)
{
	if (UVoxelWorldSubsystem* Subsystem = GetVoxelWorldSubsystem(WorldContextObject))
	{
		Subsystem->ApplyWorldDefinition(WorldDefinition);
	}
}
