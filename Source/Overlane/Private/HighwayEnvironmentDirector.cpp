#include "HighwayEnvironmentDirector.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float VisualRoadZ = 2.0f;
    constexpr float CubeSize = 100.0f;
    constexpr float VisualRouteOverscan = 15000.0f;

    float CalculateUniformScaleForHeight(const UStaticMesh* Mesh, float DesiredHeight)
    {
        if (!Mesh)
        {
            return 1.0f;
        }

        const float SourceHeight = Mesh->GetBounds().BoxExtent.Z * 2.0f;
        return DesiredHeight / FMath::Max(SourceHeight, 1.0f);
    }

    FVector CalculateGroundedInstanceLocation(const UStaticMesh* Mesh, float UniformScale, float X, float Y, float GroundZ = VisualRoadZ)
    {
        if (!Mesh)
        {
            return FVector(X, Y, GroundZ);
        }

        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const float MinimumZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * UniformScale;
        return FVector(X, Y, GroundZ - MinimumZ);
    }
}

AHighwayEnvironmentDirector::AHighwayEnvironmentDirector()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);
    SetActorHiddenInGame(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    // Deliberately keeps NO overrides.
    //
    // This component used to force AutoExposureBias and BloomIntensity while
    // being unbound at Priority 1000, which outranks any PostProcessVolume placed
    // in the level. Art-directing the game from a volume -- the only way to tune
    // the look without a recompile -- would have silently failed for exposure and
    // bloom. The component is kept as the hook for effects that genuinely have to
    // come from code later; everything authored belongs in the level's volume.
    VisualPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("VisualPostProcess"));
    VisualPostProcess->SetupAttachment(SceneRoot);
    VisualPostProcess->bUnbound = true;
    VisualPostProcess->Priority = -1.0f;

    LeftLandscape = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLandscape"));
    LeftLandscape->SetupAttachment(SceneRoot);
    RightLandscape = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLandscape"));
    RightLandscape->SetupAttachment(SceneRoot);
    RoadSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoadSurface"));
    RoadSurface->SetupAttachment(SceneRoot);
    LeftShoulder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftShoulder"));
    LeftShoulder->SetupAttachment(SceneRoot);
    RightShoulder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightShoulder"));
    RightShoulder->SetupAttachment(SceneRoot);
    LeftGuardRail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftGuardRail"));
    LeftGuardRail->SetupAttachment(SceneRoot);
    RightGuardRail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightGuardRail"));
    RightGuardRail->SetupAttachment(SceneRoot);

    LaneDashes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LaneDashes"));
    LaneDashes->SetupAttachment(SceneRoot);
    EdgeReflectors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("EdgeReflectors"));
    EdgeReflectors->SetupAttachment(SceneRoot);
    LampPoles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LampPoles"));
    LampPoles->SetupAttachment(SceneRoot);
    LampArms = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LampArms"));
    LampArms->SetupAttachment(SceneRoot);
    LampHeads = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LampHeads"));
    LampHeads->SetupAttachment(SceneRoot);
    SignPosts = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SignPosts"));
    SignPosts->SetupAttachment(SceneRoot);
    SignBoards = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SignBoards"));
    SignBoards->SetupAttachment(SceneRoot);
    DistantHills = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DistantHills"));
    DistantHills->SetupAttachment(SceneRoot);
    HeroStreetLights = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HeroStreetLights"));
    HeroStreetLights->SetupAttachment(SceneRoot);
    HeroTrees = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HeroTrees"));
    HeroTrees->SetupAttachment(SceneRoot);
    HeroBuildings = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HeroBuildings"));
    HeroBuildings->SetupAttachment(SceneRoot);
    HeroServicePads = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HeroServicePads"));
    HeroServicePads->SetupAttachment(SceneRoot);
    HeroServiceCanopies = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HeroServiceCanopies"));
    HeroServiceCanopies->SetupAttachment(SceneRoot);
    HeroServiceColumns = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HeroServiceColumns"));
    HeroServiceColumns->SetupAttachment(SceneRoot);
    HeroFuelPumps = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HeroFuelPumps"));
    HeroFuelPumps->SetupAttachment(SceneRoot);
    HeroServiceSigns = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HeroServiceSigns"));
    HeroServiceSigns->SetupAttachment(SceneRoot);
    HeroShowcaseCar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeroShowcaseCar"));
    HeroShowcaseCar->SetupAttachment(SceneRoot);
    InterchangeDecks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InterchangeDecks"));
    InterchangeDecks->SetupAttachment(SceneRoot);
    InterchangeColumns = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InterchangeColumns"));
    InterchangeColumns->SetupAttachment(SceneRoot);
    InterchangeRails = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InterchangeRails"));
    InterchangeRails->SetupAttachment(SceneRoot);
    InterchangeSigns = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InterchangeSigns"));
    InterchangeSigns->SetupAttachment(SceneRoot);
    InterchangeCityBlocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InterchangeCityBlocks"));
    InterchangeCityBlocks->SetupAttachment(SceneRoot);
    InterchangeBuildings = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InterchangeBuildings"));
    InterchangeBuildings->SetupAttachment(SceneRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> StreetLightFinder(TEXT("/Game/Building/Geometry/SM_StreetLight.SM_StreetLight"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> HillTreeFinder(TEXT("/Game/ArchVis/SampleScene/Tree/HillTree_02.HillTree_02"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BuildingFinder(TEXT("/Game/Building/Geometry/SM_Building.SM_Building"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConceptCarFinder(TEXT("/Game/ConceptCar/Car/SM_AutomotiveTP_Car.SM_AutomotiveTP_Car"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    // Generated by Tools/BuildRoadMaterial.py: a world-aligned asphalt material,
    // which is the only kind this geometry can carry. Found rather than required,
    // so the project still builds and runs if it has not been generated yet.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RoadMaterialFinder(
        TEXT("/Game/Environment/Surfaces/MI_OverlaneAsphalt.MI_OverlaneAsphalt"));

    CubeMesh = CubeFinder.Object;
    CylinderMesh = CylinderFinder.Object;
    SphereMesh = SphereFinder.Object;
    ConeMesh = ConeFinder.Object;
    BaseMaterial = MaterialFinder.Object;

    if (RoadMaterialFinder.Succeeded())
    {
        RoadSurfaceMaterial = RoadMaterialFinder.Object;
    }

    for (UStaticMeshComponent* Component : { LeftLandscape, RightLandscape, RoadSurface, LeftShoulder, RightShoulder, LeftGuardRail, RightGuardRail })
    {
        Component->SetStaticMesh(CubeMesh);
        ConfigureVisualMesh(Component);
    }

    LaneDashes->SetStaticMesh(CubeMesh);
    EdgeReflectors->SetStaticMesh(CubeMesh);
    LampPoles->SetStaticMesh(CylinderMesh);
    LampArms->SetStaticMesh(CubeMesh);
    LampHeads->SetStaticMesh(SphereMesh);
    SignPosts->SetStaticMesh(CylinderMesh);
    SignBoards->SetStaticMesh(CubeMesh);
    // A flattened sphere, not a cone: a dome reads as a hill, a cone reads as a
    // cone no matter how it is scaled.
    DistantHills->SetStaticMesh(SphereMesh);
    HeroStreetLights->SetStaticMesh(StreetLightFinder.Object);
    HeroTrees->SetStaticMesh(HillTreeFinder.Object);
    HeroBuildings->SetStaticMesh(BuildingFinder.Object);
    HeroServicePads->SetStaticMesh(CubeMesh);
    HeroServiceCanopies->SetStaticMesh(CubeMesh);
    HeroServiceColumns->SetStaticMesh(CylinderMesh);
    HeroFuelPumps->SetStaticMesh(CubeMesh);
    HeroServiceSigns->SetStaticMesh(CubeMesh);
    HeroShowcaseCar->SetStaticMesh(ConceptCarFinder.Object);
    ConfigureVisualMesh(HeroShowcaseCar);
    InterchangeDecks->SetStaticMesh(CubeMesh);
    InterchangeColumns->SetStaticMesh(CylinderMesh);
    InterchangeRails->SetStaticMesh(CubeMesh);
    InterchangeSigns->SetStaticMesh(CubeMesh);
    InterchangeCityBlocks->SetStaticMesh(CubeMesh);
    InterchangeBuildings->SetStaticMesh(BuildingFinder.Object);
    for (UInstancedStaticMeshComponent* Component : { LaneDashes, EdgeReflectors, LampPoles, LampArms, LampHeads, SignPosts, SignBoards, DistantHills, HeroStreetLights, HeroTrees, HeroBuildings, HeroServicePads, HeroServiceCanopies, HeroServiceColumns, HeroFuelPumps, HeroServiceSigns, InterchangeDecks, InterchangeColumns, InterchangeRails, InterchangeSigns, InterchangeCityBlocks, InterchangeBuildings })
    {
        ConfigureVisualInstances(Component);
    }
}

void AHighwayEnvironmentDirector::BeginPlay()
{
    Super::BeginPlay();
    BuildVisualRoute();
}

void AHighwayEnvironmentDirector::ConfigureVisualMesh(UStaticMeshComponent* Component) const
{
    if (!Component)
    {
        return;
    }

    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(true);
}

void AHighwayEnvironmentDirector::ConfigureVisualInstances(UInstancedStaticMeshComponent* Component) const
{
    if (!Component)
    {
        return;
    }

    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(true);
}

UMaterialInstanceDynamic* AHighwayEnvironmentDirector::CreateColorMaterial(const FLinearColor& Color) const
{
    if (!BaseMaterial)
    {
        return nullptr;
    }

    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, const_cast<AHighwayEnvironmentDirector*>(this));
    Material->SetVectorParameterValue(TEXT("Color"), Color);
    return Material;
}

UMaterialInstanceDynamic* AHighwayEnvironmentDirector::CreateSurfaceMaterial(UMaterialInterface* Override, const FLinearColor& Tint) const
{
    if (!Override)
    {
        return CreateColorMaterial(Tint);
    }

    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Override, const_cast<AHighwayEnvironmentDirector*>(this));
    if (!Material)
    {
        return CreateColorMaterial(Tint);
    }

    // Deliberately does NOT push the fallback colour into the material.
    //
    // That colour exists to stand in for a texture on the flat material. Feeding
    // it to a textured material as a multiplier crushed the asphalt to black:
    // the albedo is already around 0.06 and the fallback asphalt colour is 0.03,
    // so the product was effectively zero and the surface detail was invisible.
    // A real material carries its own tint, exposed on its instance.

    // Hand the material the layout it needs to place wheel-polish wear lanes
    // analytically, instead of hard-coding the road numbers a second time.
    Material->SetScalarParameterValue(TEXT("RoadWidth"), RoadWidth);
    Material->SetScalarParameterValue(TEXT("RouteLength"), RouteLength);

    // The actual traffic lanes sit at Y = -600 / 0 / +600, so the spacing is 600,
    // not RoadWidth/3 = 666. Feeding the wrong number made the wheel-polish wear
    // lanes drift steadily out of alignment with the lanes cars actually drive
    // in, which reads as a vague wrongness rather than an obvious bug.
    Material->SetScalarParameterValue(TEXT("LaneWidth"), TrafficLaneSpacing);

    return Material;
}

void AHighwayEnvironmentDirector::BuildVisualRoute()
{
    if (bBuilt || !CubeMesh)
    {
        return;
    }

    bBuilt = true;
    SetActorLocation(FVector(RouteStartX, 0.0f, 0.0f));

    const float VisualRouteLength = RouteLength + (VisualRouteOverscan * 2.0f);

    // Darker and less yellow than it was.
    //
    // Under Lumen this surface is the largest thing next to the road, so its
    // albedo is bounced straight onto the asphalt. At 0.18/0.22/0.11 with a warm
    // low sun it was tinting the road brown and making it read as dirt rather
    // than asphalt - a problem that did not exist before global illumination was
    // enabled, because before that nothing bounced at all.
    UMaterialInstanceDynamic* TerrainMaterial = CreateSurfaceMaterial(TerrainSurfaceMaterial, FLinearColor(0.085f, 0.115f, 0.06f));
    UMaterialInstanceDynamic* AsphaltMaterial = CreateSurfaceMaterial(RoadSurfaceMaterial, FLinearColor(0.025f, 0.03f, 0.045f));
    UMaterialInstanceDynamic* ShoulderMaterial = CreateSurfaceMaterial(ShoulderSurfaceMaterial, FLinearColor(0.09f, 0.10f, 0.12f));
    UMaterialInstanceDynamic* RailMaterial = CreateSurfaceMaterial(MetalSurfaceMaterial, FLinearColor(0.18f, 0.23f, 0.28f));

    LeftLandscape->SetRelativeLocation(FVector(RouteLength * 0.5f, -11000.0f, -4.0f));
    LeftLandscape->SetRelativeScale3D(FVector(VisualRouteLength / CubeSize, 200.0f, 0.06f));
    LeftLandscape->SetMaterial(0, TerrainMaterial);

    RightLandscape->SetRelativeLocation(FVector(RouteLength * 0.5f, 11000.0f, -4.0f));
    RightLandscape->SetRelativeScale3D(FVector(VisualRouteLength / CubeSize, 200.0f, 0.06f));
    RightLandscape->SetMaterial(0, TerrainMaterial);

    RoadSurface->SetRelativeLocation(FVector(RouteLength * 0.5f, 0.0f, VisualRoadZ));
    RoadSurface->SetRelativeScale3D(FVector(VisualRouteLength / CubeSize, RoadWidth / CubeSize, 0.07f));
    RoadSurface->SetMaterial(0, AsphaltMaterial);

    for (const TPair<UStaticMeshComponent*, float>& Shoulder : { TPair<UStaticMeshComponent*, float>(LeftShoulder, -1060.0f), TPair<UStaticMeshComponent*, float>(RightShoulder, 1060.0f) })
    {
        Shoulder.Key->SetRelativeLocation(FVector(RouteLength * 0.5f, Shoulder.Value, VisualRoadZ + 1.0f));
        Shoulder.Key->SetRelativeScale3D(FVector(VisualRouteLength / CubeSize, 1.2f, 0.08f));
        Shoulder.Key->SetMaterial(0, ShoulderMaterial);
    }

    for (const TPair<UStaticMeshComponent*, float>& GuardRail : { TPair<UStaticMeshComponent*, float>(LeftGuardRail, -1180.0f), TPair<UStaticMeshComponent*, float>(RightGuardRail, 1180.0f) })
    {
        GuardRail.Key->SetRelativeLocation(FVector(RouteLength * 0.5f, GuardRail.Value, 120.0f));
        GuardRail.Key->SetRelativeScale3D(FVector(VisualRouteLength / CubeSize, 0.12f, 0.09f));
        GuardRail.Key->SetMaterial(0, RailMaterial);
    }

    LaneDashes->SetMaterial(0, CreateColorMaterial(FLinearColor(0.92f, 0.92f, 0.86f)));
    EdgeReflectors->SetMaterial(0, CreateColorMaterial(FLinearColor(1.0f, 0.72f, 0.10f)));
    LampPoles->SetMaterial(0, RailMaterial);
    LampArms->SetMaterial(0, RailMaterial);
    LampHeads->SetMaterial(0, CreateColorMaterial(FLinearColor(0.55f, 0.88f, 1.0f)));
    SignPosts->SetMaterial(0, RailMaterial);
    SignBoards->SetMaterial(0, CreateColorMaterial(FLinearColor(0.05f, 0.28f, 0.55f)));
    // Distance desaturates and cools. A saturated green mass on the horizon reads
    // as a nearby object; a cool grey-green one reads as far away, and it also
    // stops the hills bouncing colour onto the road now that Lumen is on.
    DistantHills->SetMaterial(0, CreateSurfaceMaterial(TerrainSurfaceMaterial, FLinearColor(0.075f, 0.095f, 0.105f)));
    HeroServicePads->SetMaterial(0, CreateSurfaceMaterial(RoadSurfaceMaterial, FLinearColor(0.035f, 0.04f, 0.055f)));
    HeroServiceCanopies->SetMaterial(0, CreateColorMaterial(FLinearColor(0.86f, 0.08f, 0.025f)));
    HeroServiceColumns->SetMaterial(0, CreateColorMaterial(FLinearColor(0.80f, 0.84f, 0.88f)));
    HeroFuelPumps->SetMaterial(0, CreateColorMaterial(FLinearColor(0.03f, 0.22f, 0.64f)));
    HeroServiceSigns->SetMaterial(0, CreateColorMaterial(FLinearColor(1.0f, 0.62f, 0.05f)));
    InterchangeDecks->SetMaterial(0, CreateSurfaceMaterial(ConcreteSurfaceMaterial, FLinearColor(0.19f, 0.23f, 0.28f)));
    InterchangeColumns->SetMaterial(0, CreateSurfaceMaterial(ConcreteSurfaceMaterial, FLinearColor(0.44f, 0.48f, 0.52f)));
    InterchangeRails->SetMaterial(0, CreateColorMaterial(FLinearColor(0.79f, 0.83f, 0.86f)));
    InterchangeSigns->SetMaterial(0, CreateColorMaterial(FLinearColor(0.04f, 0.34f, 0.20f)));
    InterchangeCityBlocks->SetMaterial(0, CreateColorMaterial(FLinearColor(0.12f, 0.20f, 0.29f)));

    AddLaneMarkings();
    AddRoadsideFurniture();
    AddHeroDistrict();
    AddHeroServiceAreas();
    if (UStaticMesh* ConceptCarMesh = HeroShowcaseCar->GetStaticMesh())
    {
        // Keep the hero car at a credible road-car size, on the forecourt and
        // well beyond the collision barrier.  It is an art landmark, not AI
        // traffic, so it cannot change the tested traffic or player behaviour.
        const float ConceptCarScale = 490.0f / FMath::Max(ConceptCarMesh->GetBounds().BoxExtent.X * 2.0f, 1.0f);
        const FVector Location = CalculateGroundedInstanceLocation(ConceptCarMesh, ConceptCarScale, 33200.0f, 3570.0f, 16.0f);
        HeroShowcaseCar->SetRelativeLocation(Location);
        HeroShowcaseCar->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
        HeroShowcaseCar->SetRelativeScale3D(FVector(ConceptCarScale));
    }
    AddInterchangeSetPiece();
    AddDistantLandscape();
}

void AHighwayEnvironmentDirector::AddLaneMarkings()
{
    constexpr float DashLength = 360.0f;
    constexpr float DashSpacing = 720.0f;
    constexpr float DividerOffsets[] = { -300.0f, 300.0f };

    for (float X = 700.0f; X < RouteLength + VisualRouteOverscan; X += DashSpacing)
    {
        for (float DividerY : DividerOffsets)
        {
            LaneDashes->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, DividerY, VisualRoadZ + 7.0f), FVector(DashLength / CubeSize, 0.11f, 0.025f)));
        }
    }

    // A short, bright edge dash at each shoulder gives a clear speed cue even
    // on the very long straight prototype road.
    for (float X = 500.0f; X < RouteLength + VisualRouteOverscan; X += 1250.0f)
    {
        EdgeReflectors->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, -940.0f, VisualRoadZ + 7.0f), FVector(3.2f, 0.13f, 0.03f)));
        EdgeReflectors->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, 940.0f, VisualRoadZ + 7.0f), FVector(3.2f, 0.13f, 0.03f)));
    }
}

