////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper shader for IGCS Depth of Field, a multi-sampling adaptive depth of field system built into
// the IGCS Connector for IGCS powered camera tools.
//
// By Frans Bouma, aka Otis / Infuse Project (Otis_Inf) https://opm.fransbouma.com 
// and
// By Pascal Gilcher, aka Marty McFly  https://www.martysmods.com
// 
// Additional contributions: https://github.com/FransBouma/IgcsConnector/graphs/contributors
//
// This shader has been released under the following license:
//
// Copyright (c) 2024 Frans Bouma
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer. 
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ReShade.fxh"

namespace IgcsDOF
{
	#define IGCS_DOF_SHADER_VERSION "v2.5.4-tilt-test2"
	
// #define IGCS_DOF_DEBUG	
	
	// ------------------------------
	// Visible values
	// ------------------------------
						
	uniform float SetupAlpha <
		ui_label = "Setup alpha";
		ui_type = "drag";
		ui_min = 0.2; ui_max = 0.8;
		ui_step = 0.001;
	> = 0.5;

	uniform bool TiltedFocusPlaneEnabled <
		ui_category = "Tilted Focus Plane (TEST)";
		ui_label = "Enable tilted focus plane";
		ui_tooltip = "Tilts the focus plane around the normal focus pivot. The pivot itself remains unchanged.";
	> = false;

	uniform float TiltedFocusPlaneAngle <
		ui_category = "Tilted Focus Plane (TEST)";
		ui_label = "Tilt angle";
		ui_type = "drag";
		ui_min = -45.0; ui_max = 45.0;
		ui_step = 0.1;
		ui_tooltip = "Signed tilt angle in degrees. 0 keeps the original parallel focus plane.";
	> = 0.0;

	uniform float TiltedFocusPlaneRotation <
		ui_category = "Tilted Focus Plane (TEST)";
		ui_label = "Tilt rotation";
		ui_type = "drag";
		ui_min = 0.0; ui_max = 180.0;
		ui_step = 0.1;
		ui_tooltip = "Direction of the focus-depth gradient. 0 degrees = left/right, 90 degrees = top/bottom.";
	> = 0.0;

	uniform bool TiltedFocusPlaneTwoPass <
		ui_category = "Tilted Focus Plane (TEST)";
		ui_label = "Two-pass (+Tilt / -Tilt)";
		ui_tooltip = "Evaluates both tilt directions around the same focus pivot and blends them equally. This is a shader-side validation of the two-pass idea.";
	> = false;

	// ------------------------------
	// Hidden values, set by the connector
	// ------------------------------
	
	uniform float HighlightBoost <
		ui_category = "Highlight tweaking";
		ui_label="Highlight boost factor";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 1.00;
		ui_tooltip = "Will boost/dim the highlights a small amount";
		ui_step = 0.001;
		hidden=true;
	> = 0.50;
	uniform float SampleWeightR <
		ui_category = "Sample Weight Red";
		ui_label="Sample Weight for Red Channel for current sample";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 10.00;
		ui_step = 0.001;
		hidden=true;
	> = 1.0;
	uniform float SampleWeightG <
		ui_category = "Sample Weight Green";
		ui_label="Sample Weight for Green Channel for current sample";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 10.00;
		ui_step = 0.001;
		hidden=true;
	> = 1.0;
	uniform float SampleWeightB <
		ui_category = "Sample Weight Blue";
		ui_label="Sample Weight for Blue Channel for current sample";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 10.00;
		ui_step = 0.001;
		hidden=true;
	> = 1.0;
	uniform float HighlightGammaFactor <
		ui_category = "Highlight tweaking";
		ui_label="Highlight gamma factor";
		ui_type = "drag";
		ui_min = 0.001; ui_max = 5.00;
		ui_tooltip = "Controls the gamma factor to boost/dim highlights\n2.2, the default, gives natural colors and brightness";
		ui_step = 0.01;
		hidden=true;
	> = 2.2;
	
	uniform float FocusDelta <
		ui_label = "Focus delta";
		ui_type = "drag";
		ui_min = -1.0; ui_max = 1.0;
		ui_step = 0.001;
		hidden=true;
	> = 0.0f;

