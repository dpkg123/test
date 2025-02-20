/*=============================================================================
	ParticleModules_Collision.cpp: 
	Collision-related particle module implementations.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"

/*-----------------------------------------------------------------------------
	FParticlesStatGroup
-----------------------------------------------------------------------------*/
DECLARE_CYCLE_STAT(TEXT("Particle Collision Time"),STAT_ParticleCollisionTime,STATGROUP_Particles);

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleCollisionBase);

/*-----------------------------------------------------------------------------
	UParticleModuleCollision implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleCollision);

UINT UParticleModuleCollision::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FParticleCollisionPayload);
}

void UParticleModuleCollision::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleCollisionTime);
	SPAWN_INIT;
	{
		PARTICLE_ELEMENT(FParticleCollisionPayload, CollisionPayload);
		CollisionPayload.UsedDampingFactor = DampingFactor.GetValue(Owner->EmitterTime, Owner->Component);
		CollisionPayload.UsedDampingFactorRotation = DampingFactorRotation.GetValue(Owner->EmitterTime, Owner->Component);
		CollisionPayload.UsedCollisions = appRound(MaxCollisions.GetValue(Owner->EmitterTime, Owner->Component));
		CollisionPayload.Delay = DelayAmount.GetValue(Owner->EmitterTime, Owner->Component);
		if (CollisionPayload.Delay > SpawnTime)
		{
			Particle.Flags |= STATE_Particle_DelayCollisions;
			Particle.Flags &= ~STATE_Particle_CollisionHasOccurred;
		}
	}
}

void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleCollisionTime);

	if (bDropDetail && GWorld && GWorld->GetWorldInfo() && GWorld->GetWorldInfo()->bDropDetail)
	{
		return;
	}

	AActor* Actor = Owner->Component->GetOwner();
	if (!Actor && GIsGame)
	{
		return;
	}

	UBOOL bMeshEmitter = Owner->Type()->IsA(FParticleMeshEmitterInstance::StaticType);
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);

	FParticleMeshEmitterInstance* pkMeshInst = bMeshEmitter ? CastEmitterInstance<FParticleMeshEmitterInstance>(Owner) : NULL;

	FVector ParentScale = FVector(1.0f, 1.0f, 1.0f);
	if (Owner->Component)
	{
		ParentScale *= Owner->Component->Scale * Owner->Component->Scale3D;
		if (Actor && !Owner->Component->AbsoluteScale)
		{
			ParentScale *= Actor->DrawScale * Actor->DrawScale3D;
		}
	}

	FParticleEventInstancePayload* EventPayload = NULL;
	if (LODLevel->EventGenerator)
	{
		EventPayload = (FParticleEventInstancePayload*)(Owner->GetModuleInstanceData(LODLevel->EventGenerator));
		if (EventPayload && 
			(EventPayload->bCollisionEventsPresent == FALSE) && 
			(EventPayload->bDeathEventsPresent == FALSE))
		{
			EventPayload = NULL;
		}
	}

	BEGIN_UPDATE_LOOP;
	{
		if (((Particle.Flags & STATE_Particle_Freeze) != 0) ||
			((Particle.Flags & STATE_Particle_IgnoreCollisions) != 0) ||
			((Particle.Flags & STATE_Particle_FreezeTranslation) != 0) ||
			((Particle.Flags & STATE_Particle_FreezeRotation) != 0))
		{
			CONTINUE_UPDATE_LOOP;
		}

		PARTICLE_ELEMENT(FParticleCollisionPayload, CollisionPayload);
		if ((Particle.Flags & STATE_Particle_DelayCollisions) != 0)
		{
			if (CollisionPayload.Delay > Particle.RelativeTime)
			{
				CONTINUE_UPDATE_LOOP;
			}
			Particle.Flags &= ~STATE_Particle_DelayCollisions;
		}

		FVector			Location;
		FVector			OldLocation;

		// Location won't be calculated till after tick so we need to calculate an intermediate one here.
		Location	= Particle.Location + Particle.Velocity * DeltaTime;
		if (LODLevel->RequiredModule->bUseLocalSpace)
		{
			// Transform the location and old location into world space
			Location		= Owner->Component->LocalToWorld.TransformFVector(Location);
			OldLocation		= Owner->Component->LocalToWorld.TransformFVector(Particle.OldLocation);
		}
		else
		{
			OldLocation	= Particle.OldLocation;
		}
		FVector	Direction = (Location - OldLocation).SafeNormal();

        // Determine the size
		FVector Size = Particle.Size * ParentScale;

		FVector	Extent(0.0f);
		FCheckResult Hit;

		Hit.Normal.X = 0.0f;
		Hit.Normal.Y = 0.0f;
		Hit.Normal.Z = 0.0f;

		check( Owner->Component );

		if (PerformCollisionCheck(Owner, &Particle, Hit, Actor, Location + Direction * Size / DirScalar, OldLocation, TRACE_ProjTargets | TRACE_AllBlocking, Extent))
		{
			UBOOL bDecrementMaxCount = TRUE;
			UBOOL bIgnoreCollision = FALSE;
			if (Hit.Actor)
			{
				if (bPawnsDoNotDecrementCount == TRUE)
				{
					if (Hit.Actor->IsA(APawn::StaticClass()))
					{
						//debugf(TEXT("Particle from %s Collided with a PAWN!"), *(Owner->Component->Template->GetPathName()));
						bDecrementMaxCount = FALSE;
					}
				}

				//@todo.SAS. Allow for PSys to say what it wants to collide w/?
				if (Hit.Actor->IsA(ATrigger::StaticClass()))
				{
					bIgnoreCollision = TRUE;
				}
			}

			if (bIgnoreCollision == FALSE)
			{
				if (bDecrementMaxCount && (bOnlyVerticalNormalsDecrementCount == TRUE))
				{
					if ((Hit.Normal.IsNearlyZero() == FALSE) && (Abs(Hit.Normal.Z) + VerticalFudgeFactor) < 1.0f)
					{
						//debugf(TEXT("Particle from %s had a non-vertical hit!"), *(Owner->Component->Template->GetPathName()));
						bDecrementMaxCount = FALSE;
					}
				}

				if (bDecrementMaxCount)
				{
					CollisionPayload.UsedCollisions--;
				}

				if (CollisionPayload.UsedCollisions > 0)
				{
					if (LODLevel->RequiredModule->bUseLocalSpace)
					{
						// Transform the particle velocity to world space
						FVector OldVelocity		= Owner->Component->LocalToWorld.TransformNormal(Particle.Velocity);
						FVector	BaseVelocity	= Owner->Component->LocalToWorld.TransformNormal(Particle.BaseVelocity);
						BaseVelocity			= BaseVelocity.MirrorByVector(Hit.Normal) * CollisionPayload.UsedDampingFactor;

						Particle.BaseVelocity		= Owner->Component->LocalToWorld.Inverse().TransformNormal(BaseVelocity);
						Particle.BaseRotationRate	= Particle.BaseRotationRate * CollisionPayload.UsedDampingFactorRotation.X;
						if (pkMeshInst && pkMeshInst->MeshRotationActive)
						{
							FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + pkMeshInst->MeshRotationOffset);
							PayloadData->RotationRateBase *= CollisionPayload.UsedDampingFactorRotation;
						}

						// Reset the current velocity and manually adjust location to bounce off based on normal and time of collision.
						FVector NewVelocity	= Direction.MirrorByVector(Hit.Normal) * (Location - OldLocation).Size() * CollisionPayload.UsedDampingFactor;
						Particle.Velocity		= FVector(0.f);

						// New location
						FVector	NewLocation		= Location + NewVelocity * (1.f - Hit.Time);
						Particle.Location		= Owner->Component->LocalToWorld.Inverse().TransformFVector(NewLocation);

						if (bApplyPhysics && Hit.Component)
						{
							FVector vImpulse;
							vImpulse = -(NewVelocity - OldVelocity) * ParticleMass.GetValue(Particle.RelativeTime, Owner->Component);
							Hit.Component->AddImpulse(vImpulse, Hit.Location, Hit.BoneName);
						}
					}
					else
					{
						FVector vOldVelocity = Particle.Velocity;

						// Reflect base velocity and apply damping factor.
						Particle.BaseVelocity		= Particle.BaseVelocity.MirrorByVector(Hit.Normal) * CollisionPayload.UsedDampingFactor;
						Particle.BaseRotationRate	= Particle.BaseRotationRate * CollisionPayload.UsedDampingFactorRotation.X;
						if (pkMeshInst && pkMeshInst->MeshRotationActive)
						{
							FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + pkMeshInst->MeshRotationOffset);
							PayloadData->RotationRateBase *= CollisionPayload.UsedDampingFactorRotation;
						}

						// Reset the current velocity and manually adjust location to bounce off based on normal and time of collision.
						FVector vNewVelocity	= Direction.MirrorByVector(Hit.Normal) * (Location - OldLocation).Size() * CollisionPayload.UsedDampingFactor;
						Particle.Velocity		= FVector(0.f);
						Particle.Location	   += vNewVelocity * (1.f - Hit.Time);

						if (bApplyPhysics && Hit.Component)
						{
							FVector vImpulse;
							vImpulse = -(vNewVelocity - vOldVelocity) * ParticleMass.GetValue(Particle.RelativeTime, Owner->Component);
							Hit.Component->AddImpulse(vImpulse, Hit.Location, Hit.BoneName);
						}
					}

					if (EventPayload && (EventPayload->bCollisionEventsPresent == TRUE))
					{
						LODLevel->EventGenerator->HandleParticleCollision(Owner, EventPayload, &CollisionPayload, &Hit, &Particle, Direction);
					}
				}
				else
				{
					if (LODLevel->RequiredModule->bUseLocalSpace == TRUE)
					{
						Size = Owner->Component->LocalToWorld.TransformNormal(Size);
					}
					Particle.Location = Hit.Location + (Size / 2.0f);
					if (LODLevel->RequiredModule->bUseLocalSpace == TRUE)
					{
						// We need to transform the location back relative to the PSys.
						// NOTE: LocalSpace makes sense only for stationary emitters that use collision.
						Particle.Location = Owner->Component->LocalToWorld.Inverse().TransformFVector(Particle.Location);
					}
					switch (CollisionCompletionOption)
					{
					case EPCC_Kill:
						{
							if (EventPayload && (EventPayload->bDeathEventsPresent == TRUE))
							{
								LODLevel->EventGenerator->HandleParticleKilled(Owner, EventPayload, &Particle);
							}
							KILL_CURRENT_PARTICLE;
						}
						break;
					case EPCC_Freeze:
						{
							Particle.Flags |= STATE_Particle_Freeze;
						}
						break;
					case EPCC_HaltCollisions:
						{
							Particle.Flags |= STATE_Particle_IgnoreCollisions;
						}
						break;
					case EPCC_FreezeTranslation:
						{
							Particle.Flags |= STATE_Particle_FreezeTranslation;
						}
						break;
					case EPCC_FreezeRotation:
						{
							Particle.Flags |= STATE_Particle_FreezeRotation;
						}
						break;
					case EPCC_FreezeMovement:
						{
							Particle.Flags |= STATE_Particle_FreezeRotation;
							Particle.Flags |= STATE_Particle_FreezeTranslation;
						}
						break;
					}

					if (EventPayload && (EventPayload->bCollisionEventsPresent == TRUE))
					{
						LODLevel->EventGenerator->HandleParticleCollision(Owner, EventPayload, &CollisionPayload, &Hit, &Particle, Direction);
					}
				}
				Particle.Flags |= STATE_Particle_CollisionHasOccurred;
			}
		}
	}
	END_UPDATE_LOOP;
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleCollision::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UDistributionFloatUniform* MaxCollDist = Cast<UDistributionFloatUniform>(MaxCollisions.Distribution);
	if (MaxCollDist)
	{
		MaxCollDist->Min = 1.0f;
		MaxCollDist->Max = 1.0f;
		MaxCollDist->bIsDirty = TRUE;
	}
}

UBOOL UParticleModuleCollision::GenerateLODModuleValues(UParticleModule* SourceModule, FLOAT Percentage, UParticleLODLevel* LODLevel)
{
	// disable collision on emitters at the lowest LOD level
	//@todo.SAS. Determine how to forcibly disable collision now...
	return TRUE;
}

/**
 *	Perform the desired collision check for this module.
 *	
 *	@param	Owner			The emitter instance that owns the particle being checked
 *	@param	InParticle		The particle being checked for a collision
 *	@param	Hit				The hit results to fill in for a collision
 *	@param	SourceActor		The source actor for the check
 *	@param	End				The end position for the check
 *	@param	Start			The start position for the check
 *	@param	TraceFlags		The trace flags to use for the check
 *	@param	Extent			The extent to use for the check
 *	
 *	@return UBOOL			TRUE if a collision occurred.
 */
