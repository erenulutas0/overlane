#include "TrafficVehicleBase.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "OverlaneGameModeBase.h"
#include "OverlaneVehiclePawn.h"
#include "TrafficLanePath.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FVector CalculateTrafficMeshScaleForSize(const UStaticMesh* Mesh, const FVector& DesiredSize)
{
    if (!Mesh)
    {
        return FVector::OneVector;
    }

    const FVector SourceSize = Mesh->GetBounds().BoxExtent * 2.0f;
    const float UniformScale = FMath::Min3(
        DesiredSize.X / FMath::Max(SourceSize.X, 1.0f),
        DesiredSize.Y / FMath::Max(SourceSize.Y, 1.0f),
        DesiredSize.Z / FMath::Max(SourceSize.Z, 1.0f));
    return FVector(UniformScale);
}

}

ATrafficVehicleBase::ATrafficVehicleBase()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(30.0f);
    SetMinNetUpdateFrequency(15.0f);

    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
    Collision->SetBoxExtent(FVector(210.0f, 95.0f, 65.0f));
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    // Traffic uses logical following for other traffic; physical blocking is retained for Pawns.
    Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
    SetRootComponent(Collision);

    NearMissTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("NearMissTrigger"));
    NearMissTrigger->SetupAttachment(Collision);
    NearMissTrigger->SetBoxExtent(FVector(360.0f, 340.0f, 120.0f));
    NearMissTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    NearMissTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    NearMissTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    NearMissTrigger->SetGenerateOverlapEvents(true);
    NearMissTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATrafficVehicleBase::HandleNearMissBegin);
    NearMissTrigger->OnComponentEndOverlap.AddDynamic(this, &ATrafficVehicleBase::HandleNearMissEnd);

    VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));
    VehicleMesh->SetupAttachment(Collision);
    VehicleMesh->SetRelativeScale3D(FVector(4.2f, 1.9f, 1.3f));
    VehicleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CabinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinMesh"));
    CabinMesh->SetupAttachment(Collision);
    CabinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FrontLightBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontLightBar"));
    FrontLightBar->SetupAttachment(Collision);
    FrontLightBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RearLightBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RearLightBar"));
    RearLightBar->SetupAttachment(Collision);
    RearLightBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FrontLeftWheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontLeftWheel"));
    FrontLeftWheel->SetupAttachment(Collision);
    FrontLeftWheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
    FrontLeftWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FrontRightWheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontRightWheel"));
    FrontRightWheel->SetupAttachment(Collision);
    FrontRightWheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
    FrontRightWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RearLeftWheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RearLeftWheel"));
    RearLeftWheel->SetupAttachment(Collision);
    RearLeftWheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
    RearLeftWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RearRightWheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RearRightWheel"));
    RearRightWheel->SetupAttachment(Collision);
    RearRightWheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
    RearRightWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SportsCarBodyMesh(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar.SM_SportsCar"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SportsCarGlassMesh(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Glass.SM_SportsCar_Glass"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SportsCarWheelMesh(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel"));
    TemplateSportsCarBodyMesh = SportsCarBodyMesh.Object;
    TemplateSportsCarGlassMesh = SportsCarGlassMesh.Object;
    TemplateSportsCarWheelMesh = SportsCarWheelMesh.Object;
    bUsesTemplateSportsCar = TemplateSportsCarBodyMesh && TemplateSportsCarGlassMesh && TemplateSportsCarWheelMesh;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> OffroadBodyMesh(TEXT("/Game/Vehicles/OffroadCar/SM_Offroad_Body.SM_Offroad_Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> OffroadTireMesh(TEXT("/Game/Vehicles/OffroadCar/SM_Offroad_Tire.SM_Offroad_Tire"));
    TemplateOffroadBodyMesh = OffroadBodyMesh.Object;
    TemplateOffroadTireMesh = OffroadTireMesh.Object;
    bUsesTemplateOffroad = TemplateOffroadBodyMesh && TemplateOffroadTireMesh;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    PlaceholderBodyMesh = PlaceholderMesh.Object;
    PlaceholderWheelMesh = WheelMesh.Object;

    if (bUsesTemplateSportsCar)
    {
        VehicleMesh->SetStaticMesh(TemplateSportsCarBodyMesh);
        CabinMesh->SetStaticMesh(TemplateSportsCarGlassMesh);
        FrontLeftWheel->SetStaticMesh(TemplateSportsCarWheelMesh);
        FrontRightWheel->SetStaticMesh(TemplateSportsCarWheelMesh);
        RearLeftWheel->SetStaticMesh(TemplateSportsCarWheelMesh);
        RearRightWheel->SetStaticMesh(TemplateSportsCarWheelMesh);
        FrontLightBar->SetVisibility(false);
        RearLightBar->SetVisibility(false);
    }
    else
    {
        if (PlaceholderBodyMesh)
        {
            VehicleMesh->SetStaticMesh(PlaceholderBodyMesh);
            CabinMesh->SetStaticMesh(PlaceholderBodyMesh);
            FrontLightBar->SetStaticMesh(PlaceholderBodyMesh);
            RearLightBar->SetStaticMesh(PlaceholderBodyMesh);
        }

        if (PlaceholderWheelMesh)
        {
            FrontLeftWheel->SetStaticMesh(PlaceholderWheelMesh);
            FrontRightWheel->SetStaticMesh(PlaceholderWheelMesh);
            RearLeftWheel->SetStaticMesh(PlaceholderWheelMesh);
            RearRightWheel->SetStaticMesh(PlaceholderWheelMesh);
        }
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlaceholderMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (PlaceholderMaterial.Succeeded())
    {
        VehicleMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial.Object, this);
        CabinMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial.Object, this);
        TireMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial.Object, this);
        FrontLampMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial.Object, this);
        RearLampMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial.Object, this);
        CabinMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.015f, 0.035f, 0.07f));
        TireMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.012f, 0.014f, 0.018f));
        FrontLampMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.42f, 0.88f, 1.0f));
        RearLampMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.92f, 0.035f, 0.02f));

    }

    ApplyTrafficVisualState();
    DeactivateForPool();
}

