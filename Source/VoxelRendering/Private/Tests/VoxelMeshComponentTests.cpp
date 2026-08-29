// VoxelMeshComponentTests.cpp
//
// Deliberately scoped to what's safely testable without a registered
// component in a live scene: SetMeshData/ClearMeshData's bookkeeping
// (material count, bounds). Does NOT call CreateSceneProxy or otherwise
// exercise FVoxelMeshSceneProxy/RHI resource creation - that requires a
// real render scene and isn't something a lightweight editor automation
// test should attempt. Visual/rendering correctness for this module is a
// look-and-verify task (extend VoxelDebug or a test level), not something
// this automation test claims to cover - stated explicitly so nobody reads
// "test passes" as "renders correctly."

#include "Misc/AutomationTest.h"
#include "VoxelMeshComponent.h"
#include "VoxelMeshData.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMeshComponentBookkeepingTest, "Voxel.Rendering.ComponentBookkeeping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelMeshComponentBookkeepingTest::RunTest(const FString& Parameters)
{
	UVoxelMeshComponent* Component = NewObject<UVoxelMeshComponent>(GetTransientPackage());
	TestNotNull(TEXT("Component should construct"), Component);
	if (!Component)
	{
		return false;
	}

	TestEqual(TEXT("A freshly constructed component should report 0 materials"), Component->GetNumMaterials(), 0);

	FVoxelMeshData MeshData;
	FVoxelMeshVertex V0, V1, V2, V3;
	V0.Position = FVector3f(0, 0, 0);
	V1.Position = FVector3f(1, 0, 0);
	V2.Position = FVector3f(1, 1, 0);
	V3.Position = FVector3f(0, 1, 0);
	MeshData.Vertices = { V0, V1, V2, V3 };
	MeshData.Bounds = FBox(FVector(0, 0, 0), FVector(1, 1, 0));

	FVoxelMeshSection Section;
	Section.MaterialId = 5;
	Section.Indices = { 0, 1, 2, 0, 2, 3 };
	MeshData.Sections.Add(Section);

	Component->SetMeshData(MoveTemp(MeshData));

	TestEqual(TEXT("One section should mean one reported material"), Component->GetNumMaterials(), 1);

	const FBoxSphereBounds Bounds = Component->CalcBounds(FTransform::Identity);
	TestTrue(TEXT("Bounds extent should be non-zero after setting real geometry"), Bounds.BoxExtent.SizeSquared() > 0.0f);

	Component->ClearMeshData();
	TestEqual(TEXT("After ClearMeshData, material count should return to 0"), Component->GetNumMaterials(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
