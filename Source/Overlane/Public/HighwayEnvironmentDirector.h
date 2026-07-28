#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HighwayEnvironmentDirector.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPostProcessComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A deterministic, local visual layer for the graybox route.
 *
 * It intentionally owns no collision and is spawned once in each local game
 * world. That makes the 6 km route look identical in standalone, listen-server
 * host, and client windows without adding scenery replication traffic.
 */
UCLASS()
class OVERLANE_API AHighwayEnvironmentDirector : public AActor
{
    GENERATED_BODY()

public:
    AHighwayEnvironmentDirector();

protected:
    virtual void BeginPlay() override;

private:
    UMaterialInstanceDynamic* CreateColorMaterial(const FLinearColor& Color) const;
    void BuildVisualRoute();
    void AddLaneMarkings();
    void AddRoadsideFurniture();
    void AddHeroDistrict();
    void AddHeroServiceAreas();
    void AddInterchangeSetPiece();
    void AddProductionArchitecture();
    void AddDistantLandscape();

    void ConfigureVisualMesh(UStaticMeshComponent* Component) const;
    void ConfigureVisualInstances(UInstancedStaticMeshComponent* Component) const;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<USceneComponent> SceneRoot;

    // A local, unbound exposure guard keeps the bright template sky from
    // washing out traffic and roadside silhouettes. It has no gameplay or
    // replication role.
    UPROPERTY(VisibleAnywhere, Category = "Highway|Rendering")
    TObjectPtr<UPostProcessComponent> VisualPostProcess;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UStaticMeshComponent> LeftLandscape;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UStaticMeshComponent> RightLandscape;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UStaticMeshComponent> RoadSurface;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UStaticMeshComponent> LeftShoulder;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UStaticMeshComponent> RightShoulder;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UStaticMeshComponent> LeftGuardRail;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UStaticMeshComponent> RightGuardRail;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UInstancedStaticMeshComponent> LaneDashes;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UInstancedStaticMeshComponent> EdgeReflectors;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UInstancedStaticMeshComponent> LampPoles;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UInstancedStaticMeshComponent> LampArms;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UInstancedStaticMeshComponent> LampHeads;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UInstancedStaticMeshComponent> SignPosts;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UInstancedStaticMeshComponent> SignBoards;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UInstancedStaticMeshComponent> DistantHills;

    UPROPERTY(VisibleAnywhere, Category = "Highway|HeroDistrict")
    TObjectPtr<UInstancedStaticMeshComponent> HeroStreetLights;

    UPROPERTY(VisibleAnywhere, Category = "Highway|HeroDistrict")
    TObjectPtr<UInstancedStaticMeshComponent> HeroTrees;

    UPROPERTY(VisibleAnywhere, Category = "Highway|HeroDistrict")
    TObjectPtr<UInstancedStaticMeshComponent> HeroBuildings;

    // These components form a deliberately non-interactive visual rest-stop
    // layer.  The playable lane remains the original graybox collision route.
    UPROPERTY(VisibleAnywhere, Category = "Highway|HeroDistrict")
    TObjectPtr<UInstancedStaticMeshComponent> HeroServicePads;

    UPROPERTY(VisibleAnywhere, Category = "Highway|HeroDistrict")
    TObjectPtr<UInstancedStaticMeshComponent> HeroServiceCanopies;

    UPROPERTY(VisibleAnywhere, Category = "Highway|HeroDistrict")
    TObjectPtr<UInstancedStaticMeshComponent> HeroServiceColumns;

    UPROPERTY(VisibleAnywhere, Category = "Highway|HeroDistrict")
    TObjectPtr<UInstancedStaticMeshComponent> HeroFuelPumps;

    UPROPERTY(VisibleAnywhere, Category = "Highway|HeroDistrict")
    TObjectPtr<UInstancedStaticMeshComponent> HeroServiceSigns;

    // A single locally staged ConceptCar is used as a non-interactive showroom
    // landmark at the fuel stop.  It deliberately remains separate from the
    // instanced traffic visual system and cannot affect gameplay collision.
    UPROPERTY(VisibleAnywhere, Category = "Highway|HeroDistrict")
    TObjectPtr<UStaticMeshComponent> HeroShowcaseCar;

    // The interchange is deliberately a visual-only set piece.  It sits over
    // the existing playable corridor, so it can make the first long run feel
    // like a real motorway approach without changing traffic or collision.
    UPROPERTY(VisibleAnywhere, Category = "Highway|Interchange")
    TObjectPtr<UInstancedStaticMeshComponent> InterchangeDecks;

    UPROPERTY(VisibleAnywhere, Category = "Highway|Interchange")
    TObjectPtr<UInstancedStaticMeshComponent> InterchangeColumns;

    UPROPERTY(VisibleAnywhere, Category = "Highway|Interchange")
    TObjectPtr<UInstancedStaticMeshComponent> InterchangeRails;

    UPROPERTY(VisibleAnywhere, Category = "Highway|Interchange")
    TObjectPtr<UInstancedStaticMeshComponent> InterchangeSigns;

    UPROPERTY(VisibleAnywhere, Category = "Highway|Interchange")
    TObjectPtr<UInstancedStaticMeshComponent> InterchangeCityBlocks;

    UPROPERTY(VisibleAnywhere, Category = "Highway|Interchange")
    TObjectPtr<UInstancedStaticMeshComponent> InterchangeBuildings;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> CubeMesh;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> CylinderMesh;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> SphereMesh;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> ConeMesh;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> BaseMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Highway")
    float RouteStartX = -300000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Highway")
    float RouteLength = 600000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Highway")
    float RoadWidth = 2000.0f;

    bool bBuilt = false;
};