void ATrafficVehicleBase::ActivateOnLane(ATrafficLanePath* Lane, float StartDistance, float InDesiredSpeed)
{
    AssignedLane = Lane;
    LaneChangeTarget = nullptr;
    LaneDistance = FMath::Max(0.0f, StartDistance);
    DesiredSpeed = FMath::Max(0.0f, InDesiredSpeed);
    CurrentSpeed = DesiredSpeed;
    TrafficSpeedLimit = DesiredSpeed;
    bTrafficActive = AssignedLane != nullptr;
    bNearMissEncounterActive = false;
    bNearMissBlocked = false;
    bNearMissAwarded = false;
    bChangingLane = false;
    LaneChangeElapsed = 0.0f;
    ApplyTrafficActiveState();

    if (bTrafficActive)
    {
        SetActorTransform(AssignedLane->GetTransformAtDistance(LaneDistance));
    }

    ForceNetUpdate();
}

void ATrafficVehicleBase::DeactivateForPool()
{
    bTrafficActive = false;
    AssignedLane = nullptr;
    LaneChangeTarget = nullptr;
    bNearMissEncounterActive = false;
    ApplyTrafficActiveState();
    ForceNetUpdate();
}

bool ATrafficVehicleBase::IsOccupyingLane(const ATrafficLanePath* Lane) const
{
    return bTrafficActive && (AssignedLane == Lane || (bChangingLane && LaneChangeTarget == Lane));
}

bool ATrafficVehicleBase::BeginLaneChange(ATrafficLanePath* TargetLane, float DurationSeconds)
{
    if (!bTrafficActive || bChangingLane || !AssignedLane || !TargetLane || TargetLane == AssignedLane)
    {
        return false;
    }

    LaneChangeTarget = TargetLane;
    LaneChangeElapsed = 0.0f;
    LaneChangeDuration = FMath::Max(0.1f, DurationSeconds);
    bChangingLane = true;
    return true;
}

void ATrafficVehicleBase::SetTrafficSpeedLimit(float InSpeedLimit)
{
    TrafficSpeedLimit = FMath::Clamp(InSpeedLimit, 0.0f, DesiredSpeed);
}

void ATrafficVehicleBase::MarkPlayerCollision()
{
    bNearMissBlocked = true;
    bNearMissEncounterActive = false;
}

void ATrafficVehicleBase::HandleNearMissBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    const AOverlaneVehiclePawn* PlayerVehicle = Cast<AOverlaneVehiclePawn>(OtherActor);
    if (!bTrafficActive || bNearMissAwarded || bNearMissBlocked || !PlayerVehicle)
    {
        return;
    }

    // bNearMissEncounterActive is a single flag on this traffic car, so an AI
    // rival entering the same trigger would overwrite whatever the human had
    // armed. The pawn-side IsAIRacer guards do not cover this path.
    if (PlayerVehicle->IsAIRacer())
    {
        return;
    }

    bNearMissEncounterActive = PlayerVehicle->GetSpeedKph() >= MinimumNearMissSpeedKph;
}

