#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

// NGX SDK for DLSS Frame Generation
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_defs.h>

// Streamline for swap chain interception
#define NV_WINDOWS
#pragma warning(push)
#pragma warning(disable: 4471)
#include <sl.h>
#include <sl_consts.h>
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

	// Streamline interposer for swap chain interception
	void LoadInterposer();
	bool InitStreamline();
	void SetD3DDevice(ID3D12Device* a_device);
	void UpgradeSwapChain(IDXGISwapChain4** a_swapChain);

	// NGX for DLSS-G feature
	bool InitNGX(ID3D12Device* a_device);
	bool CreateFrameGenFeature(ID3D12GraphicsCommandList* a_cmdList, uint32_t a_width, uint32_t a_height);
	bool EvaluateFrameGen(ID3D12GraphicsCommandList* a_cmdList,
		ID3D12Resource* a_backbuffer,
		ID3D12Resource* a_depth,
		ID3D12Resource* a_motionVectors,
		ID3D12Resource* a_hudlessColor);
	void Shutdown();

	bool slInitialized = false;
	bool ngxInitialized = false;
	bool featureDLSSG = false;

	NVSDK_NGX_Handle* dlssgHandle = nullptr;
	NVSDK_NGX_Parameter* ngxParams = nullptr;
	ID3D12Device* d3d12Device = nullptr;
	HMODULE interposer = nullptr;

	// Streamline function pointers
	PFun_slInit* slInit{};
	PFun_slShutdown* slShutdown{};
	PFun_slUpgradeInterface* slUpgradeInterface{};
	PFun_slSetD3DDevice* slSetD3DDevice{};
	PFun_slSetConstants* slSetConstants{};
	PFun_slGetNewFrameToken* slGetNewFrameToken{};
	PFun_slEvaluateFeature* slEvaluateFeature{};
};
