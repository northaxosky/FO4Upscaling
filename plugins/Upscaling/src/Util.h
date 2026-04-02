#pragma once

#include <d3d11.h>

namespace Util
{

	enum class RenderTarget
	{
		kFrameBuffer = 0,

		kRefractionNormal = 1,

		kMainPreAlpha = 2,
		kMain = 3,
		kMainTemp = 4,

		kSSRRaw = 7,
		kSSRBlurred = 8,
		kSSRBlurredExtra = 9,

		kMainVerticalBlur = 14,
		kMainHorizontalBlur = 15,

		kSSRDirection = 10,
		kSSRMask = 11,

		kUI = 17,
		kUITemp = 18,

		kGbufferNormal = 20,
		kGbufferNormalSwap = 21,
		kGbufferAlbedo = 22,
		kGbufferEmissive = 23,
		kGbufferMaterial = 24, //  Glossiness, Specular, Backlighting, SSS

		kSSAO = 28,

		kTAAAccumulation = 26,
		kTAAAccumulationSwap = 27,

		kMotionVectors = 29,

		kUIDownscaled = 36,
		kUIDownscaledComposite = 37,

		kMainDepthMips = 39,

		kUnkMask = 57,

		kSSAOTemp = 48,
		kSSAOTemp2 = 49,
		kSSAOTemp3 = 50,

		kDiffuseBuffer = 58,
		kSpecularBuffer = 59,

		kDownscaledHDR = 64,
		kDownscaledHDRLuminance2 = 65,
		kDownscaledHDRLuminance3 = 66,
		kDownscaledHDRLuminance4 = 67,
		kDownscaledHDRLuminance5Adaptation = 68,
		kDownscaledHDRLuminance6AdaptationSwap = 69,
		kDownscaledHDRLuminance6 = 70,

		kCount = 101
	};

	enum class DepthStencilTarget
	{
		kMainOtherOther = 0,
		kMainOther = 1,
		kMain = 2,
		kMainCopy = 3,
		kMainCopyCopy = 4,

		kShadowMap = 8,

		kCount = 13
	};

	[[nodiscard]] static RE::BSGraphics::State* State_GetSingleton()
	{
		REL::Relocation<RE::BSGraphics::State*> singleton{ REL::ID({ 600795, 2704621, 2704621 }) };
		return singleton.get();
	}

	[[nodiscard]] static RE::BSGraphics::RenderTargetManager* RenderTargetManager_GetSingleton()
	{
		REL::Relocation<RE::BSGraphics::RenderTargetManager*> singleton{ REL::ID({ 1508457, 2666735, 2666735 }) };
		return singleton.get();
	}

	// OG runtime has 101 render targets (vs 100 in NG/AE) in RenderTargetManager,
	// shifting all fields after the renderTargetData array by +0x30 (sizeof(RenderTarget)).
	// These accessors use absolute offsets from the struct base to read/write the correct fields.
	// OG offsets: dynamicWidthRatio=0xFB8, dynamicHeightRatio=0xFBC, isDynamic=0xFD8
	// NG offsets: dynamicWidthRatio=0xF88, dynamicHeightRatio=0xF8C, isDynamic=0xFA8

	[[nodiscard]] static float& DynamicWidthRatio(RE::BSGraphics::RenderTargetManager* rtm)
	{
		constexpr std::ptrdiff_t offsets[] = { 0xFB8, 0xF88, 0xF88 };
		auto offset = offsets[static_cast<uint8_t>(REL::Module::GetRuntimeIndex())];
		return *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(rtm) + offset);
	}

	[[nodiscard]] static float& DynamicHeightRatio(RE::BSGraphics::RenderTargetManager* rtm)
	{
		constexpr std::ptrdiff_t offsets[] = { 0xFBC, 0xF8C, 0xF8C };
		auto offset = offsets[static_cast<uint8_t>(REL::Module::GetRuntimeIndex())];
		return *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(rtm) + offset);
	}

	[[nodiscard]] static bool& IsDynamicResolutionCurrentlyActivated(RE::BSGraphics::RenderTargetManager* rtm)
	{
		constexpr std::ptrdiff_t offsets[] = { 0xFD8, 0xFA8, 0xFA8 };
		auto offset = offsets[static_cast<uint8_t>(REL::Module::GetRuntimeIndex())];
		return *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(rtm) + offset);
	}

	ID3D11DeviceChild* CompileShader(const wchar_t* FilePath, const std::vector<std::pair<const char*, const char*>>& Defines, const char* ProgramType, const char* Program = "main");
}