void AHighwayEnvironmentDirector::AddRoadsideFurniture()
{
    constexpr float PostSpacing = 2500.0f;
    for (float X = 400.0f; X < RouteLength; X += PostSpacing)
    {
        for (const float Side : { -1.0f, 1.0f })
        {
            EdgeReflectors->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Side * 1110.0f, 72.0f), FVector(0.08f, 0.08f, 1.35f)));
        }
    }

    constexpr float LampSpacing = 8000.0f;
    for (float X = 100000.0f; X < RouteLength; X += LampSpacing)
    {
        for (const float Side : { -1.0f, 1.0f })
        {
            const float Y = Side * 1280.0f;
            LampPoles->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, 700.0f), FVector(0.10f, 0.10f, 14.0f)));
            LampArms->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y - (Side * 150.0f), 1380.0f), FVector(0.35f, 2.1f, 0.08f)));
            LampHeads->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y - (Side * 285.0f), 1350.0f), FVector(0.18f, 0.18f, 0.12f)));
        }
    }

    constexpr float SignSpacing = 30000.0f;
    int32 SignIndex = 0;
    for (float X = 18000.0f; X < RouteLength; X += SignSpacing)
    {
        const float Side = SignIndex++ % 2 == 0 ? -1.0f : 1.0f;
        const float Y = Side * 1550.0f;
        SignPosts->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, 360.0f), FVector(0.12f, 0.12f, 7.2f)));
        SignBoards->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y - (Side * 35.0f), 720.0f), FVector(5.8f, 0.18f, 1.7f)));
    }
}