void ATrafficVehicleBase::HandleNearMissEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
    AOverlaneVehiclePawn* PlayerVehicle = Cast<AOverlaneVehiclePawn>(OtherActor);
    if (!bNearMissEncounterActive || bNearMissAwarded || bNearMissBlocked || !PlayerVehicle)
    {
        return;
    }

    // An AI rival leaving the trigger must not fall through to the unconditional
    // clear below, which would silently cancel the human's in-flight near miss.
    if (PlayerVehicle->IsAIRacer())
    {
        return;
    }

    const float PlayerForwardOffset = FVector::DotProduct(PlayerVehicle->GetActorLocation() - GetActorLocation(), GetActorForwardVector());
    const float PlayerLateralOffset = FMath::Abs(FVector::DotProduct(PlayerVehicle->GetActorLocation() - GetActorLocation(), GetActorRightVector()));
    const float RelativePassSpeedKph = PlayerVehicle->GetSpeedKph() - (CurrentSpeed * 0.036f);
    if (PlayerForwardOffset > MinimumNearMissForwardOffset
        && PlayerLateralOffset <= MaximumNearMissLateralDistance
        && PlayerVehicle->GetSpeedKph() >= MinimumNearMissSpeedKph
        && RelativePassSpeedKph >= MinimumRelativePassSpeedKph)
    {
        bNearMissAwarded = PlayerVehicle->TryRegisterNearMiss(TrafficColor);
    }

    bNearMissEncounterActive = false;
}

void ATrafficVehicleBase::SetTrafficColor(const FLinearColor& InColor)
{
    TrafficColor = InColor;
    ReplicatedTrafficColor = InColor;
    ApplyTrafficVisualState();
    ForceNetUpdate();
}

void ATrafficVehicleBase::SetTrafficVariant(FName InVariantName, const FVector& InMeshScale, const FVector& InCollisionExtent)
{
    TrafficVariantName = InVariantName;
    ReplicatedTrafficVariantName = InVariantName;
    ReplicatedMeshScale = InMeshScale;
    ReplicatedCollisionExtent = InCollisionExtent;
    ApplyTrafficVisualState();
    ForceNetUpdate();
}

void ATrafficVehicleBase::ApplyTrafficActiveState()
{
    SetActorHiddenInGame(!bTrafficActive);
    SetActorEnableCollision(bTrafficActive);
}

namespace
{
    /**
     * Candidate tint parameter names, in preference order. Different vendors and
     * Epic's own templates disagree, so the material is asked rather than assumed.
     */
    static const FName OverlaneTintParameterNames[] =
    {
        FName(TEXT("Color")),
        FName(TEXT("BaseColor")),
        FName(TEXT("Base Color")),
        FName(TEXT("PaintColor")),
        FName(TEXT("Paint Color")),
        FName(TEXT("BodyColor")),
        FName(TEXT("Tint"))
    };
}

bool ATrafficVehicleBase::ApplyTintedAuthoredMaterials(const FLinearColor& Tint)
{
    UStaticMesh* Mesh = VehicleMesh ? VehicleMesh->GetStaticMesh() : nullptr;
    if (!Mesh)
    {
        return false;
    }

    bool bTintedAnySlot = false;

    for (int32 MaterialIndex = 0; MaterialIndex < VehicleMesh->GetNumMaterials(); ++MaterialIndex)
    {
        // Source from the ASSET, never from the component: re-reading the
        // component would nest a dynamic instance inside the previous one every
        // time a pooled car changes variant.
        UMaterialInterface* AuthoredMaterial = Mesh->GetMaterial(MaterialIndex);
        if (!AuthoredMaterial)
        {
            continue;
        }

        FLinearColor ExistingValue;
        const FName* MatchedParameter = nullptr;
        for (const FName& Candidate : OverlaneTintParameterNames)
        {
            if (AuthoredMaterial->GetVectorParameterValue(Candidate, ExistingValue))
            {
                MatchedParameter = &Candidate;
                break;
            }
        }

        if (!MatchedParameter)
        {
            // Glass, tyres and trim usually have no tint parameter, and must keep
            // their authored look rather than being painted body colour.
            VehicleMesh->SetMaterial(MaterialIndex, AuthoredMaterial);
            continue;
        }

        if (UMaterialInstanceDynamic* TintedMaterial = VehicleMesh->CreateDynamicMaterialInstance(MaterialIndex, AuthoredMaterial))
        {
            TintedMaterial->SetVectorParameterValue(*MatchedParameter, Tint);
            bTintedAnySlot = true;
        }
    }

    return bTintedAnySlot;
}