	uniform int SessionState < 
		ui_type = "combo";
		ui_min= 0; ui_max=1;
		ui_items="Off\0SessionStart\0Setup\0Render\0Done\0";
		ui_label = "Session state";
		hidden=true;
	> = 0;

	uniform bool BlendFrame <
		ui_label = "Blend frame";
		hidden=true;
	> = false;
	
	uniform float BlendFactor <
		ui_label = "Blend factor";
		ui_type = "drag";
		ui_min = 0.0f; ui_max = 1.0f;
		ui_step = 0.01f;
		hidden=true;
	> = 0.0f;
	
	uniform float2 AlignmentDelta <
		ui_type = "drag";
		ui_step = 0.001;
		ui_min = 0.000; ui_max = 1.000;
		hidden=true;
	> = float2(0.0f, 0.0f);

	uniform bool ShowMagnifier<
		ui_label = "Show magnifier";
		hidden=true;
	> = false;
	
	uniform float MagnificationFactor <
		ui_label = "MagnificationFactor";
		ui_type = "drag";
		ui_min = 1.0; ui_max = 10.0;
		ui_step = 1.0;
		hidden=true;
	> = 2.0;
	
	uniform float2 MagnificationArea <
		ui_type = "drag";
		ui_step = 0.001;
		ui_min = 0.01; ui_max = 1.000;
		hidden=true;
	> = float2(0.1f, 0.1f);

	uniform float2 MagnificationLocationCenter <
		ui_type = "drag";
		ui_step = 0.001;
		ui_min = 0.01; ui_max = 1.000;
		hidden=true;
	> = float2(0.5f, 0.5f);

	uniform float CateyeRadiusStart <
		ui_label = "Cateye Bokeh Radius start";
		ui_type = "drag";
		ui_min = 0.0; ui_max = 1.0;
		ui_step = 0.001;
		hidden=true;
	> = 0.2;
	uniform float CateyeRadiusEnd <
		ui_label = "Cateye Bokeh Radius end";
		ui_type = "drag";
		ui_min = 0.0; ui_max = 1.0;
		ui_step = 0.001;
		hidden=true;
	> = 0.7;
	uniform float CateyeIntensity <
		ui_label = "Cateye Bokeh Intensity";
		ui_type = "drag";
		ui_min = -1.0; ui_max = 1.0;
		ui_step = 0.001;
		hidden=true;
	> = 0.0;
	uniform bool CateyeVignette <		
		hidden=true;
	> = false;

	uniform bool VignettingEnabled <
		hidden=true;
	> = false;
	uniform float VignettingStart <
		hidden=true;
	> = 0.65;
	uniform float VignettingEnd <
		hidden=true;
	> = 1.0;
	uniform float VignettingStrength <
		hidden=true;
	> = 0.5;
	
#ifdef IGCS_DOF_DEBUG
	uniform bool DBBool1<
		ui_label = "DBG Bool1";
	> =false;
	uniform bool DBBool2<
		ui_label = "DBG Bool2";
	> =false;
	uniform bool DBBool3<
		ui_label = "DBG Bool3";
	> =false;
#endif

#ifndef BUFFER_PIXEL_SIZE
	#define BUFFER_PIXEL_SIZE	ReShade::PixelSize
#endif
#ifndef BUFFER_SCREEN_SIZE
	#define BUFFER_SCREEN_SIZE	ReShade::ScreenSize
#endif

	#define CEIL_DIV(num, denom) ((((num) - 1) / (denom)) + 1)

