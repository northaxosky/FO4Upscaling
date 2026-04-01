#include "DX11Hooks.h"
#include "Upscaling.h"

#include "ENB/ENBSeriesAPI.h"

bool enbLoaded = false;

extern "C" DLLEXPORT constinit auto F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData data{};

	data.PluginVersion(Plugin::VERSION);
	data.PluginName(Plugin::NAME.data());
	data.AuthorName("");
	data.UsesAddressLibrary(true);
	data.UsesSigScanning(false);
	data.IsLayoutDependent(true);
	data.HasNoStructUse(false);
	data.CompatibleVersions({ F4SE::RUNTIME_LATEST });

	return data;
}();

#ifndef NDEBUG
void AddDebugInformation()
{
	auto rendererData = RE::BSGraphics::GetRendererData();

	for (uint32_t i = 0; i < 101; i++) {
		if (auto texture = reinterpret_cast<ID3D11Texture2D*>(rendererData->renderTargets[i].texture)) {
			auto name = std::format("RT {}", i);
			texture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
		}
		if (auto texture = reinterpret_cast<ID3D11Texture2D*>(rendererData->renderTargets[i].copyTexture)) {
			auto name = std::format("COPY RT {}", i);
			texture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
		}
		if (auto srView = reinterpret_cast<ID3D11ShaderResourceView*>(rendererData->renderTargets[i].srView)) {
			auto name = std::format("SRV {}", i);
			srView->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
		}
		if (auto copySRView = reinterpret_cast<ID3D11ShaderResourceView*>(rendererData->renderTargets[i].copySRView)) {
			auto name = std::format("COPY SRV {}", i);
			copySRView->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
		}
		if (auto rtView = reinterpret_cast<ID3D11RenderTargetView*>(rendererData->renderTargets[i].rtView)) {
			auto name = std::format("RTV {}", i);
			rtView->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
		}
		if (auto uaView = reinterpret_cast<ID3D11UnorderedAccessView*>(rendererData->renderTargets[i].uaView)) {
			auto name = std::format("UAV {}", i);
			uaView->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
		}
	}

	for (uint32_t i = 0; i < 13; i++) {
		if (auto texture = reinterpret_cast<ID3D11Texture2D*>(rendererData->depthStencilTargets[i].texture)) {
			auto name = std::format("DEPTH RT {}", i);
			texture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
		}
		if (auto srViewDepth = reinterpret_cast<ID3D11ShaderResourceView*>(rendererData->depthStencilTargets[i].srViewDepth)) {
			auto name = std::format("DS VIEW {}", i);
			srViewDepth->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
		}
	}
}
#endif

void OnInit(F4SE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case F4SE::MessagingInterface::kGameDataReady:
	{
		REX::INFO("Data loaded");
		Upscaling::GetSingleton()->OnDataLoaded();
#ifndef NDEBUG
		AddDebugInformation();
#endif
	}
	break;
	default:
		break;
	}
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

#ifndef NDEBUG
	while (!REX::W32::IsDebuggerPresent()) {};
#endif

	if (ENB_API::RequestENBAPI()) {
		REX::INFO("ENB detected");
		enbLoaded = true;
	} else {
		REX::INFO("ENB not detected");
	}

	DX11Hooks::Install();
	Upscaling::InstallHooks();

	const auto messaging = F4SE::GetMessagingInterface();
	messaging->RegisterListener(OnInit);

	return true;
}
