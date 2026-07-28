#include "OverlaneVehiclePawn.h"

#include "ArcadeHandlingComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "OverlaneGameModeBase.h"
#include "TrafficVehicleBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FVector CalculateMeshScaleForSize(const FBoxSphereBounds& MeshBounds, const FVector& DesiredSize)
{
    const FVector SourceSize = MeshBounds.BoxExtent * 2.0f;
    const float UniformScale = FMath::Min3(
        DesiredSize.X / FMath::Max(SourceSize.X, 1.0f),
        DesiredSize.Y / FMath::Max(SourceSize.Y, 1.0f),
        DesiredSize.Z / FMath::Max(SourceSize.Z, 1.0f));
    return FVector(UniformScale);
}

}

AOverlaneVehiclePawn::AOverlaneVehiclePawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(60.0f);
    SetMinNetUpdateFrequency(30.0f);

    VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
    VehicleCollision->SetBoxExtent(FVector(200.0f, 90.0f, 60.0f));
    VehicleCollision->SetCollisionProfileName(TEXT("Pawn"));
    VehicleCollision->SetNotifyRigidBodyCollision(true);
    SetRootComponent(VehicleCollision);

    VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));
    VehicleMesh->SetupAttachment(VehicleCollision);
    VehicleMesh->SetRelativeScale3D(FVector(4.2f, 1.9f, 0.68f));
    VehicleMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -5.0f));
    VehicleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CabinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinMesh"));
    CabinMesh->SetupAttachment(VehicleCollision);
    CabinMesh->SetRelativeLocation(FVector(-45.0f, 0.0f, 75.0f));
    CabinMesh->SetRelativeScale3D(FVector(2.05f, 1.45f, 0.65f));
    CabinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FrontLightBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontLightBar"));
    FrontLightBar->SetupAttachment(VehicleCollision);
    FrontLightBar->SetRelativeLocation(FVector(212.0f, 0.0f, -8.0f));
    FrontLightBar->SetRelativeScale3D(FVector(0.12f, 1.42f, 0.14f));
    FrontLightBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RearLightBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RearLightBar"));
    RearLightBar->SetupAttachment(VehicleCollision);
    RearLightBar->SetRelativeLocation(FVector(-212.0f, 0.0f, -8.0f));
    RearLightBar->SetRelativeScale3D(FVector(0.12f, 1.42f, 0.14f));
    RearLightBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FrontLeftWheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontLeftWheel"));
    FrontLeftWheel->SetupAttachment(VehicleCollision);
    FrontLeftWheel->SetRelativeLocation(FVector(125.0f, -108.0f, -35.0f));
    FrontLeftWheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
    FrontLeftWheel->SetRelativeScale3D(FVector(0.80f, 0.80f, 0.38f));
    FrontLeftWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FrontRightWheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontRightWheel"));
    FrontRightWheel->SetupAttachment(VehicleCollision);
    FrontRightWheel->SetRelativeLocation(FVector(125.0f, 108.0f, -35.0f));
    FrontRightWheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
    FrontRightWheel->SetRelativeScale3D(FVector(0.80f, 0.80f, 0.38f));
    FrontRightWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RearLeftWheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RearLeftWheel"));
    RearLeftWheel->SetupAttachment(VehicleCollision);
    RearLeftWheel->SetRelativeLocation(FVector(-125.0f, -108.0f, -35.0f));
    RearLeftWheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
    RearLeftWheel->SetRelativeScale3D(FVector(0.80f, 0.80f, 0.38f));
    RearLeftWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RearRightWheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RearRightWheel"));
    RearRightWheel->SetupAttachment(VehicleCollision);
    RearRightWheel->SetRelativeLocation(FVector(-125.0f, 108.0f, -35.0f));
    RearRightWheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
    RearRightWheel->SetRelativeScale3D(FVector(0.80f, 0.80f, 0.38f));
    RearRightWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Use the template's proven static pieces rather than its skeletal rig.  The
    // skeletal asset requires a vehicle animation setup that this arcade pawn
    // deliberately does not own; the static pieces render on every local/network
    // pawn while the collision/handling root stays untouched.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SportsCarBodyMesh(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar.SM_SportsCar"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SportsCarGlassMesh(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Glass.SM_SportsCar_Glass"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SportsCarWheelMesh(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel"));
    const bool bUsesTemplateSportsCar = SportsCarBodyMesh.Succeeded() && SportsCarGlassMesh.Succeeded() && SportsCarWheelMesh.Succeeded();

    if (bUsesTemplateSportsCar)
    {
        const FVector CollisionExtent = VehicleCollision->GetUnscaledBoxExtent();
        const FBoxSphereBounds BodyBounds = SportsCarBodyMesh.Object->GetBounds();
        const FVector BodyScale = CalculateMeshScaleForSize(BodyBounds, FVector(390.0f, 174.0f, 104.0f));
        const FBoxSphereBounds WheelBounds = SportsCarWheelMesh.Object->GetBounds();

        // The SportsCar static pieces retain the coordinate system of the
        // template skeletal car.  In that source the wheel pivots are at
        // (+/-135, +/-90, 25), and the tyre already lies in the X/Z plane.
        // Using a generic collision-box offset and an extra 90 degree roll
        // moved the wheels outside the arches and turned them sideways.
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
        VehicleMesh->SetStaticMesh(SportsCarBodyMesh.Object);
        VehicleMesh->SetRelativeScale3D(BodyScale);
        VehicleMesh->SetRelativeLocation(BodyLocation);

        CabinMesh->SetStaticMesh(SportsCarGlassMesh.Object);
        CabinMesh->SetRelativeScale3D(BodyScale);
        CabinMesh->SetRelativeLocation(BodyLocation);

        for (const TPair<UStaticMeshComponent*, FVector>& Wheel : {
                 TPair<UStaticMeshComponent*, FVector>(FrontLeftWheel, FVector(SportsCarFrontWheelX * BodyScale.X, -SportsCarWheelY * BodyScale.Y, AssemblyRootZ + (SportsCarWheelZ * BodyScale.Z))),
                 TPair<UStaticMeshComponent*, FVector>(FrontRightWheel, FVector(SportsCarFrontWheelX * BodyScale.X, SportsCarWheelY * BodyScale.Y, AssemblyRootZ + (SportsCarWheelZ * BodyScale.Z))),
                 TPair<UStaticMeshComponent*, FVector>(RearLeftWheel, FVector(SportsCarRearWheelX * BodyScale.X, -SportsCarWheelY * BodyScale.Y, AssemblyRootZ + (SportsCarWheelZ * BodyScale.Z))),
                 TPair<UStaticMeshComponent*, FVector>(RearRightWheel, FVector(SportsCarRearWheelX * BodyScale.X, SportsCarWheelY * BodyScale.Y, AssemblyRootZ + (SportsCarWheelZ * BodyScale.Z))) })
        {
            Wheel.Key->SetStaticMesh(SportsCarWheelMesh.Object);
            Wheel.Key->SetRelativeLocation(Wheel.Value);
            Wheel.Key->SetRelativeRotation(FRotator::ZeroRotator);
            Wheel.Key->SetRelativeScale3D(FVector(WheelScale));
        }

        FrontLightBar->SetVisibility(false);
        RearLightBar->SetVisibility(false);
    }
    else
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (PlaceholderMesh.Succeeded())
        {
            VehicleMesh->SetStaticMesh(PlaceholderMesh.Object);
            CabinMesh->SetStaticMesh(PlaceholderMesh.Object);
            FrontLightBar->SetStaticMesh(PlaceholderMesh.Object);
            RearLightBar->SetStaticMesh(PlaceholderMesh.Object);
        }

        static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        if (WheelMesh.Succeeded())
        {
            FrontLeftWheel->SetStaticMesh(WheelMesh.Object);
            FrontRightWheel->SetStaticMesh(WheelMesh.Object);
            RearLeftWheel->SetStaticMesh(WheelMesh.Object);
            RearRightWheel->SetStaticMesh(WheelMesh.Object);
        }
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlaceholderMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (PlaceholderMaterial.Succeeded())
    {
        VehicleBodyMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial.Object, this);
        CabinMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial.Object, this);
        TireMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial.Object, this);
        FrontLampMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial.Object, this);
        RearLampMaterial = UMaterialInstanceDynamic::Create(PlaceholderMaterial.Object, this);

        VehicleBodyMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.24f, 0.52f));
        CabinMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.055f, 0.10f));
        TireMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.012f, 0.014f, 0.018f));
        FrontLampMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.48f, 0.90f, 1.0f));
        RearLampMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.035f, 0.02f));

        if (!bUsesTemplateSportsCar)
        {
            VehicleMesh->SetMaterial(0, VehicleBodyMaterial);
            CabinMesh->SetMaterial(0, CabinMaterial);
            FrontLightBar->SetMaterial(0, FrontLampMaterial);
            RearLightBar->SetMaterial(0, RearLampMaterial);
            for (UStaticMeshComponent* Wheel : { FrontLeftWheel, FrontRightWheel, RearLeftWheel, RearRightWheel })
            {
                Wheel->SetMaterial(0, TireMaterial);
            }
        }
    }

    ArcadeHandling = CreateDefaultSubobject<UArcadeHandlingComponent>(TEXT("ArcadeHandling"));

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(VehicleCollision);
    CameraBoom->TargetArmLength = BaseCameraDistance;
    CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));
    CameraBoom->SetRelativeRotation(FRotator(-12.0f, 0.0f, 0.0f));
    CameraBoom->bDoCollisionTest = true;

    ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
    ChaseCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    ChaseCamera->bUsePawnControlRotation = false;
    ChaseCamera->SetFieldOfView(BaseCameraFov);
}

void AOverlaneVehiclePawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    for (auto ContactIt = ActiveTrafficCollisionContacts.CreateIterator(); ContactIt; ++ContactIt)
    {
        ATrafficVehicleBase* TrafficVehicle = ContactIt->Get();
        if (!TrafficVehicle || FVector::DistSquared2D(GetActorLocation(), TrafficVehicle->GetActorLocation()) > FMath::Square(CollisionReleaseDistance))
        {
            ContactIt.RemoveCurrent();
        }
    }

    TrafficImpactFeedbackRemaining = FMath::Max(0.0f, TrafficImpactFeedbackRemaining - DeltaSeconds);
    NearMissFeedbackRemaining = FMath::Max(0.0f, NearMissFeedbackRemaining - DeltaSeconds);

    if (HasAuthority())
    {
        ReplicatedSpeedKph = ArcadeHandling->GetSpeedKph();
        ReplicatedBoostCharge = ArcadeHandling->GetBoostChargeRatio();
        bReplicatedBoostActive = ArcadeHandling->IsBoostActive();
        OwnerServerTransform = GetActorTransform();
    }

    const float SpeedRatio = FMath::Clamp(GetSpeedKph() / 245.0f, 0.0f, 1.0f);
    const float TurboCameraKick = IsBoostActive() ? 7.0f : 0.0f;
    const float TargetDistance = FMath::Lerp(BaseCameraDistance, MaxCameraDistance + (IsBoostActive() ? 135.0f : 0.0f), SpeedRatio);
    const float TargetFov = FMath::Lerp(BaseCameraFov + CameraFovOffset, MaxCameraFov + CameraFovOffset + TurboCameraKick, SpeedRatio);
    CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetDistance, DeltaSeconds, CameraResponseSpeed);
    ChaseCamera->SetFieldOfView(FMath::FInterpTo(ChaseCamera->FieldOfView, TargetFov, DeltaSeconds, CameraResponseSpeed));
}