UBOOL UParticleModuleCollision::PerformCollisionCheck(FParticleEmitterInstance* Owner, FBaseParticle* InParticle, 
	FCheckResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start, DWORD TraceFlags, const FVector& Extent)
{
	check(Owner && Owner->Component);
	return !(Owner->Component->SingleLineCheck(Hit, SourceActor, End, Start, TraceFlags, Extent));
}

/*-----------------------------------------------------------------------------
	UParticleModuleCollisionActor implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleCollisionActor);

void UParticleModuleCollisionActor::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	if (ActorsToCollideWith.Num() > 0)
	{
		// Update the list of actors... 
		FParticleCollisionActorInstancePayload* CollisionActorPayload = (FParticleCollisionActorInstancePayload*)(Owner->GetModuleInstanceData(this));
		if (CollisionActorPayload != NULL)
		{
			for (INT NameIdx = 0; NameIdx < ActorsToCollideWith.Num(); NameIdx++)
			{
				AActor* FoundActor = NULL;
				if (Owner->Component->GetActorParameter(ActorsToCollideWith(NameIdx), FoundActor))
				{
					CollisionActorPayload->ActorList.AddUniqueItem(FoundActor);
				}
			}

			if (bCheckPawnCollisions == TRUE)
			{
				INT CurrentCount = 0;
				AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
				if (WorldInfo != NULL)
				{
					FBox ComponentBox = Owner->Component->Bounds.GetBox();
					for (APawn* CheckPawn = WorldInfo->PawnList; CheckPawn != NULL; CheckPawn = CheckPawn->NextPawn)
					{
						FBox CheckBox = CheckPawn->GetComponentsBoundingBox();
						if (CheckBox.Intersect(ComponentBox))
						{
							if (CurrentCount < CollisionActorPayload->PawnList.Num())
							{
								FParticlePawnCollisionInfo& PawnInfo = CollisionActorPayload->PawnList(CurrentCount);
								PawnInfo.Pawn = CheckPawn;
								PawnInfo.PawnBox = CheckBox;
							}
							else
							{
								new(CollisionActorPayload->PawnList)FParticlePawnCollisionInfo(CheckBox, CheckPawn);
							}
							CurrentCount++;
						}
					}

					for (INT ClearIdx = CurrentCount; ClearIdx < CollisionActorPayload->PawnList.Num(); ClearIdx++)
					{
						CollisionActorPayload->PawnList(CurrentCount).Pawn = NULL;
					}
				}
			}
		}

		Super::Update(Owner, Offset, DeltaTime);
	}
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleCollisionActor::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	INT Size = Super::RequiredBytesPerInstance(Owner);
	if ((ActorsToCollideWith.Num() > 0) || (bCheckPawnCollisions == TRUE))
	{
		Size += sizeof(FParticleCollisionActorInstancePayload);
	}
	return Size;
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleCollisionActor::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return Super::PrepPerInstanceBlock(Owner, InstData);
}

/**
 *	Perform the desired collision check for this module.
 *	
 *	@param	Owner			The emitter instance that owns the particle being checked
 *	@param	InParticle		The particle being checked for a collision
 *	@param	Hit				The hit results to fill in for a collision
 *	@param	SourceActor		The source actor for the check
 *	@param	End				The end position for the check
 *	@param	Start			The start position for the check
 *	@param	TraceFlags		The trace flags to use for the check
 *	@param	Extent			The extent to use for the check
 *	
 *	@return UBOOL			TRUE if a collision occurred.
 */