void AHighwayEnvironmentDirector::AddHeroDistrict()
{
    // The first 900 metres are deliberately denser than the rest of the 6 km
    // sprint. Players see the higher-fidelity assets immediately, while the
    // long route remains cheap enough for a multiplayer prototype.
    constexpr float HeroStart = 7000.0f;
    constexpr float HeroEnd = 95000.0f;

    if (UStaticMesh* StreetLightMesh = HeroStreetLights->GetStaticMesh())
    {
        const float Scale = CalculateUniformScaleForHeight(StreetLightMesh, 920.0f);
        for (float X = HeroStart + 1500.0f; X < HeroEnd; X += 7000.0f)
        {
            for (const float Side : { -1.0f, 1.0f })
            {
                const FVector Location = CalculateGroundedInstanceLocation(StreetLightMesh, Scale, X, Side * 1340.0f);
                HeroStreetLights->AddInstance(FTransform(FRotator(0.0f, Side < 0.0f ? 180.0f : 0.0f, 0.0f), Location, FVector(Scale)));
            }
        }
    }

    if (UStaticMesh* TreeMesh = HeroTrees->GetStaticMesh())
    {
        for (int32 TreeIndex = 0; TreeIndex < 52; ++TreeIndex)
        {
            const float Progress = static_cast<float>(TreeIndex) / 51.0f;
            const float X = FMath::Lerp(HeroStart, HeroEnd, Progress) + static_cast<float>((TreeIndex % 5) * 155);
            const float Side = TreeIndex % 2 == 0 ? -1.0f : 1.0f;
            const float DesiredHeight = 620.0f + static_cast<float>((TreeIndex * 37) % 5) * 85.0f;
            const float Scale = CalculateUniformScaleForHeight(TreeMesh, DesiredHeight);
            const float Y = Side * (3600.0f + static_cast<float>((TreeIndex * 211) % 7) * 340.0f);
            const FVector Location = CalculateGroundedInstanceLocation(TreeMesh, Scale, X, Y, -2.0f);
            HeroTrees->AddInstance(FTransform(FRotator(0.0f, static_cast<float>((TreeIndex * 53) % 360), 0.0f), Location, FVector(Scale)));
        }
    }

    if (UStaticMesh* BuildingMesh = HeroBuildings->GetStaticMesh())
    {
        const TArray<TPair<FVector2D, float>> Buildings = {
            TPair<FVector2D, float>(FVector2D(16000.0f, -5600.0f), 720.0f),
            TPair<FVector2D, float>(FVector2D(27500.0f, 5900.0f), 980.0f),
            TPair<FVector2D, float>(FVector2D(43000.0f, -6200.0f), 760.0f),
            TPair<FVector2D, float>(FVector2D(58500.0f, 5600.0f), 1080.0f),
            TPair<FVector2D, float>(FVector2D(73500.0f, -6000.0f), 840.0f),
            TPair<FVector2D, float>(FVector2D(88000.0f, 6100.0f), 920.0f)
        };
        for (int32 BuildingIndex = 0; BuildingIndex < Buildings.Num(); ++BuildingIndex)
        {
            const TPair<FVector2D, float>& Building = Buildings[BuildingIndex];
            const float Scale = CalculateUniformScaleForHeight(BuildingMesh, Building.Value);
            const FVector Location = CalculateGroundedInstanceLocation(BuildingMesh, Scale, Building.Key.X, Building.Key.Y, -2.0f);
            HeroBuildings->AddInstance(FTransform(FRotator(0.0f, BuildingIndex % 2 == 0 ? 90.0f : -90.0f, 0.0f), Location, FVector(Scale)));
        }
    }
}

