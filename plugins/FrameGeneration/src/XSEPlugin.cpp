
#include "DX11Hooks.h"
#include "Upscaling.h"

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
	F4SE::Init(a_f4se);

#ifndef NDEBUG
	while (!REX::W32::IsDebuggerPresent()) {};
#endif

	DX11Hooks::Install();

	Upscaling::GetSingleton()->LoadSettings();

	auto messaging = F4SE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);

	return true;
}
