// Fill out your copyright notice in the Description page of Project Settings.

#include "SWeapon.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "CoopGame.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "AProjectile.h"
#include <ProjectReplicant\Public\SCharacter.h>
#include "Animation/AnimInstance.h"
#include "Components/BoxComponent.h"

// Sets default values
ASWeapon::ASWeapon()
{
	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCollisionProfileName("Weapon");
	RootComponent = MeshComp;

	MuzzleSocketName = "MuzzleSocket";
	TracerTargetName = "Target";

	CollisionComp = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComp"));
	CollisionComp->SetupAttachment(RootComponent, MuzzleSocketName);
	//uncomment below to see hitbox
	//CollisionComp->bHiddenInGame = false;
	CollisionComp->InitBoxExtent(FVector(15, 20, 50));
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ASWeapon::OnWeaponOverlap);
	CollisionComp->SetGenerateOverlapEvents(true);
	//CollisionComp->OnComponentHit.AddDynamic(this, &ASWeapon::OnWeaponHit);	
	//CollisionComp->SetNotifyRigidBodyCollision(true);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComp->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
	CollisionComp->CanCharacterStepUpOn = ECB_No;
	CollisionComp->bReturnMaterialOnMove;

	BaseDamage = 20.0f;
	BulletSpread = 2.0f;
	RateOfFire = 600;

	SetReplicates(true);

	NetUpdateFrequency = 66.0f;
	MinNetUpdateFrequency = 33.0f;
}


void ASWeapon::BeginPlay()
{
	Super::BeginPlay();

	TimeBetweenShots = 60 / RateOfFire;
}


void ASWeapon::Fire()
{
	if (TypeOfWeapon == WeaponType::Hitscan)
	{
		ASWeapon::OnHitScanFire();
	}
	if (TypeOfWeapon == WeaponType::Projectile)
	{
		ASWeapon::OnProjectileFire();
	}
	if (TypeOfWeapon == WeaponType::Melee)
	{
		ASWeapon::OnMeleeFire();
	}
}

void ASWeapon::OnHitScanFire()
{
	// Trace the world, from pawn eyes to crosshair location

	if (GetLocalRole() < ROLE_Authority)
	{
		ServerFire();
	}

	AActor* MyOwner = GetOwner();
	if (MyOwner)
	{
		FVector EyeLocation;
		FRotator EyeRotation;
		MyOwner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

		FVector ShotDirection = EyeRotation.Vector();

		// Bullet Spread
		float HalfRad = FMath::DegreesToRadians(BulletSpread);
		ShotDirection = FMath::VRandCone(ShotDirection, HalfRad, HalfRad);

		FVector TraceEnd = EyeLocation + (ShotDirection * 10000);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(MyOwner);
		QueryParams.AddIgnoredActor(this);
		QueryParams.bTraceComplex = true;
		QueryParams.bReturnPhysicalMaterial = true;

		// Particle "Target" parameter
		FVector TracerEndPoint = TraceEnd;

		EPhysicalSurface SurfaceType = SurfaceType_Default;

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TraceEnd, COLLISION_WEAPON, QueryParams))
		{
			// Blocking hit! Process damage
			AActor* HitActor = Hit.GetActor();

			SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

			float ActualDamage = BaseDamage;
			if (SurfaceType == SURFACE_FLESHVULNERABLE)
			{
				ActualDamage *= 4.0f;
			}

			UGameplayStatics::ApplyPointDamage(HitActor, ActualDamage, ShotDirection, Hit, MyOwner->GetInstigatorController(), MyOwner, DamageType);

			PlayImpactEffects(SurfaceType, Hit.ImpactPoint);

			TracerEndPoint = Hit.ImpactPoint;

		}

		PlayFireEffects(TracerEndPoint);

		if (GetLocalRole() == ROLE_Authority)
		{
			HitScanTrace.TraceTo = TracerEndPoint;
			HitScanTrace.SurfaceType = SurfaceType;
		}

		LastFireTime = GetWorld()->TimeSeconds;
	}
}

