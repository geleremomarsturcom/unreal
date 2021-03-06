// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/** Interface for Non Real-Time (NRT) Audio Analyzer UObjects. */

#pragma once

#include "CoreMinimal.h"
#include "IAudioAnalyzerNRTInterface.h"
#include "AudioAnalyzerAsset.h"
#include "Sound/SoundWave.h"
#include "AudioAnalyzerNRT.generated.h"

DECLARE_DELEGATE( FAnalyzeAudioDelegate );

/** UAudioAnalyzerNRTSettings
 *
 * UAudioAnalyzerNRTSettings provides a way to store and reuse existing analyzer settings
 * across multipler analyzers. This class provides the interface and functionality to 
 * automatically trigger reanalysis of audio across all analyzers associated with this 
 * settingwhen when a UPROPERTY in this setting object is edited.
 *
 */
UCLASS(Abstract, EditInlineNew, BlueprintType)
class AUDIOANALYZER_API UAudioAnalyzerNRTSettings : public UAudioAnalyzerAsset
{
	GENERATED_BODY()

	public:

#if WITH_EDITOR

		FAnalyzeAudioDelegate AnalyzeAudioDelegate;

		/** 
		 * Called when a UPROPERTY of this class is edited. Triggers
		 * reanalysis of audio.
		 *
		 * This determines whether to trigger analysis by calling ShouldEventTriggerAnalysis(...)
		 */
		void PostEditChangeProperty (struct FPropertyChangedEvent & PropertyChangedEvent) override;

		/** 
		 * This returns true when the PropertyChangeEvent is due to update a setting property. 
		 * Override this method in order to customize this behavior.
		 */
		virtual bool ShouldEventTriggerAnalysis(struct FPropertyChangedEvent & PropertyChangeEvent);
#endif

};

/** UAudioAnalyzerNRT
 *
 * UAudioAnalyzerNRT applies an analyzer to a sound using specific settings, stores the 
 * results and exposes them via blueprints. 
 *
 * Subclasses of UAudioAnalyzerNRT must implement GetAnalyzerNRTFactoryName() to associate
 * the UAudioAnalyzerNRT with an IAudioAnalyzerNRTFactory implementation. 
 *
 * To support blueprint access, subclasses can implement UFUNCTIONs to expose the data
 * returned by GetResult().
 */
UCLASS(Abstract, EditInlineNew, BlueprintType)
class AUDIOANALYZER_API UAudioAnalyzerNRT : public UAudioAnalyzerAsset
{
	GENERATED_BODY()

	public:

		/**
		 * The USoundWave which is analyzed.
		 */
		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AudioAnalyzer)
		USoundWave* Sound;

		/**
		 * Returns the result object generated by the associated IAudioAnalyzerNRTFactory.
		 * The template argument must be the IAudioAnalyzerNRTResult subclass returned by
		 * the associated IAudioAnalyzerNRTFactory.
		 */
		template<class ResultType>
		TSharedPtr<ResultType, ESPMode::ThreadSafe> GetResult()
		{
			TSharedPtr<ResultType, ESPMode::ThreadSafe> ReturnedResult;
			{
				FScopeLock ResultLock(&ResultCriticalSection);
				ReturnedResult = StaticCastSharedPtr<ResultType>(Result); 
			}
			return ReturnedResult;
		}
	 
		/** 
		 * Implementations can override this method to create settings objects
		 * specific for their analyzer. 
		 */
		virtual TUniquePtr<Audio::IAnalyzerNRTSettings> GetSettings();

		/**
		 * Performs serialization of results.
		 */
		virtual void Serialize(FArchive& Ar) override;

		/* TODO: progress/status for calculation */

#if WITH_EDITOR

		void SetResult(TSharedPtr<Audio::IAnalyzerNRTResult, ESPMode::ThreadSafe> NewResult);

		/**
		 * Called before a UPROPERTY of this class is edited. This checks to see
		 * if the UPROPERTY is a UAudioAnalyzerNRTSettings. If so, the previous settings' 
		 * AnalyzeAudioDelegate will be unbound from this object's AnalyzeAudio()
		 */
		void PreEditChange(UProperty* PropertyAboutToChange) override;

		/**
		 * Called when a UPROPERTY of this class is edited. Triggering 
		 * reanalysis of audio when appropriate and binds new settings if the UPROPERTY
		 * is a UAudioAnalyzerNRTSettings derived object.
		 *
		 * This determines whether to trigger analysis by calling ShouldEventTriggerAnalysis(...)
		 */
		void PostEditChangeProperty (struct FPropertyChangedEvent & PropertyChangedEvent) override;

		/**
		 * Returns true when the PropertyChangeEvent is due to update SoundWave or Settings.
		 */
		virtual bool ShouldEventTriggerAnalysis(struct FPropertyChangedEvent & PropertyChangeEvent);

		/** Performs the analaysis of the audio */
		UFUNCTION()
		void AnalyzeAudio();
#endif


	protected:
		/* Subclasses must override this method in order to inform this object which AnalyzerNRTFactory to use for analysis */
		virtual FName GetAnalyzerNRTFactoryName() const PURE_VIRTUAL(UAudioAnalyzerNRT::GetAnalyzerNRTFactoryName, return FName(););

	private:

		// Returns UAudioAnalyzerNRTSettings* if property points to a valid UAudioAnalyzerNRTSettings, otherwise returns nullptr.
		UAudioAnalyzerNRTSettings* GetSettingsFromProperty(UProperty* Property);

		TSharedPtr<Audio::IAnalyzerNRTResult, ESPMode::ThreadSafe> Result;
		FCriticalSection ResultCriticalSection;
};

