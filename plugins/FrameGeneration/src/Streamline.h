#pragma once

#define NV_WINDOWS

#pragma warning(push)
#pragma warning(disable: 4471)
#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss_g.h>
#include <sl_reflex.h>
#include <sl_version.h>
#pragma warning(pop)

#include <d3d12.h>
#include <dxgi1_6.h>

using PFun_slSetTag2 = sl::Result(const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer);

class StreamlineFG
{
public:
	static StreamlineFG* GetSingleton()
	{
		static StreamlineFG singleton;
		return &singleton;
	}

	~StreamlineFG()
	{
		if (interposer) {
			FreeLibrary(interposer);
			interposer = nullptr;
		}
	}

	void LoadInterposer();
	void Initialize();
	void CheckFeatures(IDXGIAdapter* a_adapter);
	void PostDevice();
	void SetD3DDevice(ID3D12Device* a_device);
	void UpgradeSwapChain(IDXGISwapChain4* a_swapChain);

	void SetDLSSGOptions(bool a_enabled, uint32_t a_numFrames = 1);
	void TagResources(ID3D12GraphicsCommandList* a_cmdList, ID3D12Resource* a_depth, ID3D12Resource* a_motionVectors, ID3D12Resource* a_hudlessColor);
	void UpdateConstants();
	void Present(bool a_useFrameGen);

	bool initialized = false;
	bool featureDLSSG = false;
	bool featureReflex = false;

	sl::ViewportHandle viewport{ 0 };
	sl::FrameToken* frameToken{};

	HMODULE interposer = NULL;

	// Core SL function pointers
	PFun_slInit* slInit{};
	PFun_slShutdown* slShutdown{};
	PFun_slIsFeatureSupported* slIsFeatureSupported{};
	PFun_slIsFeatureLoaded* slIsFeatureLoaded{};
	PFun_slSetFeatureLoaded* slSetFeatureLoaded{};
	PFun_slEvaluateFeature* slEvaluateFeature{};
	PFun_slSetTag2* slSetTag{};
	PFun_slGetFeatureRequirements* slGetFeatureRequirements{};
	PFun_slUpgradeInterface* slUpgradeInterface{};
	PFun_slSetConstants* slSetConstants{};
	PFun_slGetFeatureFunction* slGetFeatureFunction{};
	PFun_slGetNewFrameToken* slGetNewFrameToken{};
	PFun_slSetD3DDevice* slSetD3DDevice{};

	// DLSS-G function pointers
	PFun_slDLSSGSetOptions* slDLSSGSetOptions{};
	PFun_slDLSSGGetState* slDLSSGGetState{};

	// Reflex function pointers
	PFun_slReflexSetOptions* slReflexSetOptions{};
	PFun_slReflexGetState* slReflexGetState{};
};
