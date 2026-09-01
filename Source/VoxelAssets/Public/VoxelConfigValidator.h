// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VoxelConfigValidator.generated.h"

class UVoxelWorldDefinition;
class UVoxelGenerationDefinition;
class UVoxelBiomeDefinition;
class UVoxelBlockDefinition;

/**
 * Severity of a validation message.
 */
UENUM(BlueprintType)
enum class EVoxelValidationSeverity : uint8
{
	Info UMETA(DisplayName = "Info"),
	Warning UMETA(DisplayName = "Warning"),
	Error UMETA(DisplayName = "Error")
};

/**
 * A single validation message resulting from checking voxel configuration assets.
 */
USTRUCT(BlueprintType)
struct VOXELASSETS_API FVoxelValidationMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	EVoxelValidationSeverity Severity = EVoxelValidationSeverity::Error;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	FString Suggestion;

	FVoxelValidationMessage() = default;
	
	FVoxelValidationMessage(EVoxelValidationSeverity InSeverity, const FString& InMessage, const FString& InSuggestion = TEXT(""))
		: Severity(InSeverity)
		, Message(InMessage)
		, Suggestion(InSuggestion)
	{
	}
};

/**
 * UVoxelConfigValidator
 * 
 * Central utility for validating voxel configuration assets and reporting issues
 * to designers (e.g. missing references, invalid ranges, duplicate IDs).
 */
UCLASS(BlueprintType)
class VOXELASSETS_API UVoxelConfigValidator : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Validates a World Definition and optionally its referenced definitions.
	 * 
	 * @param WorldDef The world definition to validate.
	 * @param ChunkSize Expected chunk size, used for height bounds checking.
	 * @param WorldHeightInChunks Expected world height in chunks.
	 * @return Array of validation messages.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Validation")
	static TArray<FVoxelValidationMessage> ValidateWorldDefinition(const UVoxelWorldDefinition* WorldDef, int32 ChunkSize = 32, int32 WorldHeightInChunks = 8);

	/**
	 * Validates a Generation Definition.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Validation")
	static TArray<FVoxelValidationMessage> ValidateGenerationDefinition(const UVoxelGenerationDefinition* GenDef, int32 ChunkSize = 32, int32 WorldHeightInChunks = 8);

	/**
	 * Validates a list of Block Definitions, checking for duplicates and reserved IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Validation")
	static TArray<FVoxelValidationMessage> ValidateBlockDefinitions(const TArray<UVoxelBlockDefinition*>& BlockDefs);

	/**
	 * Validates a Biome Definition.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Validation")
	static TArray<FVoxelValidationMessage> ValidateBiomeDefinition(const UVoxelBiomeDefinition* BiomeDef);
};