void AHighwayEnvironmentDirector::AddHeroServiceAreas()
{
    // The route is still intentionally one uninterrupted racing corridor.  These
    // forecourts sit beyond the physical barrier and only create the impression
    // of a roadside service district as the player enters the first kilometre.
    // Every one of their components is configured as NoCollision in the ctor.
    const auto AddCube = [](UInstancedStaticMeshComponent* Component, const FVector& Location, const FVector& Scale, float Yaw = 0.0f)
    {
        if (Component)
        {
            Component->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), Location, Scale));
        }
    };

    const auto AddCanopy = [this, &AddCube](float CenterX, float CenterY, float Yaw)
    {
        // A low, broad roof and six support columns are enough to read as a
        // fuel forecourt at motorway speed without hiding the road horizon.
        AddCube(HeroServiceCanopies, FVector(CenterX, CenterY, 820.0f), FVector(68.0f, 18.0f, 1.35f), Yaw);

        for (const float LocalX : { -2800.0f, 0.0f, 2800.0f })
        {
            for (const float LocalY : { -650.0f, 650.0f })
            {
                HeroServiceColumns->AddInstance(FTransform(FRotator::ZeroRotator, FVector(CenterX + LocalX, CenterY + LocalY, 395.0f), FVector(1.25f, 1.25f, 7.9f)));
            }
        }
    };

    const auto AddPumpRow = [this, &AddCube](float CenterX, float CenterY)
    {
        for (const float LocalX : { -2200.0f, 0.0f, 2200.0f })
        {
            AddCube(HeroFuelPumps, FVector(CenterX + LocalX, CenterY - 320.0f, 280.0f), FVector(3.4f, 2.7f, 5.6f));
            AddCube(HeroFuelPumps, FVector(CenterX + LocalX, CenterY + 320.0f, 280.0f), FVector(3.4f, 2.7f, 5.6f));
        }
    };

    const auto AddPricePylon = [this, &AddCube](float X, float Y, float FacingYaw)
    {
        AddCube(HeroServiceSigns, FVector(X, Y, 530.0f), FVector(0.30f, 0.30f, 10.6f), FacingYaw);
        AddCube(HeroServiceSigns, FVector(X, Y, 1100.0f), FVector(1.9f, 0.24f, 3.2f), FacingYaw);
        // A smaller contrasting stripe makes the pylon legible even at a
        // distance, without relying on a text texture or another asset.
        AddCube(HeroServiceCanopies, FVector(X, Y - 26.0f, 1120.0f), FVector(1.35f, 0.08f, 0.42f), FacingYaw);
    };

    // Right-side fuel stop: this is the first large landmark the player sees
    // about five seconds after starting a 180 km/h run.
    constexpr float RightServiceX = 30000.0f;
    constexpr float RightServiceY = 3820.0f;
    AddCube(HeroServicePads, FVector(RightServiceX, RightServiceY, 2.0f), FVector(175.0f, 58.0f, 0.12f));
    AddCanopy(28000.0f, 2590.0f, 0.0f);
    AddPumpRow(28000.0f, 2590.0f);
    AddPricePylon(22400.0f, 1650.0f, 0.0f);

    // The actual staged building asset makes the stop read as a rest building
    // rather than a collection of primitive meshes.
    if (UStaticMesh* BuildingMesh = HeroBuildings->GetStaticMesh())
    {
        const float MainBuildingScale = CalculateUniformScaleForHeight(BuildingMesh, 1000.0f);
        const FVector MainBuildingLocation = CalculateGroundedInstanceLocation(BuildingMesh, MainBuildingScale, 36500.0f, 5150.0f, -2.0f);
        HeroBuildings->AddInstance(FTransform(FRotator(0.0f, -90.0f, 0.0f), MainBuildingLocation, FVector(MainBuildingScale)));

        const float AnnexScale = CalculateUniformScaleForHeight(BuildingMesh, 620.0f);
        const FVector AnnexLocation = CalculateGroundedInstanceLocation(BuildingMesh, AnnexScale, 40700.0f, 3700.0f, -2.0f);
        HeroBuildings->AddInstance(FTransform(FRotator(0.0f, -90.0f, 0.0f), AnnexLocation, FVector(AnnexScale)));
    }

    // Left-side rest plaza: it breaks the straight road's silhouette further
    // along, with a smaller canopy, car park apron and a second landmark sign.
    constexpr float LeftRestX = 68200.0f;
    constexpr float LeftRestY = -3900.0f;
    AddCube(HeroServicePads, FVector(LeftRestX, LeftRestY, 2.0f), FVector(145.0f, 51.0f, 0.12f));
    AddCanopy(65000.0f, -2760.0f, 0.0f);
    AddPumpRow(65000.0f, -2760.0f);
    AddPricePylon(60500.0f, -1650.0f, 0.0f);

    if (UStaticMesh* BuildingMesh = HeroBuildings->GetStaticMesh())
    {
        const float RestBuildingScale = CalculateUniformScaleForHeight(BuildingMesh, 870.0f);
        const FVector RestBuildingLocation = CalculateGroundedInstanceLocation(BuildingMesh, RestBuildingScale, 73200.0f, -5000.0f, -2.0f);
        HeroBuildings->AddInstance(FTransform(FRotator(0.0f, 90.0f, 0.0f), RestBuildingLocation, FVector(RestBuildingScale)));
    }

    // Reuse the staged ArchVis trees to give both plazas a planted perimeter.
    if (UStaticMesh* TreeMesh = HeroTrees->GetStaticMesh())
    {
        const TArray<FVector2D> ServiceTrees = {
            FVector2D(23500.0f, 5600.0f), FVector2D(30000.0f, 6400.0f), FVector2D(38200.0f, 6200.0f),
            FVector2D(61000.0f, -5950.0f), FVector2D(68500.0f, -6200.0f), FVector2D(75800.0f, -5900.0f)
        };
        for (int32 TreeIndex = 0; TreeIndex < ServiceTrees.Num(); ++TreeIndex)
        {
            const float Scale = CalculateUniformScaleForHeight(TreeMesh, 820.0f + static_cast<float>(TreeIndex % 3) * 95.0f);
            const FVector Location = CalculateGroundedInstanceLocation(TreeMesh, Scale, ServiceTrees[TreeIndex].X, ServiceTrees[TreeIndex].Y, -2.0f);
            HeroTrees->AddInstance(FTransform(FRotator(0.0f, 37.0f + (TreeIndex * 61.0f), 0.0f), Location, FVector(Scale)));
        }
    }
}

