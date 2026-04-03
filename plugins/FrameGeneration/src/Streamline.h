#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#define NV_WINDOWS
#pragma warning(push)
#pragma warning(disable: 4471)
#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss_g.h>
#include <sl_version.h>
#pragma warning(pop)

class StreamlineFG
{
public:
	static StreamlineFG* GetSingleton()
	{
		static StreamlineFG singleton;
		return &singleton;
	}

	void LoadInterposer();
	bool InitStreamline();
	void SetD3DDevice(ID3D12Device* a_device);
	void UpgradeSwapChain(IDXGISwapChain4** a_swapChain);
	bool CheckAndEnableDLSSG();
	void Present(bool a_useFrameGen);
	void Shutdown();

	bool slInitialized = false;
	bool featureDLSSG = false;
	ID3D12Device* d3d12Device = nullptr;
	HMODULE interposer = nullptr;

	// Core SL function pointers
	PFun_slInit* slInit{};
	PFun_slShutdown* slShutdown{};
	PFun_slUpgradeInterface* slUpgradeInterface{};
	PFun_slSetD3DDevice* slSetD3DDevice{};
	PFun_slIsFeatureSupported* slIsFeatureSupported{};
	PFun_slGetFeatureFunction* slGetFeatureFunction{};

	// DLSS-G function pointers (loaded via slGetFeatureFunction)
	PFun_slDLSSGSetOptions* slDLSSGSetOptions{};
	PFun_slDLSSGGetState* slDLSSGGetState{};
};
