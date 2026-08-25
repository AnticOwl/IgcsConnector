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
	#define IGCS_DOF_SHADER_VERSION "v2.5.4-tilt-cross-saddle-test1"
	
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

	uniform bool TiltedFocusPlaneEnabled < hidden=true; > = false;

	uniform int TiltedFocusPlaneMode < hidden=true; > = 0;

	uniform float TiltedFocusPlaneHorizontal < hidden=true; > = 0.0;

	uniform float TiltedFocusPlaneVertical < hidden=true; > = 0.0;

	uniform float TiltedFocusPlaneCrossSaddle < hidden=true; > = 0.0;

	uniform float TiltedFocusPlaneCrossSaddleRotation < hidden=true; > = 0.0;

	uniform float TiltedFocusPlaneCornerTL < hidden=true; > = 0.0;

	uniform float TiltedFocusPlaneCornerTR < hidden=true; > = 0.0;

	uniform float TiltedFocusPlaneCornerBL < hidden=true; > = 0.0;

	uniform float TiltedFocusPlaneCornerBR < hidden=true; > = 0.0;

	uniform float TiltedFocusPlanePivotX < hidden=true; > = 0.5;

	uniform float TiltedFocusPlanePivotY < hidden=true; > = 0.5;

	uniform bool TiltedFocusPlaneTwoPass < hidden=true; > = false;

	uniform bool TiltedFocusPlaneShowOverlay < hidden=true; > = true;




	uniform bool PetzvalBokehEnabled < hidden=true; > = false;
	uniform float PetzvalTangential < hidden=true; > = 0.0;
	uniform float PetzvalSagittal < hidden=true; > = 0.0;
	uniform float PetzvalStrength < hidden=true; > = 1.0;
	uniform float PetzvalCenterX < hidden=true; > = 0.5;
	uniform float PetzvalCenterY < hidden=true; > = 0.5;
	uniform bool PetzvalShowGuide < hidden=true; > = false;

	uniform bool LensDistortionEnabled < hidden=true; > = false;

	uniform bool LensDistortionShowGuide < hidden=true; > = false;

	uniform bool LensDistortionAutoFill < hidden=true; > = true;

	uniform float LensDistortionFillCrop < hidden=true; > = 1.0;

	uniform float LensDistortionStrength < hidden=true; > = 0.0;

	uniform float LensDistortionCurve < hidden=true; > = 0.0;

	uniform float LensDistortionCenterX < hidden=true; > = 0.5;

	uniform float LensDistortionCenterY < hidden=true; > = 0.5;

	uniform float LensDistortionStartRadius < hidden=true; > = 0.0;

	uniform float LensDistortionEndRadius < hidden=true; > = 1.0;

	uniform bool VignettingShowGuide < hidden=true; > = false;

	uniform float VignettingCenterX < hidden=true; > = 0.5;

	uniform float VignettingCenterY < hidden=true; > = 0.5;

	// ReShade overlay state. The same mechanism is used by Marty's focus helper:
	// the guide can appear only while its UI controls are actively edited.
	uniform bool OVERLAY_OPEN < source = "overlay_open"; >;
	uniform int ACTIVE_VARIABLE < source = "overlay_active"; >;
	uniform bool SCREENSHOT < source = "screenshot"; >;

	// ------------------------------
	// Hidden values, set by the connector
	// ------------------------------
	
	uniform float TiltPassSign < hidden=true; > = 1.0;

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

	float distortionRadius01(float2 uv)
	{
		const float screenAspect = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
		const float2 center = float2(LensDistortionCenterX, LensDistortionCenterY);
		float2 p = uv - center;
		p.x *= screenAspect;
		const float maxRadius = 0.5 * sqrt(screenAspect * screenAspect + 1.0);
		return length(p) / max(maxRadius, 1e-5);
	}

	float vignettingRadius01(float2 uv)
	{
		const float screenAspect = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
		float2 p = uv - float2(VignettingCenterX, VignettingCenterY);
		p.x *= screenAspect;
		const float maxRadius = 0.5 * sqrt(screenAspect * screenAspect + 1.0);
		return length(p) / max(maxRadius, 1e-5);
	}

	float distortionFillZoom()
	{
		float zoom = max(1.0, LensDistortionFillCrop);
		if(!LensDistortionAutoFill || !LensDistortionEnabled)
		{
			return zoom;
		}

		const float screenAspect = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
		const float2 center = float2(LensDistortionCenterX, LensDistortionCenterY);
		const float2 aspectScale = float2(screenAspect, 1.0);
		const float2 c0 = (float2(0.0, 0.0) - center) * aspectScale;
		const float2 c1 = (float2(1.0, 0.0) - center) * aspectScale;
		const float2 c2 = (float2(0.0, 1.0) - center) * aspectScale;
		const float2 c3 = (float2(1.0, 1.0) - center) * aspectScale;
		const float maxR2 = max(max(dot(c0, c0), dot(c1, c1)), max(dot(c2, c2), dot(c3, c3)));

		float maxOutwardTerm = max(0.0, LensDistortionStrength * maxR2 + LensDistortionCurve * maxR2 * maxR2);
		if(LensDistortionCurve < -1e-6)
		{
			const float vertexR2 = clamp(-LensDistortionStrength / (2.0 * LensDistortionCurve), 0.0, maxR2);
			maxOutwardTerm = max(maxOutwardTerm, LensDistortionStrength * vertexR2 + LensDistortionCurve * vertexR2 * vertexR2);
		}

		return zoom * max(1.0, 1.0 + maxOutwardTerm);
	}


	float petzvalBokehWeight(float2 uv, float2 apertureSample)
	{
		if(!PetzvalBokehEnabled || PetzvalStrength <= 0.0001 ||
		   (abs(PetzvalTangential) < 0.0001 && abs(PetzvalSagittal) < 0.0001))
		{
			return 1.0;
		}

		const float screenAspect = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
		float2 p = uv - float2(PetzvalCenterX, PetzvalCenterY);
		p.x *= screenAspect;

		const float radius = length(p);
		if(radius < 1e-6)
		{
			return 1.0;
		}

		const float maxRadius = max(
			length(float2(PetzvalCenterX * screenAspect, PetzvalCenterY)),
			max(
				length(float2((1.0 - PetzvalCenterX) * screenAspect, PetzvalCenterY)),
				max(
					length(float2(PetzvalCenterX * screenAspect, 1.0 - PetzvalCenterY)),
					length(float2((1.0 - PetzvalCenterX) * screenAspect, 1.0 - PetzvalCenterY))
				)
			)
		);
		const float edgeAmount = saturate(radius / max(maxRadius, 1e-6));

		float2 radial = p / radius;
		float2 tangent = float2(-radial.y, radial.x);

		float2 a = apertureSample;
		a.x *= screenAspect;

		const float radialComponent = dot(a, radial);
		const float tangentComponent = dot(a, tangent);

		// A Petzval-like local pupil ellipse. The camera alignment itself is untouched:
		// only the contribution of this aperture sample changes.
		// Amplify the existing Petzval model without changing its controls or UI.
		const float petzvalGain = PetzvalStrength * 3.0;
		const float radialScale = max(0.10, 1.0 + edgeAmount * PetzvalSagittal * petzvalGain);
		const float tangentScale = max(0.10, 1.0 + edgeAmount * PetzvalTangential * petzvalGain);

		const float shapedRadius = length(float2(
			radialComponent / radialScale,
			tangentComponent / tangentScale));

		// Smooth aperture clipping/weighting. Keep a tiny floor so the local accumulator
		// never becomes empty, and normalize later through result.a as usual.
		const float pupil = smoothstep(1.08, 0.90, shapedRadius);
		const float anisotropy = saturate(edgeAmount *
			max(abs(PetzvalTangential), abs(PetzvalSagittal)) *
			petzvalGain);
		return lerp(1.0, max(0.02, pupil), anisotropy);
	}

	float2 applyLensDistortion(float2 uv)
	{
		if(!LensDistortionEnabled || (abs(LensDistortionStrength) < 0.000001 && abs(LensDistortionCurve) < 0.000001))
		{
			return uv;
		}

		const float screenAspect = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
		const float2 center = float2(LensDistortionCenterX, LensDistortionCenterY);
		float2 p = uv - center;
		p.x *= screenAspect;
		p /= distortionFillZoom();

		const float r2 = dot(p, p);
		const float radius01 = distortionRadius01(uv);
		const float endRadius = max(LensDistortionEndRadius, LensDistortionStartRadius + 0.001);
		const float ramp = smoothstep(LensDistortionStartRadius, endRadius, radius01);
		const float scale = 1.0 + ramp * (LensDistortionStrength * r2 + LensDistortionCurve * r2 * r2);
		p *= scale;
		p.x /= screenAspect;

		return center + p;
	}

	float2 getTiltPlanePosition(float2 uv)
	{
		const float screenAspect = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
		float2 p = (uv - float2(TiltedFocusPlanePivotX, TiltedFocusPlanePivotY)) * 2.0;
		p.y /= screenAspect;
		p /= length(float2(rcp(screenAspect), 1.0));
		return p;
	}

	float getTiltField(float2 uv)
	{
		float2 p = getTiltPlanePosition(uv);

		// Plane: free horizontal + vertical linear gradient.
		if(TiltedFocusPlaneMode == 0)
		{
			const float tx = tan(radians(TiltedFocusPlaneHorizontal));
			const float ty = tan(radians(TiltedFocusPlaneVertical));
			return p.x * tx - p.y * ty;
		}

		// Cross / Saddle: x*y field evaluated in a rotatable local frame.
		if(TiltedFocusPlaneMode == 1)
		{
			const float rotationRadians = radians(TiltedFocusPlaneCrossSaddleRotation);
			const float c = cos(rotationRadians);
			const float s = sin(rotationRadians);
			const float2 q = float2(
				p.x * c - p.y * s,
				p.x * s + p.y * c);

			const float saddleSlope = tan(radians(TiltedFocusPlaneCrossSaddle));
			return (q.x * q.y) * saddleSlope * 2.0;
		}

		// Corner Control:
		// Each corner is independent, while the selected pivot remains exactly neutral.
		// Coordinates are normalized independently on each side of the pivot so the
		// four image corners remain -1/+1 even when the pivot is moved.
		const float2 pivot = float2(TiltedFocusPlanePivotX, TiltedFocusPlanePivotY);
		float2 q;
		q.x = uv.x >= pivot.x
			? (uv.x - pivot.x) / max(1.0 - pivot.x, 1e-5)
			: (uv.x - pivot.x) / max(pivot.x, 1e-5);
		q.y = uv.y >= pivot.y
			? (uv.y - pivot.y) / max(1.0 - pivot.y, 1e-5)
			: (uv.y - pivot.y) / max(pivot.y, 1e-5);
		q = clamp(q, -1.0, 1.0);

		const float u = q.x * 0.5 + 0.5;
		const float v = q.y * 0.5 + 0.5;

		const float kTL = tan(radians(TiltedFocusPlaneCornerTL));
		const float kTR = tan(radians(TiltedFocusPlaneCornerTR));
		const float kBL = tan(radians(TiltedFocusPlaneCornerBL));
		const float kBR = tan(radians(TiltedFocusPlaneCornerBR));

		const float top = lerp(kTL, kTR, u);
		const float bottom = lerp(kBL, kBR, u);
		const float cornerField = lerp(top, bottom, v);

		// Fade the corner field to zero at the pivot without changing the requested
		// values at the outer image edges/corners.
		const float pivotFade = max(abs(q.x), abs(q.y));
		return cornerField * pivotFade;
	}


	float2 applyTiltedFocusPlane(float2 uv, float2 alignment)
	{
		if(!TiltedFocusPlaneEnabled)
		{
			return alignment;
		}

		const float field = getTiltField(uv) * TiltPassSign;
		const float denominator = max(0.15, 1.0 + field);
		return alignment / denominator;
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
			const float2 warpedUv = applyLensDistortion(uv);
			float2 setupAlignment = applyTiltedFocusPlane(warpedUv, float2(FocusDelta, 0.0));
			float2 shifted_uv = warpedUv - setupAlignment;
			float3 currentFragment = tex2Dlod(ReShade::BackBuffer, float4(shifted_uv, 0, 0)).rgb;

			float3 cachedFragment  = tex2Dfetch(StorageBlendAccumulate, i.dispatchthreadid.xy).rgb;
			float3 fragment = lerp(cachedFragment, currentFragment, SetupAlpha);
			tex2Dstore(StorageDisplay, i.dispatchthreadid.xy, float4(fragment, 1));
			return;
		}		

		[branch]
		if(BlendFrame)
		{
			const float2 aspectRatio = float2(1, float(BUFFER_PIXEL_SIZE.y) / float(BUFFER_PIXEL_SIZE.x));
			const float2 warpedUv = applyLensDistortion(uv);
			float2 alignmentToUse = applyTiltedFocusPlane(warpedUv, AlignmentDelta.xy);
			float2 uvToReadFrom = warpedUv + alignmentToUse * aspectRatio;

			bool isInside = all(saturate(uvToReadFrom - uvToReadFrom*uvToReadFrom));

			float4 result;
			result.rgb = ReadHDRInput(uvToReadFrom);
			result.a = isInside ? 1.0 : 0.0;
			result.rgb = isInside ? result.rgb : 0.0;
			result.rgb *= float3(SampleWeightR, SampleWeightG, SampleWeightB);

			float focusDeltaSafe = abs(FocusDelta) > 1e-6 ? FocusDelta : (FocusDelta < 0.0 ? -1e-6 : 1e-6);
			float2 apertureSample = alignmentToUse / focusDeltaSafe * 2.0;
			const float petzvalWeight = petzvalBokehWeight(warpedUv, apertureSample);
			result.rgb *= petzvalWeight;
			result.a *= petzvalWeight;
			float2 normalizedOffset = apertureSample;
			float2 cateyeOffset = warpedUv * 2 - 1;
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

			// Optical vignetting adapted from Marty McFly / Pascal Gilcher's qUINT ADOF model.
			// Keep our Start/End/Strength/Center controls, but use Marty's soft quadratic
			// aperture-sample weighting instead of a hard pupil clip or simple screen darkening.
			if(VignettingEnabled)
			{
				float2 lensOffset = (warpedUv - float2(VignettingCenterX, VignettingCenterY)) * 2.0;
				lensOffset.y /= BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
				lensOffset /= length(float2(rcp(BUFFER_WIDTH * BUFFER_RCP_HEIGHT), 1.0));

				float vignetteRadius = length(lensOffset);
				float vignetteFalloff = linearstep(VignettingStart, VignettingEnd, vignetteRadius);
				float vignetteAmount = pow(saturate(vignetteFalloff), 0.75) * VignettingStrength * 3.0;
				float2 centerVec = vignetteRadius > 1e-6
					? (lensOffset / vignetteRadius) * vignetteAmount
					: float2(0.0, 0.0);

				float2 vignetteSample = apertureSample - centerVec;
				float vignetteMask = saturate(3.333 - dot(vignetteSample, vignetteSample) * 1.666);

				// Keep alpha untouched so rejected optical samples reduce light instead of
				// being normalized away during accumulation.
				result.rgb *= vignetteMask;
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

		if(SessionState==2 && TiltedFocusPlaneEnabled && !SCREENSHOT && TiltedFocusPlaneShowOverlay)
		{
			const float screenAspect = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
			const float2 pivot = float2(TiltedFocusPlanePivotX, TiltedFocusPlanePivotY);
			const float3 nearColor = float3(1.0, 0.16, 0.10);
			const float3 deepColor = float3(0.12, 0.42, 1.0);
			const float3 neutralColor = float3(0.92, 0.92, 0.92);
			const float pxAspect = BUFFER_PIXEL_SIZE.y;

			float field = getTiltField(texcoord);
			float amount = 0.0;
			if(TiltedFocusPlaneMode == 0)
			{
				amount = saturate(length(float2(TiltedFocusPlaneHorizontal, TiltedFocusPlaneVertical)) / 45.0);
			}
			else if(TiltedFocusPlaneMode == 1)
			{
				amount = saturate(abs(TiltedFocusPlaneCrossSaddle) / 45.0);
			}
			else
			{
				const float cornerMax = max(
					max(abs(TiltedFocusPlaneCornerTL), abs(TiltedFocusPlaneCornerTR)),
					max(abs(TiltedFocusPlaneCornerBL), abs(TiltedFocusPlaneCornerBR)));
				amount = saturate(cornerMax / 45.0);
			}
			float tint = 0.13 * amount * smoothstep(0.01, 0.65, abs(field));
			fragment = lerp(fragment, field >= 0.0 ? deepColor : nearColor, tint);

			float2 pAspect = (texcoord - pivot) * float2(screenAspect, 1.0);
			const float pivotHalfX = 13.0 * BUFFER_PIXEL_SIZE.x;
			const float pivotHalfY = 13.0 * BUFFER_PIXEL_SIZE.y;
			const bool pivotV = abs(texcoord.x - pivot.x) <= 1.35 * BUFFER_PIXEL_SIZE.x && abs(texcoord.y - pivot.y) <= pivotHalfY;
			const bool pivotH = abs(texcoord.y - pivot.y) <= 1.35 * BUFFER_PIXEL_SIZE.y && abs(texcoord.x - pivot.x) <= pivotHalfX;
			const float pivotRingDistance = length(pAspect);
			const bool pivotRing = abs(pivotRingDistance - 10.0 * pxAspect) <= 1.15 * pxAspect;

			if(TiltedFocusPlaneMode == 0)
			{
				float2 gradient = float2(tan(radians(TiltedFocusPlaneHorizontal)), -tan(radians(TiltedFocusPlaneVertical)));
				if(length(gradient) > 1e-5)
				{
					float2 deepAxis = normalize(gradient);
					float strength = saturate(length(float2(TiltedFocusPlaneHorizontal, TiltedFocusPlaneVertical)) / 45.0);
					float handleDistance = lerp(0.055, 0.33, strength);
					float2 handle = pivot + float2(deepAxis.x / screenAspect, deepAxis.y) * handleDistance;
					float2 hAspect = (handle - pivot) * float2(screenAspect, 1.0);
					float hLen2 = max(dot(hAspect, hAspect), 1e-8);
					float lineT = saturate(dot(pAspect, hAspect) / hLen2);
					float lineDistance = length(pAspect - hAspect * lineT);
					bool onGuideLine = lineT > 0.03 && lineT < 0.96 && lineDistance <= 1.25 * pxAspect;
					float2 handleDelta = (texcoord - handle) * float2(screenAspect, 1.0);
					float handleRadius = 9.0 * pxAspect;
					bool onHandle = abs(length(handleDelta) - handleRadius) <= 1.8 * pxAspect;
					if(onGuideLine) fragment = lerp(fragment, float3(0.72, 0.82, 1.0), 0.70);
					if(onHandle) fragment = lerp(fragment, deepColor, 0.96);
				}
			}
			else if(TiltedFocusPlaneMode == 1)
			{
				// Cross / Saddle: rotate the local axes first so the overlay follows
				// the exact same orientation as the optical field.
				float2 q = getTiltPlanePosition(texcoord);
				const float rotationRadians = radians(TiltedFocusPlaneCrossSaddleRotation);
				const float c = cos(rotationRadians);
				const float s = sin(rotationRadians);
				q = float2(
					q.x * c - q.y * s,
					q.x * s + q.y * c);

				float diagA = abs(q.x - q.y);
				float diagB = abs(q.x + q.y);
				bool onDiag = min(diagA, diagB) <= 2.0 * pxAspect;
				if(onDiag)
				{
					fragment = lerp(fragment, neutralColor, 0.62);
				}
			}
			else
			{
				// Corner Control: show four small signed corner markers.
				const float markerRadius = 9.0 * pxAspect;
				const float2 cTL = float2(0.03, 0.03);
				const float2 cTR = float2(0.97, 0.03);
				const float2 cBL = float2(0.03, 0.97);
				const float2 cBR = float2(0.97, 0.97);

				float2 dTL = (texcoord - cTL) * float2(screenAspect, 1.0);
				float2 dTR = (texcoord - cTR) * float2(screenAspect, 1.0);
				float2 dBL = (texcoord - cBL) * float2(screenAspect, 1.0);
				float2 dBR = (texcoord - cBR) * float2(screenAspect, 1.0);

				bool mTL = abs(length(dTL) - markerRadius) <= 1.8 * pxAspect;
				bool mTR = abs(length(dTR) - markerRadius) <= 1.8 * pxAspect;
				bool mBL = abs(length(dBL) - markerRadius) <= 1.8 * pxAspect;
				bool mBR = abs(length(dBR) - markerRadius) <= 1.8 * pxAspect;

				if(mTL) fragment = lerp(fragment, TiltedFocusPlaneCornerTL >= 0.0 ? deepColor : nearColor, 0.96);
				if(mTR) fragment = lerp(fragment, TiltedFocusPlaneCornerTR >= 0.0 ? deepColor : nearColor, 0.96);
				if(mBL) fragment = lerp(fragment, TiltedFocusPlaneCornerBL >= 0.0 ? deepColor : nearColor, 0.96);
				if(mBR) fragment = lerp(fragment, TiltedFocusPlaneCornerBR >= 0.0 ? deepColor : nearColor, 0.96);
			}

			if(pivotV || pivotH || pivotRing)
			{
				fragment = lerp(fragment, neutralColor, 0.92);
			}
		}


		if(SessionState==2 && PetzvalBokehEnabled && !SCREENSHOT && PetzvalShowGuide)
		{
			const float2 center = float2(PetzvalCenterX, PetzvalCenterY);
			const float screenAspect = BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
			const float px = BUFFER_PIXEL_SIZE.x;
			const float py = BUFFER_PIXEL_SIZE.y;
			const bool crossV = abs(texcoord.x - center.x) <= 1.5 * px && abs(texcoord.y - center.y) <= 22.0 * py;
			const bool crossH = abs(texcoord.y - center.y) <= 1.5 * py && abs(texcoord.x - center.x) <= 22.0 * px;

			float2 p = (texcoord - center) * float2(screenAspect, 1.0);
			const float radius = length(p);
			const float maxRadius = max(
				length(float2(PetzvalCenterX * screenAspect, PetzvalCenterY)),
				max(
					length(float2((1.0 - PetzvalCenterX) * screenAspect, PetzvalCenterY)),
					max(
						length(float2(PetzvalCenterX * screenAspect, 1.0 - PetzvalCenterY)),
						length(float2((1.0 - PetzvalCenterX) * screenAspect, 1.0 - PetzvalCenterY))
					)
				)
			);
			const float radius01 = radius / max(maxRadius, 1e-6);
			const bool innerRing = abs(radius01 - 0.35) <= 0.0035;
			const bool outerRing = abs(radius01 - 0.75) <= 0.0035;

			if(outerRing) fragment = lerp(fragment, float3(1.0, 0.35, 0.20), 0.85);
			if(innerRing) fragment = lerp(fragment, float3(0.95, 0.85, 0.20), 0.90);
			if(crossV || crossH) fragment = lerp(fragment, float3(0.20, 1.0, 0.60), 0.95);

			if(radius > 1e-6)
			{
				float2 radial = p / radius;
				float2 tangent = float2(-radial.y, radial.x);
				const float2 centerAspect = float2(center.x * screenAspect, center.y);
				const float2 posAspect = float2(texcoord.x * screenAspect, texcoord.y);

				float2 a = centerAspect - radial * 0.10;
				float2 b = centerAspect + radial * 0.10;
				float2 pa = posAspect - a;
				float2 ba = b - a;
				float t = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-6));
				float radialLine = 1.0 - smoothstep(0.002, 0.004, length(pa - ba * t));

				a = centerAspect - tangent * 0.08;
				b = centerAspect + tangent * 0.08;
				pa = posAspect - a;
				ba = b - a;
				t = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-6));
				float tangentLine = 1.0 - smoothstep(0.002, 0.004, length(pa - ba * t));

				if(radialLine > 0.01) fragment = lerp(fragment, float3(0.20, 0.75, 1.00), radialLine * 0.85);
				if(tangentLine > 0.01) fragment = lerp(fragment, float3(1.00, 0.60, 0.20), tangentLine * 0.85);
			}
		}

		if(SessionState==2 && LensDistortionEnabled && !SCREENSHOT && LensDistortionShowGuide)
		{
			const float2 center = float2(LensDistortionCenterX, LensDistortionCenterY);
			const float radius01 = distortionRadius01(texcoord);
			const float px = BUFFER_PIXEL_SIZE.x;
			const float py = BUFFER_PIXEL_SIZE.y;
			const float crossHalfX = 22.0 * px;
			const float crossHalfY = 22.0 * py;
			const bool crossV = abs(texcoord.x - center.x) <= 1.5 * px && abs(texcoord.y - center.y) <= crossHalfY;
			const bool crossH = abs(texcoord.y - center.y) <= 1.5 * py && abs(texcoord.x - center.x) <= crossHalfX;
			const float ringThickness = 0.0035;
			const bool onStartRing = abs(radius01 - LensDistortionStartRadius) <= ringThickness;
			const bool onEndRing = abs(radius01 - max(LensDistortionEndRadius, LensDistortionStartRadius + 0.001)) <= ringThickness;

			if(onEndRing) fragment = lerp(fragment, float3(1.0, 0.25, 0.15), 0.85);
			if(onStartRing) fragment = lerp(fragment, float3(1.0, 0.85, 0.15), 0.90);
			if(crossV || crossH) fragment = lerp(fragment, float3(0.15, 0.95, 1.0), 0.95);
		}

		if(SessionState==2 && VignettingShowGuide)
		{
			const float2 center = float2(VignettingCenterX, VignettingCenterY);
			const float radius01 = vignettingRadius01(texcoord);
			const float px = BUFFER_PIXEL_SIZE.x;
			const float py = BUFFER_PIXEL_SIZE.y;
			const float crossHalfX = 22.0 * px;
			const float crossHalfY = 22.0 * py;
			const bool crossV = abs(texcoord.x - center.x) <= 1.5 * px && abs(texcoord.y - center.y) <= crossHalfY;
			const bool crossH = abs(texcoord.y - center.y) <= 1.5 * py && abs(texcoord.x - center.x) <= crossHalfX;
			const float ringThickness = 0.0035;
			const bool onStartRing = abs(radius01 - VignettingStart) <= ringThickness;
			const bool onEndRing = abs(radius01 - VignettingEnd) <= ringThickness;

			if(onEndRing) fragment = lerp(fragment, float3(0.95, 0.20, 0.85), 0.85);
			if(onStartRing) fragment = lerp(fragment, float3(0.85, 0.75, 0.20), 0.90);
			if(crossV || crossH) fragment = lerp(fragment, float3(0.25, 1.0, 0.45), 0.95);
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