void AHighwayEnvironmentDirector::AddInterchangeSetPiece()
{
    // The service areas occupy the first 700 m.  This city-entry landmark is
    // just after the 1.4 km mark, giving the long sprint a clear second visual
    // chapter while remaining a completely non-interactive HISM layer.
    constexpr float InterchangeX = 142000.0f;
    constexpr float OverpassZ = 1510.0f;

    const auto AddCube = [](UInstancedStaticMeshComponent* Component, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator)
    {
        if (Component)
        {
            Component->AddInstance(FTransform(Rotation, Location, Scale));
        }
    };

    // The crossing deck is intentionally high enough for every current and
    // future traffic visual.  It spans the full motorway and the two external
    // service strips, making it read as a genuine road interchange at speed.
    AddCube(InterchangeDecks, FVector(InterchangeX, 0.0f, OverpassZ), FVector(20.0f, 75.0f, 1.35f));
    AddCube(InterchangeRails, FVector(InterchangeX, -3580.0f, OverpassZ + 175.0f), FVector(20.5f, 0.18f, 1.65f));
    AddCube(InterchangeRails, FVector(InterchangeX, 3580.0f, OverpassZ + 175.0f), FVector(20.5f, 0.18f, 1.65f));
    AddCube(InterchangeRails, FVector(InterchangeX - 950.0f, 0.0f, OverpassZ + 175.0f), FVector(0.18f, 35.8f, 1.65f));
    AddCube(InterchangeRails, FVector(InterchangeX + 950.0f, 0.0f, OverpassZ + 175.0f), FVector(0.18f, 35.8f, 1.65f));

    // Four wide piers remain beyond the crash barriers.  They therefore give
    // the bridge a grounded silhouette without ever becoming a physics risk.
    for (const float PierX : { InterchangeX - 650.0f, InterchangeX + 650.0f })
    {
        for (const float PierY : { -2550.0f, 2550.0f })
        {
            InterchangeColumns->AddInstance(FTransform(FRotator::ZeroRotator, FVector(PierX, PierY, 740.0f), FVector(2.8f, 2.8f, 14.8f)));
            AddCube(InterchangeDecks, FVector(PierX, PierY, 1320.0f), FVector(7.8f, 7.8f, 0.75f));
        }
    }

    // Parallel rising ramps sell the interchange from the driver's point of
    // view.  Their pitch is visual only; the original straight road remains
    // the sole playable surface underneath.
    const FRotator RiseRotation(-7.5f, 0.0f, 0.0f);
    const FRotator FallRotation(7.5f, 0.0f, 0.0f);
    AddCube(InterchangeDecks, FVector(InterchangeX - 7200.0f, -4250.0f, 790.0f), FVector(70.0f, 10.5f, 1.05f), RiseRotation);
    AddCube(InterchangeDecks, FVector(InterchangeX + 7200.0f, -4250.0f, 790.0f), FVector(70.0f, 10.5f, 1.05f), FallRotation);
    AddCube(InterchangeDecks, FVector(InterchangeX - 7200.0f, 4250.0f, 790.0f), FVector(70.0f, 10.5f, 1.05f), RiseRotation);
    AddCube(InterchangeDecks, FVector(InterchangeX + 7200.0f, 4250.0f, 790.0f), FVector(70.0f, 10.5f, 1.05f), FallRotation);

    // A pair of large overhead guide signs becomes visible well before the
    // bridge.  Simple geometry is more readable than text on a fast-moving
    // screen and avoids adding a UI-dependent art asset.
    for (const float SignX : { InterchangeX - 8700.0f, InterchangeX - 4200.0f })
    {
        AddCube(InterchangeSigns, FVector(SignX, 0.0f, 1390.0f), FVector(0.34f, 16.8f, 3.8f));
        AddCube(InterchangeColumns, FVector(SignX, -920.0f, 650.0f), FVector(0.55f, 0.55f, 12.8f));
        AddCube(InterchangeColumns, FVector(SignX, 920.0f, 650.0f), FVector(0.55f, 0.55f, 12.8f));
    }

    // A small skyline frames the bridge.  Basic blocks provide a consistent
    // high-rise silhouette, while the staged Building mesh breaks it up with
    // real architectural detail.  Everything sits safely outside the route.
    const TArray<TPair<FVector2D, FVector>> CityBlocks = {
        TPair<FVector2D, FVector>(FVector2D(127000.0f, -6550.0f), FVector(16.0f, 12.0f, 28.0f)),
        TPair<FVector2D, FVector>(FVector2D(133000.0f, -7000.0f), FVector(11.0f, 10.0f, 20.0f)),
        TPair<FVector2D, FVector>(FVector2D(151000.0f, -6750.0f), FVector(17.0f, 13.0f, 32.0f)),
        TPair<FVector2D, FVector>(FVector2D(158000.0f, -6250.0f), FVector(12.0f, 11.0f, 23.0f)),
        TPair<FVector2D, FVector>(FVector2D(129000.0f, 6550.0f), FVector(14.0f, 11.0f, 25.0f)),
        TPair<FVector2D, FVector>(FVector2D(137000.0f, 7000.0f), FVector(18.0f, 12.0f, 34.0f)),
        TPair<FVector2D, FVector>(FVector2D(153000.0f, 6650.0f), FVector(13.0f, 10.0f, 22.0f)),
        TPair<FVector2D, FVector>(FVector2D(160000.0f, 7200.0f), FVector(17.0f, 13.0f, 30.0f))
    };
    for (const TPair<FVector2D, FVector>& CityBlock : CityBlocks)
    {
        const FVector Location(CityBlock.Key.X, CityBlock.Key.Y, CityBlock.Value.Z * 50.0f);
        AddCube(InterchangeCityBlocks, Location, CityBlock.Value);
    }

    if (UStaticMesh* BuildingMesh = InterchangeBuildings->GetStaticMesh())
    {
        const TArray<TPair<FVector2D, float>> LandmarkBuildings = {
            TPair<FVector2D, float>(FVector2D(123500.0f, -5050.0f), 1550.0f),
            TPair<FVector2D, float>(FVector2D(160500.0f, 5150.0f), 1420.0f),
            TPair<FVector2D, float>(FVector2D(145000.0f, -5550.0f), 1120.0f)
        };
        for (int32 BuildingIndex = 0; BuildingIndex < LandmarkBuildings.Num(); ++BuildingIndex)
        {
            const float Scale = CalculateUniformScaleForHeight(BuildingMesh, LandmarkBuildings[BuildingIndex].Value);
            const FVector Location = CalculateGroundedInstanceLocation(BuildingMesh, Scale, LandmarkBuildings[BuildingIndex].Key.X, LandmarkBuildings[BuildingIndex].Key.Y, -2.0f);
            InterchangeBuildings->AddInstance(FTransform(FRotator(0.0f, BuildingIndex == 1 ? -90.0f : 90.0f, 0.0f), Location, FVector(Scale)));
        }
    }

    if (UStaticMesh* TreeMesh = HeroTrees->GetStaticMesh())
    {
        for (int32 TreeIndex = 0; TreeIndex < 18; ++TreeIndex)
        {
            const float Side = TreeIndex % 2 == 0 ? -1.0f : 1.0f;
            const float X = 124000.0f + static_cast<float>(TreeIndex) * 2200.0f;
            const float Y = Side * (4550.0f + static_cast<float>((TreeIndex % 4) * 340));
            const float Scale = CalculateUniformScaleForHeight(TreeMesh, 700.0f + static_cast<float>((TreeIndex % 3) * 95));
            const FVector Location = CalculateGroundedInstanceLocation(TreeMesh, Scale, X, Y, -2.0f);
            HeroTrees->AddInstance(FTransform(FRotator(0.0f, static_cast<float>(TreeIndex * 47), 0.0f), Location, FVector(Scale)));
        }
    }
}

