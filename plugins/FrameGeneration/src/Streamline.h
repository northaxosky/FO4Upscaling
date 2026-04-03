#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

// NGX SDK for DLSS Frame Generation
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_defs.h>

class StreamlineFG
{
public:
	static StreamlineFG* GetSingleton()
	{
		static StreamlineFG singleton;
		return &singleton;
	}

	// NGX lifecycle
	bool InitNGX(ID3D12Device* a_device);
	bool CreateFrameGenFeature(ID3D12GraphicsCommandList* a_cmdList, uint32_t a_width, uint32_t a_height);
	bool EvaluateFrameGen(ID3D12GraphicsCommandList* a_cmdList,
		ID3D12Resource* a_backbuffer,
		ID3D12Resource* a_depth,
		ID3D12Resource* a_motionVectors,
		ID3D12Resource* a_hudlessColor);
	void Shutdown();

	bool ngxInitialized = false;
	bool featureDLSSG = false;

	NVSDK_NGX_Handle* dlssgHandle = nullptr;
	NVSDK_NGX_Parameter* ngxParams = nullptr;
	ID3D12Device* d3d12Device = nullptr;
};