void ATrafficVehicleBase::ApplyTrafficVisualState()
{
    TrafficColor = ReplicatedTrafficColor;
    TrafficVariantName = ReplicatedTrafficVariantName;

    // A client's pose for this car is a guess, so bias its collision box slightly
    // narrow. A marginal graze then becomes a late server collision rather than a
    // ghost collision the client felt and the server never agreed to.
    const FVector ClientInset = HasAuthority() ? FVector::ZeroVector : FVector(0.0f, 12.0f, 0.0f);
    Collision->SetBoxExtent(ReplicatedCollisionExtent - ClientInset);
    NearMissTrigger->SetBoxExtent(ReplicatedCollisionExtent + FVector(150.0f, 245.0f, 55.0f));

    // Near-miss scoring is server-only. Leaving the trigger live on clients just
    // generates overlap traffic that nothing reads.
    NearMissTrigger->SetGenerateOverlapEvents(HasAuthority());

    ConfigureTrafficVisualAssetForVariant();

    if (VehicleMaterial)
    {
        VehicleMaterial->SetVectorParameterValue(TEXT("Color"), TrafficColor);
    }

    if (bUsingTemplateSportsCarVisual)
    {
        if (!ApplyTintedAuthoredMaterials(TrafficColor))
        {
            // No tintable parameter: keep colour coding, lose the PBR.
            for (int32 MaterialIndex = 0; MaterialIndex < VehicleMesh->GetNumMaterials(); ++MaterialIndex)
            {
                VehicleMesh->SetMaterial(MaterialIndex, VehicleMaterial);
            }
        }
        UpdateTemplateSportsCarVisualGeometry();
    }
    else if (bUsingTemplateOffroadVisual)
    {
        // Keep the template's authored paint, glass and tire materials.  The
        // traffic color is still replicated for score/near-miss logic; this
        // vehicle reads as a recognisable SUV rather than a tinted cube.
        UpdateTemplateOffroadVisualGeometry();
    }
    else
    {
        VehicleMesh->SetRelativeScale3D(ReplicatedMeshScale);
        VehicleMesh->SetMaterial(0, VehicleMaterial);
        CabinMesh->SetMaterial(0, CabinMaterial);
        FrontLightBar->SetMaterial(0, FrontLampMaterial);
        RearLightBar->SetMaterial(0, RearLampMaterial);
        for (UStaticMeshComponent* Wheel : { FrontLeftWheel, FrontRightWheel, RearLeftWheel, RearRightWheel })
        {
            Wheel->SetMaterial(0, TireMaterial);
        }
        UpdateStylizedVisualGeometry();
    }
}

void ATrafficVehicleBase::ConfigureTrafficVisualAssetForVariant()
{
    // Do not infer a visual from local state: the server replicates the
    // explicit profile name, which lets every client choose the exact same
    // asset.  The taller truck remains deliberately stylised until it gets a
    // dedicated truck asset.  The imported Offroad asset has incompatible
    // wheel pivots, so SUVs intentionally use the proven SportsCar assembly
    // until that source mesh receives a dedicated fit.
    const bool bShouldUseTemplateOffroad = false;
    const bool bShouldUseTemplateSportsCar = bUsesTemplateSportsCar
        && (ReplicatedTrafficVariantName == TEXT("Commuter")
            || ReplicatedTrafficVariantName == TEXT("Coupe")
            || ReplicatedTrafficVariantName == TEXT("Sport")
            || ReplicatedTrafficVariantName == TEXT("SUV"));
    if (bShouldUseTemplateSportsCar == bUsingTemplateSportsCarVisual
        && bShouldUseTemplateOffroad == bUsingTemplateOffroadVisual)
    {
        return;
    }

    if (bShouldUseTemplateSportsCar)
    {
        if (TemplateSportsCarBodyMesh && TemplateSportsCarGlassMesh && TemplateSportsCarWheelMesh)
        {
            VehicleMesh->EmptyOverrideMaterials();
            CabinMesh->EmptyOverrideMaterials();
            VehicleMesh->SetStaticMesh(TemplateSportsCarBodyMesh);
            CabinMesh->SetStaticMesh(TemplateSportsCarGlassMesh);
            CabinMesh->SetVisibility(true);
            for (UStaticMeshComponent* Wheel : { FrontLeftWheel, FrontRightWheel, RearLeftWheel, RearRightWheel })
            {
                Wheel->EmptyOverrideMaterials();
                Wheel->SetStaticMesh(TemplateSportsCarWheelMesh);
                Wheel->SetVisibility(true);
            }
            FrontLightBar->SetVisibility(false);
            RearLightBar->SetVisibility(false);
            bUsingTemplateSportsCarVisual = true;
            bUsingTemplateOffroadVisual = false;
            return;
        }
    }

    if (bShouldUseTemplateOffroad)
    {
        if (TemplateOffroadBodyMesh && TemplateOffroadTireMesh)
        {
            VehicleMesh->EmptyOverrideMaterials();
            VehicleMesh->SetStaticMesh(TemplateOffroadBodyMesh);
            CabinMesh->SetVisibility(false);
            CabinMesh->EmptyOverrideMaterials();
            for (UStaticMeshComponent* Wheel : { FrontLeftWheel, FrontRightWheel, RearLeftWheel, RearRightWheel })
            {
                Wheel->EmptyOverrideMaterials();
                Wheel->SetStaticMesh(TemplateOffroadTireMesh);
                Wheel->SetVisibility(true);
            }
            FrontLightBar->SetVisibility(false);
            RearLightBar->SetVisibility(false);
            bUsingTemplateSportsCarVisual = false;
            bUsingTemplateOffroadVisual = true;
            return;
        }
    }

    if (PlaceholderBodyMesh)
    {
        VehicleMesh->SetStaticMesh(PlaceholderBodyMesh);
        CabinMesh->SetStaticMesh(PlaceholderBodyMesh);
        FrontLightBar->SetStaticMesh(PlaceholderBodyMesh);
        RearLightBar->SetStaticMesh(PlaceholderBodyMesh);
    }
    if (PlaceholderWheelMesh)
    {
        for (UStaticMeshComponent* Wheel : { FrontLeftWheel, FrontRightWheel, RearLeftWheel, RearRightWheel })
        {
            Wheel->SetStaticMesh(PlaceholderWheelMesh);
        }
    }
    CabinMesh->SetVisibility(true);
    FrontLightBar->SetVisibility(true);
    RearLightBar->SetVisibility(true);
    bUsingTemplateSportsCarVisual = false;
    bUsingTemplateOffroadVisual = false;
}

