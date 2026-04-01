#include "DX11Hooks.h"
#include "Upscaling.h"

#include "ENB/ENBSeriesAPI.h"

bool enbLoaded = false;

// OG F4SE uses F4SEPlugin_Query, NG uses F4SEPlugin_Version. Export both.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface*, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION[0];
	return true;
}

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
		REX::INFO("[INIT] Data loaded");
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
	F4SE::InitInfo initInfo{};
	initInfo.logLevel = REX::LOG_LEVEL::DEBUG;
	initInfo.trampoline = true;
	initInfo.trampolineSize = 1 << 10;
	F4SE::Init(a_f4se, initInfo);

	REX::INFO("[INIT] AAAUpscaling loaded");

#ifndef NDEBUG
	while (!REX::W32::IsDebuggerPresent()) {};
#endif

	if (ENB_API::RequestENBAPI()) {
		REX::INFO("[ENB] ENB detected - FSR will be DISABLED, 7 hooks SKIPPED");
		enbLoaded = true;
	} else {
		REX::INFO("[ENB] ENB not detected - full pipeline active");
	}

	REX::INFO("[INIT] Installing DX11 hooks...");
	DX11Hooks::Install();
	REX::INFO("[INIT] DX11 hooks installed");

	REX::INFO("[INIT] Installing upscaling hooks...");
	Upscaling::InstallHooks();
	REX::INFO("[INIT] Upscaling hooks installed");

	const auto messaging = F4SE::GetMessagingInterface();
	messaging->RegisterListener(OnInit);

	return true;
}