void ASWeapon::OnProjectileFire()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerFire();
	}

	APawn* MyOwner = Cast<APawn>(GetOwner());
	if (MyOwner)
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation("MuzzleSocket");
		FRotator MuzzleRotation = MeshComp->GetSocketRotation("MuzzleSocket");

		FVector EyeLocation;
		FRotator EyeRotation;

		MyOwner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

		//Set Spawn Collision Handling Override
		FActorSpawnParameters ActorSpawnParams;
		ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
		ActorSpawnParams.Owner = MyOwner;
		if (FireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), FireSound, MuzzleLocation);
		}

		if (MuzzleEffect)
		{
			UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComp, MuzzleSocketName);
		}
		// spawn the projectile at the muzzle toward the center of the screen
		GetWorld()->SpawnActor<AProjectile>(ASWeapon::ProjectileClass, MuzzleLocation, EyeRotation, ActorSpawnParams);

		if (GetLocalRole() == ROLE_Authority)
		{
			if (FireSound)
			{
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), FireSound, MuzzleLocation);
			}

			if (MuzzleEffect)
			{
				UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComp, MuzzleSocketName);
			}
		}
	}
}

void ASWeapon::OnMeleeFire()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerFire();
	}
	ServerAnimation();

	APawn* MyOwner = Cast<APawn>(GetOwner());
	if (MyOwner)
	{

	}
}

void ASWeapon::MultiCastAnimation() 
{
	PlayAnimation();
}

void ASWeapon::PlayAnimation()
{
	AActor* MyOwner = GetOwner();
	if (MyOwner)
	{
		USkeletalMeshComponent* skelemesh = MyOwner->FindComponentByClass<USkeletalMeshComponent>();
		UAnimInstance* animInstance = (skelemesh) ? skelemesh->GetAnimInstance() : nullptr;
		if (animInstance)
		{
			GetWorld()->GetTimerManager().ClearTimer(MeleeTimerHandle);
			GetWorld()->GetTimerManager().ClearTimer(ComboResetTimerHandle);
			if (CollisionComp->IsCollisionEnabled())
			{
				ToggleCollisionCompOff();
			}

			//TODO: Find a way to replace FireDelay with a UAnimNotifyState notify
			if (ComboCounter <= 1)
			{
				float duration = animInstance->Montage_Play(ComboMontage1);
				if (duration > 0.f)
				{
					ComboCounter++;
					ToggleCollisionCompOn();
					LastFireTime = GetWorld()->TimeSeconds;
					float FireDelay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);
					GetWorld()->GetTimerManager().SetTimer(MeleeTimerHandle, this, &ASWeapon::ToggleCollisionCompOff, FireDelay, false);
					GetWorld()->GetTimerManager().SetTimer(ComboResetTimerHandle, this, &ASWeapon::ResetComboCounter, duration, false);
				}
			}
			else if (ComboCounter == 2)
			{
				float duration = animInstance->Montage_Play(ComboMontage2);
				if (duration > 0.f)
				{
					ComboCounter++;
					ToggleCollisionCompOn();
					LastFireTime = GetWorld()->TimeSeconds;
					float FireDelay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);
					GetWorld()->GetTimerManager().SetTimer(MeleeTimerHandle, this, &ASWeapon::ToggleCollisionCompOff, FireDelay, false);
					GetWorld()->GetTimerManager().SetTimer(ComboResetTimerHandle, this, &ASWeapon::ResetComboCounter, duration, false);
				}
			}
			else if (ComboCounter >= 3)
			{
				float duration = animInstance->Montage_Play(ComboMontage3);
				ComboCounter = 1;
				ToggleCollisionCompOn();
				LastFireTime = GetWorld()->TimeSeconds;
				float FireDelay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);
				GetWorld()->GetTimerManager().SetTimer(MeleeTimerHandle, this, &ASWeapon::ToggleCollisionCompOff, FireDelay, false);
				GetWorld()->GetTimerManager().SetTimer(ComboResetTimerHandle, this, &ASWeapon::ResetComboCounter, duration, false);
			}
		}
	}
}


void ASWeapon::OnRep_HitScanTrace()
{
	// Play cosmetic FX
	PlayFireEffects(HitScanTrace.TraceTo);
	PlayImpactEffects(HitScanTrace.SurfaceType, HitScanTrace.TraceTo);
}


void ASWeapon::ServerFire_Implementation()
{
	Fire();
}


bool ASWeapon::ServerFire_Validate()
{
	return true;
}

void ASWeapon::ServerAnimation_Implementation()
{
	PlayAnimation();
}


bool ASWeapon::ServerAnimation_Validate()
{
	return true;
}



void ASWeapon::StartFire()
{
	float FirstDelay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);

	GetWorldTimerManager().SetTimer(TimerHandle_TimeBetweenShots, this, &ASWeapon::Fire, TimeBetweenShots, true, FirstDelay);
}


void ASWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_TimeBetweenShots);
}