void ATrafficVehicleBase::UpdateStylizedVisualGeometry()
{
    const FVector Scale = ReplicatedMeshScale;
    const bool bTruck = Scale.Z >= 1.7f;
    const float HalfLength = 50.0f * Scale.X;
    const float HalfWidth = 50.0f * Scale.Y;
    const float HalfHeight = 50.0f * Scale.Z;
    const FVector CabinScale = bTruck
        ? FVector(Scale.X * 0.28f, Scale.Y * 0.80f, Scale.Z * 0.58f)
        : FVector(Scale.X * 0.50f, Scale.Y * 0.72f, Scale.Z * 0.58f);
    const float CabinX = bTruck ? HalfLength - (50.0f * CabinScale.X) - 10.0f : -10.0f * Scale.X;
    const float WheelRadius = FMath::Clamp(Scale.Z * 0.58f, 0.62f, 1.22f);
    const float WheelX = HalfLength * 0.62f;
    const float WheelY = HalfWidth + 13.0f;
    const float WheelZ = -HalfHeight + (50.0f * WheelRadius);

    // A pooled vehicle can previously have been a template car/SUV, both of
    // which align their imported mesh using its source bounds.  Reset the
    // placeholder body to the collision root before applying the truck shape.
    VehicleMesh->SetRelativeLocation(FVector::ZeroVector);
    CabinMesh->SetRelativeLocation(FVector(CabinX, 0.0f, HalfHeight + (50.0f * CabinScale.Z) - 4.0f));
    CabinMesh->SetRelativeScale3D(CabinScale);

    FrontLightBar->SetRelativeLocation(FVector(HalfLength + 2.0f, 0.0f, -HalfHeight * 0.15f));
    FrontLightBar->SetRelativeScale3D(FVector(0.10f, Scale.Y * 0.70f, 0.13f));
    RearLightBar->SetRelativeLocation(FVector(-HalfLength - 2.0f, 0.0f, -HalfHeight * 0.15f));
    RearLightBar->SetRelativeScale3D(FVector(0.10f, Scale.Y * 0.70f, 0.13f));

    FrontLeftWheel->SetRelativeLocation(FVector(WheelX, -WheelY, WheelZ));
    FrontRightWheel->SetRelativeLocation(FVector(WheelX, WheelY, WheelZ));
    RearLeftWheel->SetRelativeLocation(FVector(-WheelX, -WheelY, WheelZ));
    RearRightWheel->SetRelativeLocation(FVector(-WheelX, WheelY, WheelZ));
    for (UStaticMeshComponent* Wheel : { FrontLeftWheel, FrontRightWheel, RearLeftWheel, RearRightWheel })
    {
        // The fallback cylinder is authored with its axle on Z, unlike the
        // template tyres.  Restore its old roll whenever a pooled vehicle
        // switches away from an imported car/SUV profile.
        Wheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
        Wheel->SetRelativeScale3D(FVector(WheelRadius, WheelRadius, 0.38f));
    }
}

