// GenerateUIBufferCS.hlsl
// Extracts UI-only pixels with premultiplied alpha from the difference between
// the HUDLess scene (pre-UI) and the final backbuffer (post-UI).
//
// DLSS-G compositing formula:
//   Final_Color.RGB = UI.RGB + (1 - UI.Alpha) * Hudless.RGB
//
// We observe Final and Hudless, and need to produce UI with premultiplied alpha.
// For opaque UI (like Fallout 4's crosshair/compass): alpha ≈ 1 where UI exists.
// For the general case, we estimate alpha from the maximum channel difference.

Texture2D<float4> HUDLessColor : register(t0);   // Clean scene without UI
Texture2D<float4> BackbufferColor : register(t1); // Final frame with UI composited

RWTexture2D<float4> OutputUIColorAlpha : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float3 hudless = HUDLessColor[DTid.xy].rgb;
	float3 finalColor = BackbufferColor[DTid.xy].rgb;

	// Per-channel absolute difference
	float3 diff = abs(finalColor - hudless);
	float maxDiff = max(diff.r, max(diff.g, diff.b));

	// Threshold: ignore sub-pixel noise from compression/rounding
	if (maxDiff < 0.01) {
		OutputUIColorAlpha[DTid.xy] = float4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	// For opaque additive UI (Fallout 4 HUD):
	//   Final = UI_color + Hudless  (additive blend, alpha=1)
	//   UI_premultiplied = Final - Hudless = diff (signed)
	// For alpha-blended UI:
	//   Final = UI*alpha + Hudless*(1-alpha)
	//   alpha ≈ maxDiff (when UI and scene colors differ significantly)
	//
	// We treat alpha as 1.0 for any pixel that differs, since Fallout 4 HUD
	// elements are opaque overlays. The premultiplied RGB is just the difference.
	float alpha = saturate(maxDiff);

	// Premultiplied RGB: the UI contribution
	// For additive blend: UI_premul = Final - Hudless * (1 - alpha)
	float3 uiPremul = finalColor - hudless * (1.0 - alpha);
	uiPremul = max(uiPremul, float3(0.0, 0.0, 0.0));
	uiPremul = min(uiPremul, float3(1.0, 1.0, 1.0));

	OutputUIColorAlpha[DTid.xy] = float4(uiPremul, alpha);
}
