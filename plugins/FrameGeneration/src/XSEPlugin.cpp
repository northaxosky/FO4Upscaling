
#include "DX11Hooks.h"
#include "Upscaling.h"

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

void MessageHandler(F4SE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case F4SE::MessagingInterface::kPostPostLoad:
		{
			Upscaling::GetSingleton()->PostPostLoad();
			break;
		}
	}
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se, { .trampoline = true, .trampolineSize = 1 << 10 });

#ifndef NDEBUG
	while (!REX::W32::IsDebuggerPresent()) {};
#endif

	DX11Hooks::Install();

	Upscaling::GetSingleton()->LoadSettings();

	auto messaging = F4SE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);

	return true;
}