void ATrafficVehicleBase::UpdateTemplateSportsCarVisualGeometry()
{
    const FVector CollisionExtent = ReplicatedCollisionExtent;
    const FVector DesiredBodySize(
        FMath::Max(250.0f, CollisionExtent.X * 1.84f),
        FMath::Max(145.0f, CollisionExtent.Y * 1.88f),
        FMath::Max(82.0f, CollisionExtent.Z * 1.46f));
    const UStaticMesh* BodyMesh = VehicleMesh->GetStaticMesh();
    const UStaticMesh* WheelMesh = FrontLeftWheel->GetStaticMesh();
    const FVector BodyScale = CalculateTrafficMeshScaleForSize(BodyMesh, DesiredBodySize);
    const FBoxSphereBounds BodyBounds = BodyMesh ? BodyMesh->GetBounds() : FBoxSphereBounds(EForceInit::ForceInit);
    const FBoxSphereBounds WheelBounds = WheelMesh ? WheelMesh->GetBounds() : FBoxSphereBounds(EForceInit::ForceInit);

    // These are the template SportsCar's authored wheel pivots.  The static
    // wheel is already vertical in the X/Z plane, so keeping the old generic
    // 90 degree roll both turned it sideways and pushed it far outside the
    // body.  Scale the whole assembly from the same source layout instead.
    constexpr float SportsCarFrontWheelX = 135.0f;
    constexpr float SportsCarRearWheelX = -129.2f;
    constexpr float SportsCarWheelY = 90.0f;
    constexpr float SportsCarWheelZ = 25.0f;
    const float WheelScale = BodyScale.X;
    const float BodyMinimumZ = (BodyBounds.Origin.Z - BodyBounds.BoxExtent.Z) * BodyScale.Z;
    const float WheelMinimumZ = (SportsCarWheelZ * BodyScale.Z)
        + ((WheelBounds.Origin.Z - WheelBounds.BoxExtent.Z) * WheelScale);
    const float AssemblyMinimumZ = FMath::Min(BodyMinimumZ, WheelMinimumZ);
    const float AssemblyRootZ = -CollisionExtent.Z + 1.0f - AssemblyMinimumZ;
    const FVector BodyLocation(
        -(BodyBounds.Origin.X * BodyScale.X),
        -(BodyBounds.Origin.Y * BodyScale.Y),
        AssemblyRootZ);
    VehicleMesh->SetRelativeScale3D(BodyScale);
    VehicleMesh->SetRelativeLocation(BodyLocation);
    CabinMesh->SetRelativeScale3D(BodyScale);
    CabinMesh->SetRelativeLocation(BodyLocation);

    for (const TPair<UStaticMeshComponent*, FVector>& Wheel : {
             TPair<UStaticMeshComponent*, FVector>(FrontLeftWheel, FVector(SportsCarFrontWheelX * BodyScale.X, -SportsCarWheelY * BodyScale.Y, AssemblyRootZ + (SportsCarWheelZ * BodyScale.Z))),
             TPair<UStaticMeshComponent*, FVector>(FrontRightWheel, FVector(SportsCarFrontWheelX * BodyScale.X, SportsCarWheelY * BodyScale.Y, AssemblyRootZ + (SportsCarWheelZ * BodyScale.Z))),
             TPair<UStaticMeshComponent*, FVector>(RearLeftWheel, FVector(SportsCarRearWheelX * BodyScale.X, -SportsCarWheelY * BodyScale.Y, AssemblyRootZ + (SportsCarWheelZ * BodyScale.Z))),
             TPair<UStaticMeshComponent*, FVector>(RearRightWheel, FVector(SportsCarRearWheelX * BodyScale.X, SportsCarWheelY * BodyScale.Y, AssemblyRootZ + (SportsCarWheelZ * BodyScale.Z))) })
    {
        Wheel.Key->SetRelativeLocation(Wheel.Value);
        Wheel.Key->SetRelativeRotation(FRotator::ZeroRotator);
        Wheel.Key->SetRelativeScale3D(FVector(WheelScale));
    }
}

void ATrafficVehicleBase::UpdateTemplateOffroadVisualGeometry()
{
    const FVector CollisionExtent = ReplicatedCollisionExtent;
    const FVector DesiredBodySize(
        FMath::Max(340.0f, CollisionExtent.X * 1.86f),
        FMath::Max(175.0f, CollisionExtent.Y * 1.90f),
        FMath::Max(128.0f, CollisionExtent.Z * 1.55f));
    const UStaticMesh* BodyMesh = VehicleMesh->GetStaticMesh();
    const UStaticMesh* WheelMesh = FrontLeftWheel->GetStaticMesh();
    const FVector BodyScale = CalculateTrafficMeshScaleForSize(BodyMesh, DesiredBodySize);
    const FBoxSphereBounds BodyBounds = BodyMesh ? BodyMesh->GetBounds() : FBoxSphereBounds(EForceInit::ForceInit);
    const FBoxSphereBounds WheelBounds = WheelMesh ? WheelMesh->GetBounds() : FBoxSphereBounds(EForceInit::ForceInit);
    const float WheelScale = BodyScale.X;
    const float BodyMinimumZ = (BodyBounds.Origin.Z - BodyBounds.BoxExtent.Z) * BodyScale.Z;
    const float WheelMinimumZ = (WheelBounds.Origin.Z - WheelBounds.BoxExtent.Z) * WheelScale;
    const float AssemblyMinimumZ = FMath::Min(BodyMinimumZ, WheelMinimumZ);
    const float AssemblyRootZ = -CollisionExtent.Z + 1.0f - AssemblyMinimumZ;
    const FVector BodyLocation(
        -(BodyBounds.Origin.X * BodyScale.X),
        -(BodyBounds.Origin.Y * BodyScale.Y),
        AssemblyRootZ);
    VehicleMesh->SetRelativeScale3D(BodyScale);
    VehicleMesh->SetRelativeLocation(BodyLocation);

    const float BodyHalfLength = BodyBounds.BoxExtent.X * BodyScale.X;
    const float BodyHalfWidth = BodyBounds.BoxExtent.Y * BodyScale.Y;
    const float WheelX = BodyHalfLength * 0.62f;
    const float WheelMinimumY = (WheelBounds.Origin.Y - WheelBounds.BoxExtent.Y) * WheelScale;
    const float WheelMaximumY = (WheelBounds.Origin.Y + WheelBounds.BoxExtent.Y) * WheelScale;
    const float PositiveWheelY = BodyHalfWidth - 4.0f - WheelMinimumY;
    const float NegativeWheelY = -BodyHalfWidth + 4.0f - WheelMaximumY;
    for (const TPair<UStaticMeshComponent*, FVector>& Wheel : {
             TPair<UStaticMeshComponent*, FVector>(FrontLeftWheel, FVector(WheelX, NegativeWheelY, AssemblyRootZ)),
             TPair<UStaticMeshComponent*, FVector>(FrontRightWheel, FVector(WheelX, PositiveWheelY, AssemblyRootZ)),
             TPair<UStaticMeshComponent*, FVector>(RearLeftWheel, FVector(-WheelX, NegativeWheelY, AssemblyRootZ)),
             TPair<UStaticMeshComponent*, FVector>(RearRightWheel, FVector(-WheelX, PositiveWheelY, AssemblyRootZ)) })
    {
        Wheel.Key->SetRelativeLocation(Wheel.Value);
        Wheel.Key->SetRelativeRotation(FRotator::ZeroRotator);
        Wheel.Key->SetRelativeScale3D(FVector(WheelScale));
    }
}