void ASWeapon::PlayFireEffects(FVector TraceEnd)
{
	if (MuzzleEffect)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComp, MuzzleSocketName);
	}

	if (TracerEffect)
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);

		UParticleSystemComponent* TracerComp = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TracerEffect, MuzzleLocation);
		if (TracerComp)
		{
			TracerComp->SetVectorParameter(TracerTargetName, TraceEnd);
		}
	}

	APawn* MyOwner = Cast<APawn>(GetOwner());
	if (MyOwner)
	{
		APlayerController* PC = Cast<APlayerController>(MyOwner->GetController());
		if (PC)
		{
			PC->ClientPlayCameraShake(FireCamShake);
		}
	}
}

FName ASWeapon::ReturnWeaponSocketName(ASWeapon* weapon)
{
	return weapon->WeaponAttachSocketName;
}

WeaponType ASWeapon::ReturnWeaponType(ASWeapon* weapon)
{
	return weapon->TypeOfWeapon;
}

void ASWeapon::PlayImpactEffects(EPhysicalSurface SurfaceType, FVector ImpactPoint)
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
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);

		FVector ShotDirection = ImpactPoint - MuzzleLocation;
		ShotDirection.Normalize();

		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelectedEffect, ImpactPoint, ShotDirection.Rotation());
	}
}

USkeletalMeshComponent* ASWeapon::GetWepMesh()
{
	return this->MeshComp;
}

float ASWeapon::GetBaseDamage()
{
	return this->BaseDamage;
}

TSubclassOf<UDamageType> ASWeapon::GetDamageType()
{
	return this->DamageType;
}

UParticleSystem* ASWeapon::GetDefaultImpactEffect()
{
	return this->DefaultImpactEffect;
}

UParticleSystem* ASWeapon::GetFleshImpactEffect()
{
	return this->FleshImpactEffect;
}

USoundBase* ASWeapon::GetFireSound()
{
	return this->FireSound;
}

USoundBase* ASWeapon::GetImpactSound()
{
	return this->ImpactSound;
}

void ASWeapon::ToggleCollisionCompOn()
{
	if (this->CollisionComp)
	{
		if (!this->CollisionComp->IsCollisionEnabled())
		{
			this->CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}	
}

void ASWeapon::ToggleCollisionCompOff()
{
	if (this->CollisionComp)
	{
		if(this->CollisionComp->IsCollisionEnabled())
		{
			this->CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			RecentlyHit.Empty();
		}
	}
}

void ASWeapon::ResetComboCounter() 
{
	ComboCounter = 1;
}

void ASWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ASWeapon, HitScanTrace, COND_SkipOwner);
}

void ASWeapon::OnWeaponOverlap(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AActor* MeleeOwner = this->GetOwner();
	ASCharacter* SCharacter = Cast<ASCharacter>(MeleeOwner);
	if (OtherActor != NULL && OtherActor != this && OtherActor != MeleeOwner && OtherComp != NULL)
	{
		AActor* HitActor = OtherActor;
		ASCharacter* hitChar = Cast<ASCharacter>(HitActor);
		if (hitChar && SCharacter->TeamNum != hitChar->TeamNum && !RecentlyHit.Contains(hitChar))
		{
			EPhysicalSurface SurfaceType = SurfaceType_Default;
			SurfaceType = UPhysicalMaterial::DetermineSurfaceType(SweepResult.PhysMaterial.Get());
			UGameplayStatics::ApplyDamage(HitActor, BaseDamage, MeleeOwner->GetInstigatorController(), MeleeOwner, DamageType);
			//TODO: Figure out how to get the impact point so that the effect plays
			PlayImpactEffects(SurfaceType, SweepResult.ImpactPoint);
			if (ImpactSound)
			{
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, SweepResult.ImpactNormal);
			}
			RecentlyHit.Add(hitChar);
		}
	}
}

void ASWeapon::OnWeaponHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	AActor* MeleeOwner = this->GetOwner();
	ASCharacter* SCharacter = Cast<ASCharacter>(MeleeOwner);
	if (OtherActor != NULL && OtherActor != this && OtherActor != MeleeOwner && OtherComp != NULL)
	{
		AActor* HitActor = OtherActor;
		ASCharacter* hitChar = Cast<ASCharacter>(HitActor);
		if (hitChar && SCharacter->TeamNum != hitChar->TeamNum && !RecentlyHit.Contains(hitChar))
		{
			EPhysicalSurface SurfaceType = SurfaceType_Default;
			SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
			UGameplayStatics::ApplyDamage(HitActor, BaseDamage, MeleeOwner->GetInstigatorController(), MeleeOwner, DamageType);
			//TODO: Figure out how to get the impact point so that the effect plays
			PlayImpactEffects(SurfaceType, Hit.ImpactPoint);
			RecentlyHit.Add(hitChar);
		}
	}
}