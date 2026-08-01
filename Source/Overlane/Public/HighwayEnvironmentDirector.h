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

    /**
     * Build a material for one surface class.
     *
     * Falls back to the flat colour material when no override is assigned, so
     * the environment keeps working with nothing plugged in. When an override IS
     * assigned it is passed the surface tint plus the road geometry, so a
     * world-aligned master material can place wheel-polish wear lanes without
     * needing its own copy of the layout numbers.
     */
    UMaterialInstanceDynamic* CreateSurfaceMaterial(UMaterialInterface* Override, const FLinearColor& Tint) const;
    void BuildVisualRoute();
    void AddLaneMarkings();
    /** Instances the posts and beam segments that replaced the stretched rail box. */
    void BuildGuardRail();

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

    /**
     * Per-surface material overrides.
     *
     * Everything in this environment previously rendered through a single
     * instance of /Engine/BasicShapes/BasicShapeMaterial that differed only in a
     * Color parameter - asphalt, painted steel, concrete, glass and tree bark all
     * with identical roughness, no normal map and no AO. That is literally the
     * shading model of an injection-moulded toy: one surface, many dyes.
     *
     * These slots let a proper material be plugged in per surface class without
     * touching the generator. Leave any of them empty and that surface keeps the
     * old flat-colour behaviour.
     *
     * The road slot in particular wants a WORLD-ALIGNED material: RoadSurface is
     * an engine cube at scale (6300, 20, 0.07), and the cube has one 0-1 UV set
     * per face, so a UV-sampled texture stretches 6.3 km by 20 m - roughly 315:1.
     * The road has never carried a texture because the geometry cannot carry one.
     */
    /**
     * Lateral spacing between traffic lane centres, in world units.
     *
     * Must match where ATrafficLanePath actors are actually placed (Y = -600 /
     * 0 / +600), because the road material uses it to position wheel-polish wear
     * lanes. It is NOT RoadWidth / lane count: the road is wider than the lanes.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Highway|Rendering|Surfaces", meta = (ClampMin = "1.0"))
    float TrafficLaneSpacing = 600.0f;

    /**
     * Distant terrain on the horizon.
     *
     * Turn it off if a bare fogged horizon reads better than approximate hills -
     * an empty horizon is honest, whereas hills that do not convince are worse
     * than none. Kept on by default because the route is dead flat and the
     * horizon otherwise has nothing in it at all.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Highway|Rendering")
    bool bShowDistantHills = true;

    UPROPERTY(EditDefaultsOnly, Category = "Highway|Rendering|Surfaces")
    TObjectPtr<UMaterialInterface> RoadSurfaceMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Highway|Rendering|Surfaces")
    TObjectPtr<UMaterialInterface> ShoulderSurfaceMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Highway|Rendering|Surfaces")
    TObjectPtr<UMaterialInterface> ConcreteSurfaceMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Highway|Rendering|Surfaces")
    TObjectPtr<UMaterialInterface> MetalSurfaceMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Highway|Rendering|Surfaces")
    TObjectPtr<UMaterialInterface> TerrainSurfaceMaterial;

    /**
     * ambientCG photogrammetry scans, world-aligned, built by BuildScannedSurfaces.py.
     *
     * Procedural noise was enough for the barrier because concrete really is a
     * homogeneous material. Ground is not: its variation is blotchy, clustered and
     * directional, and no noise function reproduces that - which is why the verge and
     * the grass still read as flat colour blocks after everything else improved.
     *
     * Optional on purpose. If the scans are absent these stay null and the flat-tint
     * path below runs exactly as before, so the project builds without them.
     */
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> VergeScanMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> GrassScanMaterial;

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

    /**
     * Bare-earth verge between the barrier and the grass.
     *
     * Without it the concrete meets saturated green on a single hard line running
     * the whole 6 km, and a hard line between two flat colours is one of the
     * strongest "untextured prototype" cues there is - the eye reads it as a
     * material boundary rather than as ground. Real motorways always have a scuffed,
     * desaturated transition here, and giving it its own strip also breaks up the
     * enormous uniform green mass either side of the route.
     */
    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UStaticMeshComponent> LeftVerge;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UStaticMeshComponent> RightVerge;

    /**
     * The guardrail as posts and beam segments rather than one stretched box.
     *
     * It used to be two UStaticMeshComponents scaled to (6300, 0.12, 0.09) - a single
     * unbroken 6.3 km ribbon 9 cm tall. Silhouette is the tell no texture can fix: a
     * real barrier reads as a repeating rhythm of posts with a beam running between
     * them, and at 245 km/h that rhythm is most of the sensation of speed. A
     * continuous ribbon gives the eye nothing to clock against, which is a large part
     * of why the road has read as a toy.
     *
     * Instanced rather than one mesh each, so the whole run is still two draw calls.
     */
    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UInstancedStaticMeshComponent> GuardRailPosts;

    UPROPERTY(VisibleAnywhere, Category = "Highway")
    TObjectPtr<UInstancedStaticMeshComponent> GuardRailBeams;

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