	sampler BackBufferPoint			{ Texture = ReShade::BackBufferTex; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; AddressW = CLAMP; };
	texture texBlendAccumulate 		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA32F; };
	sampler SamplerBlendAccumulate	{ Texture = texBlendAccumulate; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; };
	storage StorageBlendAccumulate  { Texture = texBlendAccumulate;  };

	texture texDisplay 		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGB10A2; };
	sampler SamplerDisplay	{ Texture = texDisplay;  MagFilter = POINT; MinFilter = POINT; MipFilter = POINT;};
	storage StorageDisplay  { Texture = texDisplay;  };
	
	float3 ConeOverlap(float3 fragment)
	{
		float k = 0.4 * 0.33;
		float2 f = float2(1 - 2 * k, k);
		float3x3 m = float3x3(f.xyy, f.yxy, f.yyx);
		return mul(fragment, m);
	}
	
	float3 ConeOverlapInverse(float3 fragment)
	{		
		float k = 0.4 * 0.33;
		float2 f = float2(k - 1, k) * rcp(3 * k - 1);
		float3x3 m = float3x3(f.xyy, f.yxy, f.yyx);
		return mul(fragment, m);
	}

	float3 AccentuateWhites(float3 fragment)
	{
		fragment = pow(abs(ConeOverlap(fragment)), HighlightGammaFactor);
		return fragment / max((1.001 - (HighlightBoost * fragment)), 0.001);
	}
	
	float3 CorrectForWhiteAccentuation(float3 fragment)
	{
		float3 toReturn = fragment / (1.001 + (HighlightBoost * fragment));
		return ConeOverlapInverse(pow(max(0, toReturn), 1.0 / HighlightGammaFactor)); 
	}

	float3 ReadHDRInput(float2 uv)
	{
		uv = uv * BUFFER_SCREEN_SIZE - 0.5;
		int2 gridStart = int2(uv);
		float2 gridPos = frac(uv);

		float4 weights = float4(gridPos, 1 - gridPos);
		weights = weights.zxzx * weights.wwyy;

		float3 tl = AccentuateWhites(tex2Dfetch(BackBufferPoint, gridStart + float2(0, 0)).rgb);
		float3 tr = AccentuateWhites(tex2Dfetch(BackBufferPoint, gridStart + float2(1, 0)).rgb);
		float3 bl = AccentuateWhites(tex2Dfetch(BackBufferPoint, gridStart + float2(0, 1)).rgb);
		float3 br = AccentuateWhites(tex2Dfetch(BackBufferPoint, gridStart + float2(1, 1)).rgb);

		return tl * weights.x + tr * weights.y + bl * weights.z + br * weights.w;		
	}	

	float3 goldenDither(uint2 p)
	{ 
		uint2 umagic = uint2(3242174889u, 2447445413u);
		uint3 ret = p.x * umagic.x + p.y * umagic.y;
		ret.y += (umagic.x + umagic.y) * 3u;
		ret.z += (umagic.x + umagic.y) * 7u;
		return float3(ret) * exp2(-32.0) - 0.5;
	}

	float linearstep(float lo, float hi, float x)
	{
		return saturate((x - lo) / (hi - lo));
	}

	float2 applyTiltedFocusPlaneWithAngle(float2 uv, float2 alignment, float tiltAngle)
	{
		if(!TiltedFocusPlaneEnabled || abs(tiltAngle) < 0.0001)
		{
			return alignment;
		}

		float2 planePosition = uv * 2.0 - 1.0;
		const float screenAspect = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
		planePosition.y /= screenAspect;
		planePosition /= length(float2(rcp(screenAspect), 1.0));

		const float rotationRadians = radians(TiltedFocusPlaneRotation);
		const float2 depthGradientAxis = float2(cos(rotationRadians), sin(rotationRadians));
		const float signedPosition = dot(planePosition, depthGradientAxis);
		const float tiltSlope = tan(radians(tiltAngle));
		const float denominator = max(0.15, 1.0 + (signedPosition * tiltSlope));
		return alignment / denominator;
	}

	float2 applyTiltedFocusPlane(float2 uv, float2 alignment)
	{
		return applyTiltedFocusPlaneWithAngle(uv, alignment, TiltedFocusPlaneAngle);
	}

	struct CSIN 
	{
		uint3 groupthreadid     : SV_GroupThreadID;         
		uint3 groupid           : SV_GroupID;            
		uint3 dispatchthreadid  : SV_DispatchThreadID;     
		uint threadid           : SV_GroupIndex;
	};


	void IGCSCS(in CSIN i)
	{
		float2 uv = (i.dispatchthreadid.xy + 0.5) * BUFFER_PIXEL_SIZE;
		
		if(SessionState <= 0
		|| SessionState >= 4
		|| i.dispatchthreadid.x >= BUFFER_WIDTH || i.dispatchthreadid.y >= BUFFER_HEIGHT) 
		{
			return;
		}
		
		else if(SessionState == 1)
		{
			float3 color = tex2Dfetch(BackBufferPoint, i.dispatchthreadid.xy).rgb;
			tex2Dstore(StorageBlendAccumulate, i.dispatchthreadid.xy, float4(color, 0));
			return;
		}
		else if(SessionState == 2)
		{
			float2 setupAlignment = applyTiltedFocusPlane(uv, float2(FocusDelta, 0.0));
			float2 shifted_uv = uv - setupAlignment;
			float3 currentFragment = tex2Dlod(ReShade::BackBuffer, float4(shifted_uv, 0, 0)).rgb;

			if(TiltedFocusPlaneEnabled && TiltedFocusPlaneTwoPass && abs(TiltedFocusPlaneAngle) >= 0.0001)
			{
				float2 setupAlignmentOpposite = applyTiltedFocusPlaneWithAngle(uv, float2(FocusDelta, 0.0), -TiltedFocusPlaneAngle);
				float2 shiftedUvOpposite = uv - setupAlignmentOpposite;
				float3 oppositeFragment = tex2Dlod(ReShade::BackBuffer, float4(shiftedUvOpposite, 0, 0)).rgb;
				currentFragment = (currentFragment + oppositeFragment) * 0.5;
			}

			float3 cachedFragment  = tex2Dfetch(StorageBlendAccumulate, i.dispatchthreadid.xy).rgb;
			float3 fragment = lerp(cachedFragment, currentFragment, SetupAlpha);
			tex2Dstore(StorageDisplay, i.dispatchthreadid.xy, float4(fragment, 1));
			return;
		}		

		[branch]
		if(BlendFrame)
		{
			const float2 aspectRatio = float2(1, float(BUFFER_PIXEL_SIZE.y) / float(BUFFER_PIXEL_SIZE.x));
			float2 alignmentToUse = applyTiltedFocusPlane(uv, AlignmentDelta.xy);
			float2 uvToReadFrom = uv + alignmentToUse * aspectRatio;

			bool isInside = all(saturate(uvToReadFrom - uvToReadFrom*uvToReadFrom));

			float4 result;
			result.rgb = ReadHDRInput(uvToReadFrom);
			result.a = isInside ? 1.0 : 0.0;
			result.rgb = isInside ? result.rgb : 0.0;

			if(TiltedFocusPlaneEnabled && TiltedFocusPlaneTwoPass && abs(TiltedFocusPlaneAngle) >= 0.0001)
			{
				float2 oppositeAlignment = applyTiltedFocusPlaneWithAngle(uv, AlignmentDelta.xy, -TiltedFocusPlaneAngle);
				float2 oppositeUv = uv + oppositeAlignment * aspectRatio;
				bool oppositeInside = all(saturate(oppositeUv - oppositeUv*oppositeUv));
				float3 oppositeColor = oppositeInside ? ReadHDRInput(oppositeUv) : 0.0;
				float oppositeAlpha = oppositeInside ? 1.0 : 0.0;

				result.rgb = (result.rgb + oppositeColor) * 0.5;
				result.a = (result.a + oppositeAlpha) * 0.5;
			}

			result.rgb *= float3(SampleWeightR, SampleWeightG, SampleWeightB);

			float focusDeltaSafe = abs(FocusDelta) > 1e-6 ? FocusDelta : (FocusDelta < 0.0 ? -1e-6 : 1e-6);
			float2 normalizedOffset = alignmentToUse / focusDeltaSafe * 2.0;
			float2 cateyeOffset = uv * 2 - 1;
			cateyeOffset.y /= BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
			cateyeOffset /= length(float2(rcp(BUFFER_WIDTH * BUFFER_RCP_HEIGHT), 1));

			float distFromCenter = length(cateyeOffset);
			cateyeOffset /= max(1e-6, distFromCenter);
			
            float catseyeFalloff = smoothstep(CateyeRadiusStart - 0.001, CateyeRadiusStart + 0.001, distFromCenter);
            float effectFactor = catseyeFalloff * step(0.001, abs(CateyeIntensity));
            float cateyeStrength = linearstep(CateyeRadiusStart, CateyeRadiusEnd, distFromCenter) * sqrt(2.0) * CateyeIntensity;

            cateyeOffset *= cateyeStrength * effectFactor;
            normalizedOffset += cateyeOffset;
            float cateyeMask = lerp(1.0, smoothstep(1.0, 0.98, length(normalizedOffset)), effectFactor);
			
			result.rgb *= cateyeMask;
			result.a *= CateyeVignette ? 1 : cateyeMask;

			if(VignettingEnabled && VignettingStrength > 0.0001)
			{
				float2 fieldOffset = uv * 2.0 - 1.0;
				fieldOffset.y /= BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
				fieldOffset /= length(float2(rcp(BUFFER_WIDTH * BUFFER_RCP_HEIGHT), 1.0));
				float fieldRadius = length(fieldOffset);

				float edgeProgress = smoothstep(VignettingStart, VignettingEnd, fieldRadius);
				float2 fieldDirection = fieldOffset / max(fieldRadius, 1e-5);
				float2 pupilSample = alignmentToUse / max(abs(FocusDelta), 1e-5) * 2.0;
				float pupilShift = edgeProgress * VignettingStrength;
				float pupilDistance = length(pupilSample + fieldDirection * pupilShift);
				float pupilMask = 1.0 - smoothstep(1.0, 1.04, pupilDistance);

				result.rgb *= pupilMask;
				result.a *= pupilMask;
			}
			
			if(BlendFactor < 0.75)
			{
				float4 prevAccumulator = tex2Dfetch(StorageBlendAccumulate, i.dispatchthreadid.xy);
				result += prevAccumulator;
			}

			tex2Dstore(StorageBlendAccumulate, i.dispatchthreadid.xy, result);

			result.rgb /= max(result.w, 1e-6); 
			result.rgb = CorrectForWhiteAccentuation(result.rgb);
			float3 dither = goldenDither(i.dispatchthreadid.xy);
			dither *= 0.999;
			dither *= exp2(-8);
			result.rgb = saturate(result.rgb + dither);
			tex2Dstore(StorageDisplay, i.dispatchthreadid.xy, float4(result.rgb, 1));			
		}
	}


	void VS_Output(in uint id : SV_VertexID, out float4 vpos : SV_Position, out float2 texcoord : TEXCOORD)
	{
		PostProcessVS(id, vpos, texcoord);		
		if(SessionState == 0)
		{
			vpos = asfloat(0x7F800000);
		}
	}

	void PS_Output(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float3 fragment : SV_Target0)
	{
		fragment = tex2Dlod(SamplerDisplay, float4(texcoord, 0, 0)).rgb;

		if(SessionState==2 && ShowMagnifier)
		{
			float2 areaTopLeft = MagnificationLocationCenter - (MagnificationArea / 2.0f);
			float2 areaBottomRight = MagnificationLocationCenter + (MagnificationArea / 2.0f);
			if(texcoord.x >= areaTopLeft.x && texcoord.y >= areaTopLeft.y && texcoord.x <= areaBottomRight.x && texcoord.y <= areaBottomRight.y)
			{
				float2 sourceCoord = ((texcoord - MagnificationLocationCenter) / MagnificationFactor) + MagnificationLocationCenter;
				fragment = tex2Dlod(SamplerDisplay, float4(sourceCoord, 0, 0)).rgb;
			}
		}
	}

	technique IgcsDOF
#if __RESHADE__ >= 40000
	< ui_tooltip = "IGCS Depth of Field worker shader "
			IGCS_DOF_SHADER_VERSION
			"\n===========================================\n\n"
			"IGCS DoF is an addon-powered advanced depth of field system which\n"
			"uses Otis_Inf's camera tools as well as the IgcsConnector Reshade Addon\n"
			"to produce realistic depth of field effects. This shader works only\n"
			"with the addon and camera tools present.\n\n"
			"IGCS DoF was written by:\nFrans 'Otis_Inf' Bouma, Pascal 'Marty McFly' Gilcher and contributors.\n"
			"https://opm.fransbouma.com | https://github.com/FransBouma/IgcsConnector"; >
#endif
	{
		pass { ComputeShader = IGCSCS<32, 32>;DispatchSizeX = CEIL_DIV(BUFFER_WIDTH, 32);DispatchSizeY = CEIL_DIV(BUFFER_HEIGHT, 32); }
		pass { VertexShader = VS_Output; PixelShader = PS_Output; }
	}
}