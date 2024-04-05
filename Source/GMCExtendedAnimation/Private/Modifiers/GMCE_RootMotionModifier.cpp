#include "GMCE_RootMotionModifier.h"

#include "GMCExtendedAnimation.h"
#include "GMCE_MotionWarpingComponent.h"

UGMCE_RootMotionModifier::UGMCE_RootMotionModifier(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
}

void UGMCE_RootMotionModifier::OnStateChanged(EGMCE_RootMotionModifierState PreviousState)
{
	if (UGMCE_MotionWarpingComponent* Component = GetOwnerComponent())
	{
		if (PreviousState != EGMCE_RootMotionModifierState::Active && State == EGMCE_RootMotionModifierState::Active)
		{
			// We weren't active are are now.
			OnActivateDelegate.ExecuteIfBound(Component, this);
		}
		else if (PreviousState == EGMCE_RootMotionModifierState::Active && State != EGMCE_RootMotionModifierState::Active)
		{
			// We were active are, and now aren't.
			OnDeactivateDelegate.ExecuteIfBound(Component, this);
		}
	}
}

void UGMCE_RootMotionModifier::SetState(EGMCE_RootMotionModifierState NewState)
{
	if (NewState != State)
	{
		EGMCE_RootMotionModifierState OldState = State;
		State = NewState;

		OnStateChanged(OldState);
	}
}

UGMCE_MotionWarpingComponent* UGMCE_RootMotionModifier::GetOwnerComponent() const
{
	return Cast<UGMCE_MotionWarpingComponent>(GetOuter());
}

AGMC_Pawn* UGMCE_RootMotionModifier::GetPawnOwner() const
{
	if (const UGMCE_MotionWarpingComponent* Component = GetOwnerComponent())
	{
		return Component->GetOwningPawn();
	}

	return nullptr;
}

void UGMCE_RootMotionModifier::Update(const FGMCE_MotionWarpContext& Context)
{
	// We do need an actual pawn to animate, or there's no point in running anything.
	const AGMC_Pawn *PawnOwner = GetPawnOwner();
	if (PawnOwner == nullptr)
	{
		return;
	}

	// We ALSO need a valid animation, or there's nothing to warp.
	if (!Context.Animation.IsValid() || Context.Animation.Get() != AnimationSequence)
	{
		UE_LOG(LogGMCExAnimation, Verbose, TEXT("Motion Warping: marking modifier for removal as animation is no longer valid. %s"),
			*ToString())
		SetState(EGMCE_RootMotionModifierState::MarkedForRemoval);
		return;
	}

	PreviousPosition = Context.PreviousPosition;
	CurrentPosition = Context.CurrentPosition;
	Weight = Context.Weight;

	// Make sure we're within a valid window for this modifier to be utilized; if we've passed beyond the window,
	// mark for removal. We ONLY check if we're past the end point, because we want to pre-create the modifiers and
	// leave them in Waiting state until they hit their window.
	if (PreviousPosition >= EndTime)
	{
		// We've concluded.
		UE_LOG(LogGMCExAnimation, Verbose, TEXT("Motion Warping: marking modifier for removal as we've passed the end time. %s"),
			*ToString())

		SetState(EGMCE_RootMotionModifierState::MarkedForRemoval);
		return;
	}

	// Check if we were in the window but have moved outside of it.
	if (State == EGMCE_RootMotionModifierState::Active && PreviousPosition < EndTime && (CurrentPosition > EndTime || CurrentPosition < StartTime))
	{
		const float ExpectedDelta = Context.DeltaSeconds * Context.PlayRate;
		const float ActualDelta = CurrentPosition - PreviousPosition;

		// Unreal is non deterministic and some tiny degree of deviation from our expected delta is likely at various times.
		// If the deviation isn't tiny, however, we've been repositioned outside of the playback window by someone manually
		// setting a time on the playback. If that happens while we were in a modifier window, the modifier is by definition
		// no longer usable as the state is going to be what I believe in technical terms is referred to as "Wonkers."
		//
		// This only applies to modifiers whose window we were IN when the animation was repositioned; modifiers yet to come
		// in the animation can still be used.
		if (!FMath::IsNearlyZero(FMath::Abs(ActualDelta - ExpectedDelta), KINDA_SMALL_NUMBER))
		{
			UE_LOG(LogGMCExAnimation, Verbose, TEXT("Motion Warping: marking modifier for removal as playback has been shifted outside the window. %s: %s expected delta %f received %d"),
				*GetNameSafe(GetPawnOwner()), *GetNameSafe(AnimationSequence.Get()), ExpectedDelta, ActualDelta);

			SetState(EGMCE_RootMotionModifierState::MarkedForRemoval);
			return;
		}
	}

	// Check if we're in the window.
	if (PreviousPosition >= StartTime && PreviousPosition <= EndTime)
	{
		if (State == EGMCE_RootMotionModifierState::Waiting)
		{
			// We weren't relevant, but we are now.
			SetState(EGMCE_RootMotionModifierState::Active);
		}		
	}

	if (State == EGMCE_RootMotionModifierState::Active)
	{
		if (UGMCE_MotionWarpingComponent* Component = GetOwnerComponent())
		{
			OnUpdateDelegate.ExecuteIfBound(Component, this);
		}
	}

}

FString UGMCE_RootMotionModifier::ToString() const
{
	return FString::Printf(TEXT("%s: %s in %s Time [%f %f] Pos [%f %f]"),
		*GetNameSafe(GetPawnOwner()), *GetNameSafe(StaticClass()), *GetNameSafe(AnimationSequence.Get()),
		StartTime, EndTime, PreviousPosition, CurrentPosition);
}