void ATrafficVehicleBase::OnRep_TrafficActive()
{
    ApplyTrafficActiveState();
}

void ATrafficVehicleBase::OnRep_TrafficVisualState()
{
    ApplyTrafficVisualState();
}

bool ATrafficVehicleBase::MoveAlongTrafficPath(const FTransform& TargetTransform, float DistanceDelta)
{
    if (ShouldHoldForPlayer(TargetTransform.GetLocation()))
    {
        LaneDistance = FMath::Max(0.0f, LaneDistance - DistanceDelta);
        CurrentSpeed = 0.0f;
        return false;
    }

    SetActorTransform(TargetTransform, false, nullptr, ETeleportType::None);
    return true;
}

bool ATrafficVehicleBase::ShouldHoldForPlayer(const FVector& TargetLocation) const
{
    // Deliberately human-only, and iterating player controllers is how that is
    // enforced. This path is an anti-tunnelling hack for traffic's unswept
    // SetActorTransform movement: it stops the car dead and rewinds its lane
    // distance. The AI rival moves swept against traffic collision, so it needs
    // no equivalent -- and making it visible here would freeze every traffic car
    // within 540 uu of it, deadlocking the pair at a standstill.
    const float ForwardClearance = 540.0f;
    const float LateralClearance = 310.0f;
    if (const UWorld* World = GetWorld())
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            const AOverlaneVehiclePawn* PlayerVehicle = It->Get() ? Cast<AOverlaneVehiclePawn>(It->Get()->GetPawn()) : nullptr;
            if (!PlayerVehicle)
            {
                continue;
            }

            const FVector PlayerLocation = PlayerVehicle->GetActorLocation();
            if (FMath::Abs(TargetLocation.X - PlayerLocation.X) < ForwardClearance
                && FMath::Abs(TargetLocation.Y - PlayerLocation.Y) < LateralClearance)
            {
                return true;
            }
        }
    }

    return false;
}

namespace
{
    /** Stop guessing after this long without a snapshot. */
    constexpr float OverlaneMaxTrafficExtrapolationSeconds = 0.25f;

    /** How fast a post-snapshot visual error is bled off, per second. */
    constexpr float OverlaneTrafficSmoothingRate = 8.0f;

    static TAutoConsoleVariable<int32> CVarExtrapolateTraffic(
        TEXT("overlane.Net.ExtrapolateTraffic"),
        1,
        TEXT("1: clients extrapolate traffic between snapshots. 0: snapshot poses only."),
        ECVF_Default);
}