float AOverlaneVehiclePawn::GetSpeedKph() const
{
    return !HasAuthority() && GetNetMode() != NM_Standalone ? ReplicatedSpeedKph : ArcadeHandling->GetSpeedKph();
}

float AOverlaneVehiclePawn::GetBoostChargeRatio() const
{
    return !HasAuthority() && GetNetMode() != NM_Standalone ? ReplicatedBoostCharge : ArcadeHandling->GetBoostChargeRatio();
}

bool AOverlaneVehiclePawn::IsBoostActive() const
{
    return !HasAuthority() && GetNetMode() != NM_Standalone ? bReplicatedBoostActive : ArcadeHandling->IsBoostActive();
}

void AOverlaneVehiclePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AOverlaneVehiclePawn, bIsAIRacer);
    DOREPLIFETIME(AOverlaneVehiclePawn, ReplicatedSpeedKph);
    DOREPLIFETIME_CONDITION(AOverlaneVehiclePawn, ReplicatedBoostCharge, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AOverlaneVehiclePawn, bReplicatedBoostActive, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AOverlaneVehiclePawn, OwnerServerTransform, COND_OwnerOnly);
}

void AOverlaneVehiclePawn::OnRep_OwnerServerTransform()
{
    if (!HasAuthority())
    {
        SetActorTransform(OwnerServerTransform, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

void AOverlaneVehiclePawn::SetAIRacer(bool bInIsAIRacer)
{
    bIsAIRacer = bInIsAIRacer;

    // Crimson is unused by the player (blue) and by every traffic profile
    // (orange/blue/yellow/green), so the rival is unmistakable at 100 m.
    if (bIsAIRacer)
    {
        SetBodyColor(FLinearColor(0.86f, 0.12f, 0.06f));
    }
}

float AOverlaneVehiclePawn::GetForwardSpeedCms() const
{
    return ArcadeHandling->GetForwardSpeed();
}

void AOverlaneVehiclePawn::EnqueueInputCommand(const FOverlaneInputCommand& Command)
{
    ArcadeHandling->EnqueueCommand(Command);
}

void AOverlaneVehiclePawn::ClearPendingInputCommands()
{
    ArcadeHandling->ClearPendingCommands();
}

void AOverlaneVehiclePawn::SetPerformanceScale(float InScale)
{
    ArcadeHandling->SetPerformanceScale(InScale);
}

void AOverlaneVehiclePawn::SetBodyColor(const FLinearColor& InColor)
{
    if (!VehicleBodyMaterial || !VehicleMesh)
    {
        return;
    }

    VehicleBodyMaterial->SetVectorParameterValue(TEXT("Color"), InColor);

    // The constructor only binds VehicleBodyMaterial to the mesh on the
    // placeholder path; with the template sports car present the pawn renders
    // the template's own materials, so setting the parameter alone does nothing.
    // Assign the dynamic material to every slot, as the traffic vehicles do.
    for (int32 MaterialIndex = 0; MaterialIndex < VehicleMesh->GetNumMaterials(); ++MaterialIndex)
    {
        VehicleMesh->SetMaterial(MaterialIndex, VehicleBodyMaterial);
    }
}

void AOverlaneVehiclePawn::RegisterRivalImpact()
{
    TrafficImpactFeedbackRemaining = TrafficImpactFeedbackDuration;
    TrafficImpactFeedbackColor = FLinearColor(1.0f, 0.78f, 0.28f);
    bLastImpactWasRival = true;
}

void AOverlaneVehiclePawn::RecoverToStart()
{
    ArcadeHandling->ResetState();
    SetActorTransform(RecoveryTransform, false, nullptr, ETeleportType::TeleportPhysics);
}

void AOverlaneVehiclePawn::StopDriving()
{
    ArcadeHandling->ResetState();
}

void AOverlaneVehiclePawn::SetCameraFovOffset(float InOffset)
{
    CameraFovOffset = FMath::Clamp(InOffset, -10.0f, 10.0f);
}

void AOverlaneVehiclePawn::BeginPlay()
{
    Super::BeginPlay();

    VehicleCollision->OnComponentHit.AddDynamic(this, &AOverlaneVehiclePawn::HandleVehicleHit);

    // The road is a very large, thin static mesh. It should support the vehicle
    // visually but not participate in its horizontal sweep; barriers remain
    // blocking because they are tall, narrow static meshes.
    TArray<AActor*> StaticMeshActors;
    UGameplayStatics::GetAllActorsOfClass(this, AStaticMeshActor::StaticClass(), StaticMeshActors);
    for (AActor* Actor : StaticMeshActors)
    {
        AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);
        UStaticMeshComponent* StaticMesh = StaticMeshActor ? StaticMeshActor->GetStaticMeshComponent() : nullptr;
        if (!StaticMesh)
        {
            continue;
        }

        const FVector BoundsExtent = StaticMesh->Bounds.BoxExtent;
        const bool bIsLongThinRoad = BoundsExtent.X >= 100000.0f && BoundsExtent.Z < BoundsExtent.Y * 0.5f;
        if (bIsLongThinRoad)
        {
            StaticMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        }
    }

    FHitResult GroundHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OverlaneVehicleGroundSnap), false, this);
    const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 500.0f);
    const FVector End = GetActorLocation() - FVector(0.0f, 0.0f, 5000.0f);
    if (GetWorld()->LineTraceSingleByChannel(GroundHit, Start, End, ECC_Visibility, QueryParams))
    {
        SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, GroundHit.ImpactPoint.Z + VehicleCollision->GetScaledBoxExtent().Z));
    }

    RecoveryTransform = GetActorTransform();
}

