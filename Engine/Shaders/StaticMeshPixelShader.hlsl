#include "ShaderCommon.hlsli"

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

cbuffer MaterialData : register(b2)
{
	float4 ColorTint;
	float UVScrollEnabled;
	float2 UVScrollSpeed;
	float UVScrollPadding;
};

float4 main(VS_OUTPUT Input) : SV_TARGET
{
	float2 FinalUV = Input.UV;
	if (UVScrollEnabled > 0.5f)
	{
		FinalUV += UVScrollSpeed * TotalTime;
		FinalUV = frac(FinalUV);
	}

	float4 TexColor = g_txColor.Sample(g_Sample, FinalUV);
	return TexColor * Input.Color;
}