void ATrafficVehicleBase::OnRep_ReplicatedMovement()
{
    const FVector PreExtrapolatedLocation = GetActorLocation();

    Super::OnRep_ReplicatedMovement();

    // Whatever we had guessed ahead of the server becomes a decaying visual
    // offset, re-applied immediately so the car does not visibly snap backwards
    // every time a packet lands. Clamped so a genuine teleport still reads as one.
    FVector Error = PreExtrapolatedLocation - GetActorLocation();
    Error.Z = 0.0f;
    ClientSmoothingOffset = Error.GetClampedToMaxSize(400.0f);
    ClientExtrapolationElapsed = 0.0f;

    if (!ClientSmoothingOffset.IsNearlyZero())
    {
        AddActorWorldOffset(ClientSmoothingOffset, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

bool ATrafficVehicleBase::ShouldHoldForLocalPlayer() const
{
    const UWorld* World = GetWorld();
    const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
    const AOverlaneVehiclePawn* LocalVehicle = LocalController ? Cast<AOverlaneVehiclePawn>(LocalController->GetPawn()) : nullptr;
    if (!LocalVehicle)
    {
        return false;
    }

    // Same clearances the server uses in ShouldHoldForPlayer.
    const FVector Location = GetActorLocation();
    const FVector PlayerLocation = LocalVehicle->GetActorLocation();
    return FMath::Abs(Location.X - PlayerLocation.X) < 540.0f
        && FMath::Abs(Location.Y - PlayerLocation.Y) < 310.0f;
}

void ATrafficVehicleBase::TickClientExtrapolation(float DeltaSeconds)
{
    if (!bTrafficActive || CVarExtrapolateTraffic.GetValueOnGameThread() == 0)
    {
        return;
    }

    // Bleed off the post-snapshot error first, so extrapolation always builds on
    // the corrected pose rather than compounding an old guess.
    if (!ClientSmoothingOffset.IsNearlyZero())
    {
        const FVector Step = ClientSmoothingOffset * FMath::Clamp(DeltaSeconds * OverlaneTrafficSmoothingRate, 0.0f, 1.0f);
        AddActorWorldOffset(-Step, false, nullptr, ETeleportType::TeleportPhysics);
        ClientSmoothingOffset -= Step;
    }

    if (ClientExtrapolationElapsed >= OverlaneMaxTrafficExtrapolationSeconds)
    {
        // The snapshot is stale enough that guessing is worse than standing still.
        return;
    }

    ClientExtrapolationElapsed += DeltaSeconds;

    const float Speed = ShouldHoldForLocalPlayer() ? 0.0f : static_cast<float>(ReplicatedLaneSpeed);
    if (FMath::IsNearlyZero(Speed))
    {
        return;
    }

    AddActorWorldOffset(GetActorForwardVector() * Speed * DeltaSeconds, false, nullptr, ETeleportType::TeleportPhysics);
}

void ATrafficVehicleBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // The listen server owns all path simulation. Clients never re-run the lane
    // logic -- ShouldHoldForPlayer iterates player controllers, and a client only
    // sees itself, so identical code would give a permanently different answer.
    // They extrapolate the replicated pose instead.
    if (!HasAuthority())
    {
        TickClientExtrapolation(DeltaSeconds);
        return;
    }

    const AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
    if (GameMode && GameMode->IsRacePaused())
    {
        return;
    }

    if (!bTrafficActive || !AssignedLane)
    {
        return;
    }

    const float TargetSpeed = FMath::Min(DesiredSpeed, TrafficSpeedLimit);
    const float ResponseSpeed = CurrentSpeed > TargetSpeed ? 12.0f : 2.5f;
    CurrentSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaSeconds, ResponseSpeed);
    ReplicatedLaneSpeed = static_cast<int16>(FMath::Clamp(CurrentSpeed, -32000.0f, 32000.0f));
    const float DistanceDelta = CurrentSpeed * DeltaSeconds;
    LaneDistance += DistanceDelta;
    if (LaneDistance >= AssignedLane->GetLaneLength())
    {
        DeactivateForPool();
        return;
    }

    if (bChangingLane && LaneChangeTarget)
    {
        LaneChangeElapsed += DeltaSeconds;
        const float Alpha = FMath::Clamp(LaneChangeElapsed / LaneChangeDuration, 0.0f, 1.0f);
        const FTransform SourceTransform = AssignedLane->GetTransformAtDistance(LaneDistance);
        const FTransform TargetTransform = LaneChangeTarget->GetTransformAtDistance(LaneDistance);
        FTransform BlendedTransform;
        BlendedTransform.SetLocation(FMath::Lerp(SourceTransform.GetLocation(), TargetTransform.GetLocation(), Alpha));
        BlendedTransform.SetRotation(FQuat::Slerp(SourceTransform.GetRotation(), TargetTransform.GetRotation(), Alpha));
        BlendedTransform.SetScale3D(FVector::OneVector);
        if (!MoveAlongTrafficPath(BlendedTransform, DistanceDelta))
        {
            LaneChangeElapsed = FMath::Max(0.0f, LaneChangeElapsed - DeltaSeconds);
            return;
        }

        if (Alpha >= 1.0f)
        {
            AssignedLane = LaneChangeTarget;
            LaneChangeTarget = nullptr;
            bChangingLane = false;
        }
        return;
    }

    MoveAlongTrafficPath(AssignedLane->GetTransformAtDistance(LaneDistance), DistanceDelta);
}

void ATrafficVehicleBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATrafficVehicleBase, bTrafficActive);
    DOREPLIFETIME(ATrafficVehicleBase, ReplicatedTrafficColor);
    DOREPLIFETIME(ATrafficVehicleBase, ReplicatedMeshScale);
    DOREPLIFETIME(ATrafficVehicleBase, ReplicatedCollisionExtent);
    DOREPLIFETIME(ATrafficVehicleBase, ReplicatedTrafficVariantName);
    DOREPLIFETIME(ATrafficVehicleBase, ReplicatedLaneSpeed);
}