void AHighwayEnvironmentDirector::AddProductionArchitecture()
{
    // Reference staging only: the route intentionally does not call this.
    // The sample meshes require authored asset-specific transforms before
    // they can be used in a playable environment.
#if 0
    // This is the first deliberately small production-art pass.  It upgrades
    // the existing landmarks with installed Unreal sample meshes but leaves
    // the proven 6 km collision corridor, lanes, and replicated gameplay
    // completely untouched.  A later authored map can reuse this palette.
    const auto AddGroundedModule = [](UInstancedStaticMeshComponent* Component, float DesiredHeight, const FVector& Anchor, float GroundZ, float Yaw = 0.0f)
    {
        if (!Component || !Component->GetStaticMesh())
        {
            return;
        }

        const float Scale = CalculateUniformScaleForHeight(Component->GetStaticMesh(), DesiredHeight);
        const FVector Location = CalculateGroundedInstanceLocation(Component->GetStaticMesh(), Scale, Anchor.X, Anchor.Y, GroundZ);
        Component->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), Location, FVector(Scale)));
    };

    const auto AddCanopyFrame = [&AddGroundedModule, this](float CenterX, float CenterY)
    {
        // The old broad canopy is kept as a distant silhouette.  Its near
        // supports and roof trim now come from actual Building sample meshes.
        AddGroundedModule(ProductionRoofs, 165.0f, FVector(CenterX, CenterY, 0.0f), 705.0f);
        for (const float LocalX : { -2650.0f, 0.0f, 2650.0f })
        {
            for (const float LocalY : { -610.0f, 610.0f })
            {
                AddGroundedModule(ProductionColumns, 690.0f, FVector(CenterX + LocalX, CenterY + LocalY, 0.0f), 4.0f);
            }
        }
    };

    // The two existing service stops become the first close-range art test.
    // The elements sit outside both barriers and therefore cannot block or
    // overlap a player's vehicle, even in a two-player listen-server run.
    AddCanopyFrame(28000.0f, 2590.0f);
    AddCanopyFrame(65000.0f, -2760.0f);
    AddGroundedModule(ProductionWalls, 780.0f, FVector(36500.0f, 5850.0f, 0.0f), -2.0f, -90.0f);
    AddGroundedModule(ProductionFrames, 650.0f, FVector(36500.0f, 4560.0f, 0.0f), -2.0f, -90.0f);
    AddGroundedModule(ProductionGlassFacades, 910.0f, FVector(40500.0f, 5220.0f, 0.0f), -2.0f, -90.0f);
    AddGroundedModule(ProductionWalkways, 230.0f, FVector(38600.0f, 4300.0f, 0.0f), -2.0f, 90.0f);
    AddGroundedModule(ProductionRailings, 145.0f, FVector(39200.0f, 3380.0f, 0.0f), 620.0f, 90.0f);
    AddGroundedModule(ProductionWalls, 720.0f, FVector(73200.0f, -5700.0f, 0.0f), -2.0f, 90.0f);
    AddGroundedModule(ProductionFrames, 590.0f, FVector(73200.0f, -4480.0f, 0.0f), -2.0f, 90.0f);

    // Around the visual interchange, a few real facade modules break the
    // earlier block-only skyline.  Their spacing makes the structure read as
    // a city edge rather than a field of independent cubes at driving speed.
    const TArray<TPair<FVector2D, float>> InterchangeFacades = {
        TPair<FVector2D, float>(FVector2D(130500.0f, -4300.0f), 1320.0f),
        TPair<FVector2D, float>(FVector2D(137500.0f, 4450.0f), 1180.0f),
        TPair<FVector2D, float>(FVector2D(151500.0f, -4500.0f), 1420.0f),
        TPair<FVector2D, float>(FVector2D(158500.0f, 4380.0f), 1100.0f)
    };
    for (int32 FacadeIndex = 0; FacadeIndex < InterchangeFacades.Num(); ++FacadeIndex)
    {
        const TPair<FVector2D, float>& Facade = InterchangeFacades[FacadeIndex];
        const float SideYaw = Facade.Key.Y < 0.0f ? 90.0f : -90.0f;
        AddGroundedModule(ProductionGlassFacades, Facade.Value, FVector(Facade.Key.X, Facade.Key.Y, 0.0f), -2.0f, SideYaw);
        AddGroundedModule(ProductionFrames, Facade.Value * 0.78f, FVector(Facade.Key.X + (FacadeIndex % 2 == 0 ? 980.0f : -980.0f), Facade.Key.Y, 0.0f), -2.0f, SideYaw);
    }

    // The 2.2 km city gate gives the extended sprint another authored visual
    // chapter.  It deliberately frames the highway from beyond the barriers
    // instead of becoming a physical toll booth or a new route branch.
    constexpr float CityGateX = 220000.0f;
    for (const float Side : { -1.0f, 1.0f })
    {
        const float SideYaw = Side < 0.0f ? 90.0f : -90.0f;
        AddGroundedModule(ProductionColumns, 1280.0f, FVector(CityGateX - 1850.0f, Side * 3200.0f, 0.0f), -2.0f, SideYaw);
        AddGroundedModule(ProductionColumns, 1280.0f, FVector(CityGateX + 1850.0f, Side * 3200.0f, 0.0f), -2.0f, SideYaw);
        AddGroundedModule(ProductionGlassFacades, 1560.0f, FVector(CityGateX, Side * 4550.0f, 0.0f), -2.0f, SideYaw);
        AddGroundedModule(ProductionWalls, 1040.0f, FVector(CityGateX + 3500.0f, Side * 4380.0f, 0.0f), -2.0f, SideYaw);
        AddGroundedModule(ProductionBeams, 380.0f, FVector(CityGateX - 3700.0f, Side * 3720.0f, 0.0f), 1020.0f, SideYaw);
        AddGroundedModule(ProductionRailings, 180.0f, FVector(CityGateX, Side * 2800.0f, 0.0f), 760.0f, SideYaw);
    }
