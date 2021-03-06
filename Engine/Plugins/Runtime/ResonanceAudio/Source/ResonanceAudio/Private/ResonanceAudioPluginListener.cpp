//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#include "ResonanceAudioPluginListener.h"
#include "ResonanceAudioAmbisonics.h"
#include "AudioDevice.h"
#include "Async/Async.h"
#include "Components/BrushComponent.h"
#include "Model.h"

namespace ResonanceAudio
{
	FResonanceAudioPluginListener::FResonanceAudioPluginListener()
		: ResonanceAudioApi(nullptr)
		, ResonanceAudioModule(nullptr)
		, ReverbPtr(nullptr)
		, SpatializationPtr(nullptr)
		, AmbisonicsPtr(nullptr)
	{
	}

	FResonanceAudioPluginListener::~FResonanceAudioPluginListener()
	{
		if (ResonanceAudioApi != nullptr)
		{
			delete ResonanceAudioApi;
			ResonanceAudioApi = nullptr;
		}
	}

	void FResonanceAudioPluginListener::OnListenerInitialize(FAudioDevice* AudioDevice, UWorld* ListenerWorld)
	{
		if (!ResonanceAudioModule)
		{
			ResonanceAudioModule = &FModuleManager::GetModuleChecked<FResonanceAudioModule>("ResonanceAudio");
		}

		// Initialize Resonance Audio API.
		const size_t FramesPerBuffer = static_cast<size_t>(AudioDevice->GetBufferLength());
		const int SampleRate = static_cast<int>(AudioDevice->GetSampleRate());

		ResonanceAudioApi = CreateResonanceAudioApi(ResonanceAudioModule->GetResonanceAudioDynamicLibraryHandle(), 2 /* num channels */, FramesPerBuffer, SampleRate);
		if (ResonanceAudioApi == nullptr)
		{
			UE_LOG(LogResonanceAudio, Error, TEXT("Failed to initialize Resonance Audio API"));
			return;
		}

		ReverbPtr = (FResonanceAudioReverb*)AudioDevice->ReverbPluginInterface.Get();
		SpatializationPtr = (FResonanceAudioSpatialization*)AudioDevice->SpatializationPluginInterface.Get();
		AmbisonicsPtr = (FResonanceAudioAmbisonicsMixer*)AudioDevice->GetAmbisonicsMixer().Get();

		// Make sure that Reverb *AND* spatialization plugins are enabled.
		if (ReverbPtr == nullptr || SpatializationPtr == nullptr)
		{
			UE_LOG(LogResonanceAudio, Error, TEXT("Resonance Audio requires both Reverb and Spatialization plugins. Please enable them in the Project Settings."));
			return;
		}

		ReverbPtr->SetResonanceAudioApi(ResonanceAudioApi);
		SpatializationPtr->SetResonanceAudioApi(ResonanceAudioApi);

		if (AmbisonicsPtr == nullptr)
		{
			UE_LOG(LogResonanceAudio, Warning, TEXT("Resonance Audio Ambisonics plugin not found. Ambisonics files will not play correctly."));
		}
		else
		{
			AmbisonicsPtr->SetResonanceAudioApi(ResonanceAudioApi);
		}

		UE_LOG(LogResonanceAudio, Log, TEXT("Resonance Audio Listener is initialized"));
	}

	void FResonanceAudioPluginListener::OnListenerUpdated(FAudioDevice* AudioDevice, const int32 ViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds)
	{
		if (ResonanceAudioApi == nullptr) {
			UE_LOG(LogResonanceAudio, Error, TEXT("Resonance Audio API not loaded"));
			return;
		}
		else
		{
			const FVector ConvertedPosition = ConvertToResonanceAudioCoordinates(ListenerTransform.GetLocation());
			ResonanceAudioApi->SetHeadPosition(ConvertedPosition.X, ConvertedPosition.Y, ConvertedPosition.Z);
			const FQuat ConvertedRotation = ConvertToResonanceAudioRotation(ListenerTransform.GetRotation());
			ResonanceAudioApi->SetHeadRotation(ConvertedRotation.X, ConvertedRotation.Y, ConvertedRotation.Z, ConvertedRotation.W);


			static IConsoleVariable* ExtraBinauralLoggingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.ExtraResonanceLogging"));
			if (ExtraBinauralLoggingCVar && ExtraBinauralLoggingCVar->GetBool())
			{
				UE_LOG(LogResonanceAudio, Warning, TEXT("Set listener position (X,Y,Z) to: (%f, %f, %f)"), ConvertedPosition.X, ConvertedPosition.Y, ConvertedPosition.Z);
			}
		}
	}

	void FResonanceAudioPluginListener::OnListenerShutdown(FAudioDevice* AudioDevice)
	{
		if (ResonanceAudioModule)
		{
			ResonanceAudioModule->UnregisterAudioDevice(AudioDevice);
		}

		UE_LOG(LogResonanceAudio, Log, TEXT("Resonance Audio Listener is shutdown"));
	}

	void FResonanceAudioPluginListener::OnTick(UWorld* InWorld, const int32 ViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds)
	{
		if (ReverbPtr != nullptr && InWorld->AudioVolumes.Num() > 0)
		{
			AAudioVolume* CurrentVolume = InWorld->GetAudioSettings(ListenerTransform.GetLocation(), nullptr, nullptr);
			if (CurrentVolume != nullptr)
			{
				UResonanceAudioReverbPluginPreset* Preset = static_cast<UResonanceAudioReverbPluginPreset*>(CurrentVolume->GetReverbSettings().ReverbPluginEffect);
				if (Preset != nullptr && Preset->UseAudioVolumeTransform())
				{
					// Obtain Resonance Audio room transform from the Unreal Audio Volume transform.
					const FVector CurrentVolumePosition = CurrentVolume->GetActorLocation();
					Preset->SetRoomPosition(CurrentVolumePosition);
					const FQuat CurrentVolumeRotation = CurrentVolume->GetActorQuat();
					Preset->SetRoomRotation(CurrentVolumeRotation);
					const FVector CurrentVolumeDimensions = CurrentVolume->GetActorScale3D();
					const FVector CurrentBrushShapeExtents = 2.0f * CurrentVolume->GetBrushComponent()->Brush->Bounds.BoxExtent;
					const FVector RoomDimensions = CurrentVolumeDimensions * CurrentBrushShapeExtents;
					// The default Audio Volume cube size is 200cm, please see UCubeBuilder constructor for initialization details.
					Preset->SetRoomDimensions(RoomDimensions);
				}
				// Activate this preset or no room effects if nullptr.
				ReverbPtr->SetPreset(Preset);
			}
			else
			{
				ReverbPtr->SetPreset(nullptr);
				UE_LOG(LogResonanceAudio, Log, TEXT("Set reverb preset to nullptr"));
			}
		}
	}

}  // namespace ResonanceAudio
