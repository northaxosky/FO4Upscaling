#include "DX11Hooks.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include "Upscaling.h"
#include "DX12SwapChain.h"
#include "FidelityFX.h"
#include "Streamline.h"

#include "ENB/ENBSeriesAPI.h"

bool enbLoaded = false;

decltype(&D3D11CreateDeviceAndSwapChain) ptrD3D11CreateDeviceAndSwapChain;
decltype(&IDXGIFactory::CreateSwapChain) ptrCreateSwapChain;

HRESULT WINAPI hk_IDXGIFactory_CreateSwapChain(IDXGIFactory2* This, _In_ ID3D11Device* a_device, _In_ DXGI_SWAP_CHAIN_DESC* pDesc, _COM_Outptr_ IDXGISwapChain** ppSwapChain)
{
	IDXGIDevice* dxgiDevice = nullptr;
	DX::ThrowIfFailed(a_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice));

	IDXGIAdapter* adapter = nullptr;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(&adapter));

	auto proxy = DX12SwapChain::GetSingleton();

	proxy->SetD3D11Device(a_device);

	ID3D11DeviceContext* context;
	a_device->GetImmediateContext(&context);
	proxy->SetD3D11DeviceContext(context);

	proxy->CreateD3D12Device(adapter);
	proxy->CreateSwapChain((IDXGIFactory5*)This, *pDesc);
	proxy->CreateInterop();

	*ppSwapChain = proxy->GetSwapChainProxy();

	return S_OK;
}

