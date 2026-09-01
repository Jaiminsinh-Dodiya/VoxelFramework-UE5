// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelStreamingBlueprintLibrary.h"
#include "VoxelStreamingManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UVoxelStreamingManager* UVoxelStreamingBlueprintLibrary::GetVoxelStreamingManager(const UObject* WorldContextObject)
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

	return World->GetSubsystem<UVoxelStreamingManager>();
}

void UVoxelStreamingBlueprintLibrary::ApplyStreamingPreset(const UObject* WorldContextObject, const UVoxelStreamingPreset* Preset)
{
	if (UVoxelStreamingManager* Manager = GetVoxelStreamingManager(WorldContextObject))
	{
		Manager->ApplyPreset(Preset);
	}
}

void UVoxelStreamingBlueprintLibrary::SetStreamingRenderDistance(const UObject* WorldContextObject, int32 NewRenderDistance)
{
	if (UVoxelStreamingManager* Manager = GetVoxelStreamingManager(WorldContextObject))
	{
		Manager->SetRenderDistance(NewRenderDistance);
	}
}

int32 UVoxelStreamingBlueprintLibrary::GetStreamingRenderDistance(const UObject* WorldContextObject)
{
	if (const UVoxelStreamingManager* Manager = GetVoxelStreamingManager(WorldContextObject))
	{
		return Manager->GetRenderDistance();
	}
	return 8;
}

void UVoxelStreamingBlueprintLibrary::SetStreamingSimulationDistance(const UObject* WorldContextObject, int32 NewSimulationDistance)
{
	if (UVoxelStreamingManager* Manager = GetVoxelStreamingManager(WorldContextObject))
	{
		Manager->SetSimulationDistance(NewSimulationDistance);
	}
}

int32 UVoxelStreamingBlueprintLibrary::GetStreamingSimulationDistance(const UObject* WorldContextObject)
{
	if (const UVoxelStreamingManager* Manager = GetVoxelStreamingManager(WorldContextObject))
	{
		return Manager->GetSimulationDistance();
	}
	return 4;
}

void UVoxelStreamingBlueprintLibrary::SetStreamingBudgetMs(const UObject* WorldContextObject, float NewBudgetMs)
{
	if (UVoxelStreamingManager* Manager = GetVoxelStreamingManager(WorldContextObject))
	{
		Manager->SetStreamingBudgetMs(NewBudgetMs);
	}
}

float UVoxelStreamingBlueprintLibrary::GetStreamingBudgetMs(const UObject* WorldContextObject)
{
	if (const UVoxelStreamingManager* Manager = GetVoxelStreamingManager(WorldContextObject))
	{
		return Manager->GetStreamingBudgetMs();
	}
	return 1.5f;
}

void UVoxelStreamingBlueprintLibrary::SetStreamingFrozen(const UObject* WorldContextObject, bool bFrozen)
{
	if (UVoxelStreamingManager* Manager = GetVoxelStreamingManager(WorldContextObject))
	{
		Manager->SetStreamingFrozen(bFrozen);
	}
}

bool UVoxelStreamingBlueprintLibrary::IsStreamingFrozen(const UObject* WorldContextObject)
{
	if (const UVoxelStreamingManager* Manager = GetVoxelStreamingManager(WorldContextObject))
	{
		return Manager->IsStreamingFrozen();
	}
	return false;
}