#endif
}

void AHighwayEnvironmentDirector::AddDistantLandscape()
{
    if (!bShowDistantHills)
    {
        return;
    }

    // These used to be cones scaled 10-16 wide by 14-27 tall, sitting only 50-70
    // metres off the road. Taller than wide, evenly spaced and close enough to
    // parallax against the barrier, they read as giant traffic cones rather than
    // terrain - and no amount of scaling fixes that, because a cone silhouette is
    // a cone. A flattened sphere gives a rounded dome, which is what a distant
    // hill actually looks like.
    //
    // Real distant terrain is also much wider than tall (roughly 8:1 here), far
    // enough away not to parallax, and irregular. Even spacing is what makes a
    // horizon read as manufactured.
    int32 HillIndex = 0;
    for (float X = 40000.0f; X < RouteLength + 60000.0f; X += 47000.0f)
    {
        for (const float Side : { -1.0f, 1.0f })
        {
            // Two coprime strides so the pattern does not visibly repeat.
            const float SpreadNoise = static_cast<float>((HillIndex * 37) % 13) / 13.0f;
            const float SizeNoise = static_cast<float>((HillIndex * 23) % 11) / 11.0f;

            const float Y = Side * (36000.0f + (SpreadNoise * 42000.0f));
            const float ScaleXY = 240.0f + (SizeNoise * 210.0f);
            const float ScaleZ = ScaleXY * (0.10f + (SpreadNoise * 0.05f));
            const float AlongRoad = X + (SpreadNoise * 26000.0f);

            // Sphere pivot is central, so Z = 0 buries the lower half and leaves a
            // dome. A little extra burial varies the profile between instances.
            const float Z = -ScaleZ * (4.0f + (SizeNoise * 12.0f));

            DistantHills->AddInstance(FTransform(
                FRotator(0.0f, SpreadNoise * 360.0f, 0.0f),
                FVector(AlongRoad, Y, Z),
                FVector(ScaleXY, ScaleXY * (0.7f + (SizeNoise * 0.6f)), ScaleZ)));
            ++HillIndex;
        }
    }
}
