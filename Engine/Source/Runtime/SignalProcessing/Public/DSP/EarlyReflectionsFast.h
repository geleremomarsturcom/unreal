// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/IntegerDelay.h"
#include "DSP/LongDelayAPF.h"
#include "DSP/BufferOnePoleLPF.h"
#include "DSP/FeedbackDelayNetwork.h"

namespace Audio
{
	struct SIGNALPROCESSING_API FEarlyReflectionsFastSettings
	{
		// Early reflections gain
		float Gain;

		// Delay between input signal and early reflections
		float PreDelayMsec;

		// Input sample bandwidth before entering early reflections
		float Bandwidth;

		// Early reflections decay (lower value is longer)
		float Decay;

		// Early reflection high frequency absorption factor
		float Absorption;

		FEarlyReflectionsFastSettings();

		bool operator==(const FEarlyReflectionsFastSettings& Other) const;

		bool operator!=(const FEarlyReflectionsFastSettings& Other) const;
	};

	// Basic implementation of early reflections using a predelay, low pass filter and feedback delay network (FDN). The FDN
	// utilizes four delay lines where each delay line consists of an all pass filter and low pass filter to control diffusion
	// and absorption.
	class SIGNALPROCESSING_API FEarlyReflectionsFast
	{
	public:
		
		// Limits for settings
		static const float MaxGain;
		static const float MinGain;
		static const float MaxPreDelay;
		static const float MinPreDelay;
		static const float MaxBandwidth;
		static const float MinBandwidth;
		static const float MaxDecay;
		static const float MinDecay;
		static const float MaxAbsorption;
		static const float MinAbsorption;
		
		static const FEarlyReflectionsFastSettings DefaultSettings;
		
		// InMaxNumInternalBufferSamples sets the maximum possible samples in an internal buffer.
		FEarlyReflectionsFast(float InSampleRate, int32 InMaxNumInternalBufferSamples, const FEarlyReflectionsFastSettings& InSettings=DefaultSettings);
		~FEarlyReflectionsFast();

		// Sets the reverb settings, clamps, applies, and updates
		void SetSettings(const FEarlyReflectionsFastSettings& InSettings);

		// Process the single audio frame
		void ProcessAudio(const AlignedFloatBuffer& InSamples, const int32 InNumChannels, AlignedFloatBuffer& OutLeftSamples, AlignedFloatBuffer& OutRightSamples);

		// Clamps settings to acceptable values. 
		static void ClampSettings(FEarlyReflectionsFastSettings& InOutSettings);

	private:
		void ApplySettings();

		FEarlyReflectionsFastSettings Settings;
		float SampleRate;

		AlignedFloatBuffer LeftInputBuffer;
		AlignedFloatBuffer LeftWorkBufferA;
		AlignedFloatBuffer LeftWorkBufferB;
		AlignedFloatBuffer RightInputBuffer;
		AlignedFloatBuffer RightWorkBufferA;
		AlignedFloatBuffer RightWorkBufferB;

		FFDNCoefficients LeftCoefficients;
		FFDNCoefficients RightCoefficients;

		FFeedbackDelayNetwork LeftFDN;
		FFeedbackDelayNetwork RightFDN;

		FIntegerDelay LeftPreDelay;
		FIntegerDelay RightPreDelay;

		FBufferOnePoleLPF LeftInputLPF;
		FBufferOnePoleLPF RightInputLPF;
	};
}