HRESULT WINAPI hk_D3D11CreateDeviceAndSwapChain(
	IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	IDXGISwapChain** ppSwapChain,
	ID3D11Device** ppDevice,
	D3D_FEATURE_LEVEL* pFeatureLevel,
	ID3D11DeviceContext** ppImmediateContext)
{
	auto upscaling = Upscaling::GetSingleton();

	if (pSwapChainDesc->Windowed) {
		// Log GPU info
		if (pAdapter) {
			DXGI_ADAPTER_DESC adapterDesc;
			if (SUCCEEDED(pAdapter->GetDesc(&adapterDesc))) {
				std::string gpuName;
				for (int i = 0; i < 128 && adapterDesc.Description[i]; i++)
					gpuName += static_cast<char>(adapterDesc.Description[i]);
				REX::INFO("[FG] GPU: {} (VRAM: {}MB, VendorId: {:#x}, DeviceId: {:#x})",
					gpuName, adapterDesc.DedicatedVideoMemory / (1024 * 1024),
					adapterDesc.VendorId, adapterDesc.DeviceId);
			}
		}

		REX::INFO("[Frame Generation] Frame Generation enabled, using D3D12 proxy");

		auto fidelityFX = FidelityFX::GetSingleton();

		bool hasBackend = fidelityFX->module;

		// For DLSS-G, initialize Streamline now that D3D context exists
		if (upscaling->settings.frameGenType == 1) {
			auto streamline = StreamlineFG::GetSingleton();
			if (streamline->interposer && !streamline->initialized) {
				streamline->Initialize();
			}
			if (streamline->initialized) {
				// Tentatively set DLSS-G — feature check deferred until after D3D12 device setup
				upscaling->activeFrameGenType = Upscaling::FrameGenType::kDLSSG;
				hasBackend = true;
			} else {
				REX::WARN("[FG] Streamline not initialized, falling back to FSR3");
				upscaling->activeFrameGenType = Upscaling::FrameGenType::kFSR3;
			}
		}

		if (hasBackend) {
			upscaling->d3d12Interop = true;
			upscaling->refreshRate = Upscaling::GetRefreshRate(pSwapChainDesc->OutputWindow);

			IDXGIFactory4* dxgiFactory;
			pAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));

			const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
			pFeatureLevels = &featureLevel;
			FeatureLevels = 1;

			if (enbLoaded) {
				*(uintptr_t*)&ptrCreateSwapChain = Detours::X64::DetourClassVTable(*(uintptr_t*)dxgiFactory, &hk_IDXGIFactory_CreateSwapChain, 10);
			}
			else {
				DX::ThrowIfFailed(D3D11CreateDevice(
					pAdapter,
					DriverType,
					Software,
					Flags,
					pFeatureLevels,
					FeatureLevels,
					SDKVersion,
					ppDevice,
					pFeatureLevel,
					ppImmediateContext));

				IDXGIDevice* dxgiDevice = nullptr;
				DX::ThrowIfFailed((*ppDevice)->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice));

				IDXGIAdapter* adapter = nullptr;
				DX::ThrowIfFailed(dxgiDevice->GetAdapter(&adapter));

				auto proxy = DX12SwapChain::GetSingleton();

				proxy->SetD3D11Device(*ppDevice);

				ID3D11DeviceContext* context;
				(*ppDevice)->GetImmediateContext(&context);
				proxy->SetD3D11DeviceContext(context);

				proxy->CreateD3D12Device(adapter);

				// For DLSS-G: upgrade D3D12 device BEFORE creating command queues
				if (upscaling->activeFrameGenType == Upscaling::FrameGenType::kDLSSG) {
					auto streamline = StreamlineFG::GetSingleton();

					// Upgrade device to Streamline proxy — intercepts CreateCommandQueue
					if (streamline->slUpgradeInterface) {
						ID3D12Device* rawDevice = proxy->d3d12Device.get();
						auto upgradeResult = streamline->slUpgradeInterface((void**)&rawDevice);
						REX::INFO("[DLSSG] Device upgrade result: {}", (int)upgradeResult);
						// rawDevice is now a proxy; update the stored device
						proxy->d3d12Device.copy_from(rawDevice);
					}

					streamline->SetD3DDevice(proxy->d3d12Device.get());
				}

				// Create command queues (Streamline intercepts CreateCommandQueue if DLSS-G)
				proxy->CreateD3D12CommandQueues();

				// Check DLSS-G support after device + queues are set up
				if (upscaling->activeFrameGenType == Upscaling::FrameGenType::kDLSSG) {
					auto streamline = StreamlineFG::GetSingleton();
					streamline->CheckFeatures(adapter);
					streamline->PostDevice();
					if (!streamline->featureDLSSG) {
						REX::WARN("[FG] DLSS-G not supported, falling back to FSR3");
						upscaling->activeFrameGenType = Upscaling::FrameGenType::kFSR3;
					} else {
						REX::INFO("[FG] DLSS-G confirmed available on RTX 4090");
					}
				}

				proxy->CreateSwapChain((IDXGIFactory5*)dxgiFactory, *pSwapChainDesc);
				proxy->CreateInterop();

				*ppSwapChain = proxy->GetSwapChainProxy();
				
				return S_OK;
			}

		} else {
			REX::WARN("[Frame Generation] No frame generation backend available, skipping proxy");
		}
	}

	auto ret = ptrD3D11CreateDeviceAndSwapChain(
		pAdapter,
		DriverType,
		Software,
		Flags,
		pFeatureLevels,
		FeatureLevels,
		SDKVersion,
		pSwapChainDesc,
		ppSwapChain,
		ppDevice,
		pFeatureLevel,
		ppImmediateContext);

	return ret;
}

void DX11Hooks::Install()
{
	if (ENB_API::RequestENBAPI()) {
		REX::INFO("ENB detected, using alternative swap chain hook");
		enbLoaded = true;
	} else {
		REX::INFO("ENB not detected, using standard swap chain hook");
	}

	auto upscaling = Upscaling::GetSingleton();
	auto fidelityFX = FidelityFX::GetSingleton();

	// Always load FidelityFX as fallback
	fidelityFX->LoadFFX();

	// Load Streamline interposer DLL if DLSS-G is requested (init deferred to device creation)
	if (upscaling->settings.frameGenType == 1) {
		REX::INFO("[FG] DLSS-G requested, loading Streamline interposer");
		auto streamline = StreamlineFG::GetSingleton();
		streamline->LoadInterposer();
		if (!streamline->interposer) {
			REX::WARN("[FG] Streamline interposer failed to load, will use FSR3 fallback");
		}
	}

	uintptr_t moduleBase = (uintptr_t)GetModuleHandle(nullptr);

	(uintptr_t&)ptrD3D11CreateDeviceAndSwapChain = Detours::IATHook(moduleBase, "d3d11.dll", "D3D11CreateDeviceAndSwapChain", (uintptr_t)hk_D3D11CreateDeviceAndSwapChain);
}
