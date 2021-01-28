// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AProjectile.h"
#include "SWeapon.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include <Runtime\Engine\Classes\Kismet\GameplayStatics.h>
#include <ProjectReplicant\Public\SCharacter.h>
#include <ProjectReplicant\CoopGame.h>

AProjectile::AProjectile()
{
	// Use a sphere as a simple collision representation
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(5.0f);
	CollisionComp->SetCollisionProfileName("Weapon");
	CollisionComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
	CollisionComp->OnComponentHit.AddDynamic(this, &AProjectile::OnHit);	// set up a notification for when this component hits something blocking
	this->CollisionComp = this->CollisionComp;

	// Players can't walk on it
	CollisionComp->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
	CollisionComp->CanCharacterStepUpOn = ECB_No;

	// Set as root component
	RootComponent = CollisionComp;

	// Use a ProjectileMovementComponent to govern this projectile's movement
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 15000.f;
	ProjectileMovement->MaxSpeed = 15000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;

	// Die after 3 seconds by default
	InitialLifeSpan = 3.0f;

	SetReplicates(true);
	SetReplicateMovement(true);
}

void AProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	AActor* MyOwner = this;
	AActor* ProjectileOwner = this->GetOwner();
	ASCharacter* SCharacter = Cast<ASCharacter>(ProjectileOwner);
	if (SCharacter)
	{
		this->TeamNum = SCharacter->TeamNum;
	}

	if (this->ProjectileType == ProjectileType::Projectile)
	{
		if ((OtherActor != NULL) && (OtherActor != this) && (OtherComp != NULL))
		{
			AActor* HitActor = OtherActor;

			UGameplayStatics::ApplyDamage(HitActor, BaseDamage, MyOwner->GetInstigatorController(), MyOwner, DamageType);
		}
	}

	//UGameplayStatics::ApplyRadialDamage(const UObject* WorldContextObject, float BaseDamage, const FVector& Origin, float DamageRadius, TSubclassOf<UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser, AController* InstigatedByController, bool bDoFullDamage, ECollisionChannel DamagePreventionChannel )
	//Conditionally deal AE damage based on projectile type. 
	if (ProjectileType == ProjectileType::AOE)
	{
		UWorld* world = GetWorld();
		const  TArray<AActor*> IgnoreActors;
		AController* EventInstigator = GetInstigatorController();
		UGameplayStatics::ApplyRadialDamage(world, BaseDamage, GetActorLocation(), AOERadius, DamageType, IgnoreActors, MyOwner, EventInstigator, true);
	}

	EPhysicalSurface SurfaceType = SurfaceType_Default;
	SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
	ASWeapon* currentWeapon = SCharacter->GetCurrentWeapon();
	UParticleSystem* FleshImpactEffect = currentWeapon->GetFleshImpactEffect();;
	UParticleSystem* DefaultImpactEffect = currentWeapon->GetDefaultImpactEffect();

	if (currentWeapon)
	{
		PlayImpactEffects(SurfaceType, Hit.ImpactPoint, DefaultImpactEffect, FleshImpactEffect);
	}

	this->Destroy();
}

void AProjectile::PlayImpactEffects(EPhysicalSurface SurfaceType, FVector ImpactPoint, UParticleSystem* DefaultImpactEffect, UParticleSystem* FleshImpactEffect)
{
	UParticleSystem* SelectedEffect = nullptr;
	switch (SurfaceType)
	{
	case SURFACE_FLESHDEFAULT:
	case SURFACE_FLESHVULNERABLE:
		SelectedEffect = FleshImpactEffect;
		break;
	default:
		SelectedEffect = DefaultImpactEffect;
		break;
	}

	if (SelectedEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelectedEffect, ImpactPoint);
	}
}