void AOverlaneVehiclePawn::HandleVehicleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (ATrafficVehicleBase* TrafficVehicle = Cast<ATrafficVehicleBase>(OtherActor))
    {
        RegisterTrafficImpact(TrafficVehicle);
        return;
    }

    // Racer-on-racer contact is deliberately NOT a traffic collision: it must not
    // cost the human the 750-point penalty for something the rival initiated.
    if (Cast<AOverlaneVehiclePawn>(OtherActor))
    {
        RegisterRivalImpact();
    }
}

void AOverlaneVehiclePawn::RegisterTrafficImpact(ATrafficVehicleBase* TrafficVehicle)
{
    if (!TrafficVehicle)
    {
        return;
    }

    TrafficImpactFeedbackRemaining = TrafficImpactFeedbackDuration;
    TrafficImpactFeedbackColor = TrafficVehicle->GetTrafficColor();
    bLastImpactWasRival = false;

    // An AI rival must never touch human race state.  MarkPlayerCollision would
    // permanently disarm this traffic car's near-miss encounter, so the bot would
    // silently deny the human points just by driving near the same traffic.
    if (!bIsAIRacer)
    {
        TrafficVehicle->MarkPlayerCollision();
    }

    if (ActiveTrafficCollisionContacts.Contains(TrafficVehicle))
    {
        return;
    }

    ActiveTrafficCollisionContacts.Add(TrafficVehicle);
    if (bIsAIRacer)
    {
        return;
    }

    if (AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr)
    {
        GameMode->RegisterTrafficCollision();
    }
}

bool AOverlaneVehiclePawn::TryRegisterNearMiss(const FLinearColor& InColor)
{
    // A bot threading traffic must not award the human near-miss points.
    if (bIsAIRacer)
    {
        return false;
    }

    AOverlaneGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOverlaneGameModeBase>() : nullptr;
    if (!GameMode || !GameMode->IsRaceActive() || GameMode->IsRacePaused())
    {
        return false;
    }

    ++NearMissCount;
    NearMissFeedbackColor = InColor;
    NearMissFeedbackRemaining = NearMissFeedbackDuration;
    GameMode->RegisterNearMiss();
    return true;
}

void AOverlaneVehiclePawn::SetThrottleInput(float Value)
{
    ArcadeHandling->SetThrottleInput(Value);
}

void AOverlaneVehiclePawn::SetBrakeInput(float Value)
{
    ArcadeHandling->SetBrakeInput(Value);
}

void AOverlaneVehiclePawn::SetSteeringInput(float Value)
{
    ArcadeHandling->SetSteeringInput(Value);
}

void AOverlaneVehiclePawn::SetBoostInput(bool bEnabled)
{
    ArcadeHandling->SetBoostInput(bEnabled);
}
