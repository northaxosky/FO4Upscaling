#pragma once

#include "Buffer.h"

#include "SimpleIni.h"

class Upscaling
{
public:
	enum class FrameGenType : int { kFSR3 = 0, kDLSSG = 1 };

	static Upscaling* GetSingleton()
	{
		static Upscaling singleton;
		return &singleton;
	}

	struct Settings
	{
		bool frameGenerationMode = 1;
		bool frameLimitMode = 1;
		int frameGenType = 0;  // 0=FSR3, 1=DLSS-G
	};

	Settings settings;

	bool highFPSPhysicsFixLoaded = false;

	FrameGenType activeFrameGenType = FrameGenType::kFSR3;
	bool d3d12Interop = false;
	double refreshRate = 0.0f;

	Texture2D* HUDLessBufferShared[2];
	Texture2D* depthBufferShared[2];
	Texture2D* motionVectorBufferShared[2];
	
	winrt::com_ptr<ID3D12Resource> HUDLessBufferShared12[2];
	winrt::com_ptr<ID3D12Resource> depthBufferShared12[2];
	winrt::com_ptr<ID3D12Resource> motionVectorBufferShared12[2];

	ID3D11ComputeShader* copyDepthToSharedBufferCS = nullptr;
	ID3D11ComputeShader* generateSharedBuffersCS = nullptr;

	bool setupBuffers = false;

	void LoadSettings();
	void ReloadSettingsIfNeeded();

	void PostPostLoad();

	void CreateFrameGenerationResources();
	void PreAlpha();
	void PostAlpha();
	void CopyBuffersToSharedResources();

	static void TimerSleepQPC(int64_t targetQPC);

	void FrameLimiter(bool a_useFrameGeneration);

	void GameFrameLimiter();

	static double GetRefreshRate(HWND a_window);

	void PostDisplay();

	void Reset();

	static void InstallHooks();
};
