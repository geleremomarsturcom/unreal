// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioCaptureCore.h"
#include "MagicLeapPluginUtil.h"

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_audio.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

namespace Audio
{
	class FAudioCaptureMagicLeapStream : public IAudioCaptureStream
	{
	public:
		FAudioCaptureMagicLeapStream();

		virtual bool GetCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo, int32 DeviceIndex) override;
		virtual bool OpenCaptureStream(const FAudioCaptureDeviceParams& InParams, FOnCaptureFunction InOnCapture, uint32 NumFramesDesired) override;
		virtual bool CloseStream() override;
		virtual bool StartStream() override;
		virtual bool StopStream() override;
		virtual bool AbortStream() override;
		virtual bool GetStreamTime(double& OutStreamTime) override;
		virtual int32 GetSampleRate() const override { return SampleRate; }
		virtual bool IsStreamOpen() const override;
		virtual bool IsCapturing() const override;
		virtual void OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow) override;
		virtual bool GetInputDevicesAvailable(TArray<FCaptureDeviceInfo>& OutDevices) override;

		TArray<float> FloatBuffer;
#if WITH_MLSDK
		MLHandle InputDeviceHandle;
#endif // WITH_MLSDK
		bool bStreamStarted;

		FCriticalSection ApplicationResumeCriticalSection;
		void OnApplicationSuspend();
		void OnApplicationResume();

	private:
		FOnCaptureFunction OnCapture;
		int32 NumChannels;
		int32 SampleRate;

	};
}