UBOOL UParticleModuleCollisionActor::PerformCollisionCheck(FParticleEmitterInstance* Owner, FBaseParticle* InParticle, 
	FCheckResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start, DWORD TraceFlags, const FVector& Extent)
{
	FParticleCollisionActorInstancePayload* CollisionActorPayload = (FParticleCollisionActorInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (CollisionActorPayload != NULL)
	{
		if (bCheckPawnCollisions == TRUE)
		{
			// Give pawns priority over actors...
			for (INT PawnIdx = 0; PawnIdx < CollisionActorPayload->PawnList.Num(); PawnIdx++)
			{
				FParticlePawnCollisionInfo& PawnInfo = CollisionActorPayload->PawnList(PawnIdx);
				if ((PawnInfo.Pawn != NULL) && 
					(PawnInfo.PawnBox.IsInside(InParticle->Location) || 
					PawnInfo.PawnBox.IsInside(InParticle->Location - InParticle->Size) || 
					PawnInfo.PawnBox.IsInside(InParticle->Location + InParticle->Size))
					)
				{
					if (PawnInfo.Pawn->ActorLineCheck(Hit, End, Start, Extent, TraceFlags) == FALSE)
					{
						return TRUE;
					}
				}
			}
		}

		for (INT ActorIdx = 0; ActorIdx < CollisionActorPayload->ActorList.Num(); ActorIdx++)
		{
			AActor* FoundActor = CollisionActorPayload->ActorList(ActorIdx);
			if (FoundActor != NULL)
			{
				if (FoundActor->ActorLineCheck(Hit, End, Start, Extent, TraceFlags) == FALSE)
				{
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/**
 *	Helper function used by the editor to auto-populate a placed AEmitter with any
 *	instance parameters that are utilized.
 *
 *	@param	PSysComp		The particle system component to be populated.
 */
void UParticleModuleCollisionActor::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
#if WITH_EDITOR
	for (INT NameIdx = 0; NameIdx < ActorsToCollideWith.Num(); NameIdx++)
	{
		FName ActorName = ActorsToCollideWith(NameIdx);
		UBOOL bFound = FALSE;
		for (INT ParamIdx = 0; ParamIdx < PSysComp->InstanceParameters.Num(); ParamIdx++)
		{
			FParticleSysParam* Param = &(PSysComp->InstanceParameters(ParamIdx));
			if (Param->Name == ActorName)
			{
				if (Param->ParamType == PSPT_Actor)
				{
					bFound = TRUE;
					break;
				}
				else
				{
					//@todo. Warn the user about this...
				}
			}
		}

		if (bFound == FALSE)
		{
			INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
			PSysComp->InstanceParameters(NewParamIndex).Name = ActorName;
			PSysComp->InstanceParameters(NewParamIndex).ParamType = PSPT_Actor;
			PSysComp->InstanceParameters(NewParamIndex).Actor = NULL;
		}
	}
#endif
}
