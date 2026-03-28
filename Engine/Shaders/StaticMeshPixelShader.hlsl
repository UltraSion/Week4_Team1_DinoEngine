#include "ShaderCommon.hlsli"

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

float4 main(VS_OUTPUT Input) : SV_TARGET
{
	float4 TexColor = g_txColor.Sample(g_Sample, Input.UV);
	return TexColor * Input.Color;
}
