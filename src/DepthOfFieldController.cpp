///////////////////////////////////////////////////////////////////////
//
// Part of IGCS Connector, an add on for Reshade 5+ which allows you
// to connect IGCS built camera tools with reshade to exchange data and control
// from Reshade.
// 
// (c) Frans 'Otis_Inf' Bouma.
//
// All rights reserved.
// https://github.com/FransBouma/IgcsConnector
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/////////////////////////////////////////////////////////////////////////
#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#include "stdafx.h"
#include "DepthOfFieldController.h"

#include "OverlayControl.h"
#include "Utils.h"
#include <random>
#include <algorithm>
#include "CDataFile.h"

DepthOfFieldController::DepthOfFieldController(CameraToolsConnector& connector) : _cameraToolsConnector(connector), _state(DepthOfFieldControllerState::Off), _quality(4), _numberOfPointsInnermostRing(3)
{
}


void DepthOfFieldController::setMaxBokehSize(reshade::api::effect_runtime* runtime, float newValue)
{
	if(DepthOfFieldControllerState::Setup != _state || newValue <=0.0f)
	{
		return;
	}

	const float oldValue = _maxBokehSize;
	_maxBokehSize = newValue;
	if(oldValue > 0.0f)
	{
		const float ratio = _maxBokehSize / oldValue;
		_focusDelta *= ratio;
	}
	calculateShapePoints();
	_cameraToolsConnector.moveCameraMultishot(_maxBokehSize, 0.0f, 0.0f, true);
}


void DepthOfFieldController::setXFocusDelta(reshade::api::effect_runtime* runtime, float newValueX)
{
	if(DepthOfFieldControllerState::Setup!=_state)
	{
		return;
	}
	_focusDelta = newValueX;
	calculateShapePoints();
	setUniformFloatVariable(runtime, "FocusDelta", _focusDelta);
}


void DepthOfFieldController::displayScreenshotSessionStartError(const ScreenshotSessionStartReturnCode sessionStartResult)
{
	std::string reason = "Unknown error.";
	switch(sessionStartResult)
	{
		case ScreenshotSessionStartReturnCode::Error_CameraNotEnabled:
			reason = "you haven't enabled the camera.";
			break;
		case ScreenshotSessionStartReturnCode::Error_CameraPathPlaying:
			reason = "there's a camera path playing.";
			break;
		case ScreenshotSessionStartReturnCode::Error_AlreadySessionActive:
			reason = "there's already a session active.";
			break;
		case ScreenshotSessionStartReturnCode::Error_CameraFeatureNotAvailable:
			reason = "the camera feature isn't available in the tools.";
			break;
	}
	OverlayControl::addNotification("Depth-of-field session couldn't be started: " + reason);
}


void DepthOfFieldController::writeVariableStateToShader(reshade::api::effect_runtime* runtime)
{
	if(isReshadeStateEmpty())
	{
		std::scoped_lock lock(_reshadeStateMutex);
		_reshadeStateAtStart.obtainReshadeState(runtime);
	}

	setUniformIntVariable(runtime, "SessionState", (int)_state);
	setUniformFloatVariable(runtime, "FocusDelta", _focusDelta);
	setUniformBoolVariable(runtime, "BlendFrame", _blendFrame);
	setUniformFloatVariable(runtime, "BlendFactor", _blendFactor);
	setUniformFloat2Variable(runtime, "AlignmentDelta", _xAlignmentDelta, _yAlignmentDelta);
	setUniformFloatVariable(runtime, "HighlightBoost", _highlightBoostFactor);

	setUniformFloatVariable(runtime, "SampleWeightR", _sampleWeightRGB[0]);
	setUniformFloatVariable(runtime, "SampleWeightG", _sampleWeightRGB[1]);
	setUniformFloatVariable(runtime, "SampleWeightB", _sampleWeightRGB[2]);

	setUniformFloatVariable(runtime, "HighlightGammaFactor", _highlightGammaFactor);
	setUniformBoolVariable(runtime, "ShowMagnifier", _magnificationSettings.ShowMagnifier);
	setUniformFloatVariable(runtime, "MagnificationFactor", _magnificationSettings.MagnificationFactor);
	setUniformFloat2Variable(runtime, "MagnificationArea", _magnificationSettings.WidthMagnifierArea, _magnificationSettings.HeightMagnifierArea);
	setUniformFloat2Variable(runtime, "MagnificationLocationCenter", _magnificationSettings.XMagnifierLocation, _magnificationSettings.YMagnifierLocation);

	setUniformBoolVariable(runtime, "CateyeVignette", _addCatEyeVignette);
	setUniformFloatVariable(runtime, "CateyeRadiusStart", _catEyeRadiusStart);
	setUniformFloatVariable(runtime, "CateyeRadiusEnd", _catEyeRadiusEnd);
	setUniformFloatVariable(runtime, "CateyeIntensity", _catEyeBokehIntensity);
}


void DepthOfFieldController::loadIniFileData(CDataFile& iniFile)
{
	loadFloatFromIni(iniFile, "MaxBokehSize", &_maxBokehSize);
	loadFloatFromIni(iniFile, "HighlightBoostFactor", &_highlightBoostFactor);
	loadFloatFromIni(iniFile, "HighlightGammaFactor", &_highlightGammaFactor);
	loadFloatFromIni(iniFile, "MagnificationAreaWidth", &_magnificationSettings.WidthMagnifierArea);
	loadFloatFromIni(iniFile, "MagnificationAreaHeight", &_magnificationSettings.HeightMagnifierArea);
	loadFloatFromIni(iniFile, "AnamorphicFactor", &_anamorphicFactor);
	loadBoolFromIni(iniFile, "AstigmatismEnabled", &_astigmatismEnabled, false);
	loadFloatFromIni(iniFile, "AstigmatismStrength", &_astigmatismStrength);
	loadFloatFromIni(iniFile, "AstigmatismFocusShiftStrength", &_astigmatismFocusShiftStrength);
	loadFloatFromIni(iniFile, "AstigmatismRotation", &_astigmatismRotation);
	_astigmatismStrength = IGCS::Utils::clampEx(_astigmatismStrength, 0.0f, 1.0f);
	_astigmatismFocusShiftStrength = IGCS::Utils::clampEx(_astigmatismFocusShiftStrength, 0.0f, 1.0f);
	_astigmatismRotation = IGCS::Utils::clampEx(_astigmatismRotation, 0.0f, 180.0f);
	loadFloatFromIni(iniFile, "RingAngleOffset", &_ringAngleOffset);
	loadFloatFromIni(iniFile, "RotationAngle", &_apertureShapeSettings.RotationAngle);
	loadFloatFromIni(iniFile, "RoundFactor", &_apertureShapeSettings.RoundFactor);
	loadFloatFromIni(iniFile, "SphericalAberrationDimFactor", &_sphericalAberrationDimFactor);
	loadFloatFromIni(iniFile, "FringeIntensity", &_fringeIntensity);
	loadFloatFromIni(iniFile, "FringeWidth", &_fringeWidth);
	loadFloatFromIni(iniFile, "CAStrength", &_caStrength);
	loadFloatFromIni(iniFile, "CAWidth", &_caWidth);
	loadIntFromIni(iniFile, "NumberOfVertices", &_apertureShapeSettings.NumberOfVertices);
	loadIntFromIni(iniFile, "Quality", &_quality);
	loadIntFromIni(iniFile, "NumberOfPointsInnermostRing", &_numberOfPointsInnermostRing);
	loadIntFromIni(iniFile, "NumberOfFramesToWaitPerFrame", &_numberOfFramesToWait);
	loadIntFromIni(iniFile, "NumberOfFramesInFlight", &_numberOfFramesInFlight);
	loadBoolFromIni(iniFile, "ShowProgressBarAsOverlay", &_showProgressBarAsOverlay, true);
	loadBoolFromIni(iniFile, "AddCatEyeVignette", &_addCatEyeVignette, false);
	loadFloatFromIni(iniFile, "CatEyeRadiusStart", &_catEyeRadiusStart);
	loadFloatFromIni(iniFile, "CatEyeRadiusEnd", &_catEyeRadiusEnd);
	loadFloatFromIni(iniFile, "CatEyeBokehIntensity", &_catEyeBokehIntensity);

	int intValueFromIni = 0;
	loadIntFromIni(iniFile, "BlurType", &intValueFromIni);
	_blurType = (DepthOfFieldBlurType)intValueFromIni;
	loadIntFromIni(iniFile, "CAType", &intValueFromIni);
	_caType = (DepthOfFieldCAType)intValueFromIni;
}


void DepthOfFieldController::saveIniFileData(CDataFile& iniFile)
{
	iniFile.SetFloat("MaxBokehSize", _maxBokehSize, "", "DepthOfField");
	iniFile.SetFloat("HighlightBoostFactor", _highlightBoostFactor, "", "DepthOfField");
	iniFile.SetFloat("HighlightGammaFactor", _highlightGammaFactor, "", "DepthOfField");
	iniFile.SetFloat("MagnificationAreaWidth", _magnificationSettings.WidthMagnifierArea, "", "DepthOfField");
	iniFile.SetFloat("MagnificationAreaHeight", _magnificationSettings.HeightMagnifierArea, "", "DepthOfField");
	iniFile.SetFloat("AnamorphicFactor", _anamorphicFactor, "", "DepthOfField");
	iniFile.SetBool("AstigmatismEnabled", _astigmatismEnabled, "", "DepthOfField");
	iniFile.SetFloat("AstigmatismStrength", _astigmatismStrength, "", "DepthOfField");
	iniFile.SetFloat("AstigmatismFocusShiftStrength", _astigmatismFocusShiftStrength, "", "DepthOfField");
	iniFile.SetFloat("AstigmatismRotation", _astigmatismRotation, "", "DepthOfField");
	iniFile.SetFloat("RingAngleOffset", _ringAngleOffset, "", "DepthOfField");
	iniFile.SetFloat("RotationAngle", _apertureShapeSettings.RotationAngle, "", "DepthOfField");
	iniFile.SetFloat("RoundFactor", _apertureShapeSettings.RoundFactor, "", "DepthOfField");
	iniFile.SetFloat("SphericalAberrationDimFactor", _sphericalAberrationDimFactor, "", "DepthOfField");
	iniFile.SetFloat("FringeIntensity", _fringeIntensity, "", "DepthOfField");
	iniFile.SetFloat("FringeWidth", _fringeWidth, "", "DepthOfField");
	iniFile.SetFloat("CAStrength", _caStrength, "", "DepthOfField");
	iniFile.SetFloat("CAWidth", _caWidth, "", "DepthOfField");
	iniFile.SetInt("NumberOfVertices", _apertureShapeSettings.NumberOfVertices, "", "DepthOfField");
	iniFile.SetInt("Quality", _quality, "", "DepthOfField");
	iniFile.SetInt("NumberOfPointsInnermostRing", _numberOfPointsInnermostRing, "", "DepthOfField");
	iniFile.SetInt("NumberOfFramesToWaitPerFrame", _numberOfFramesToWait, "", "DepthOfField");
	iniFile.SetInt("NumberOfFramesInFlight", _numberOfFramesInFlight, "", "DepthOfField");
	iniFile.SetBool("ShowProgressBarAsOverlay", _showProgressBarAsOverlay, "", "DepthOfField");
	iniFile.SetInt("BlurType", (int)_blurType, "", "DepthOfField");
	iniFile.SetInt("CAType", (int)_caType, "", "DepthOfField");
	iniFile.SetBool("AddCatEyeVignette", _addCatEyeVignette, "", "DepthOfField");
	iniFile.SetFloat("CatEyeRadiusStart", _catEyeRadiusStart, "", "DepthOfField");
	iniFile.SetFloat("CatEyeRadiusEnd", _catEyeRadiusEnd, "", "DepthOfField");
	iniFile.SetFloat("CatEyeBokehIntensity", _catEyeBokehIntensity, "", "DepthOfField");
}


void DepthOfFieldController::startSession(reshade::api::effect_runtime* runtime)
{
	if(!_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}
	const auto sessionStartResult = _cameraToolsConnector.startScreenshotSession((uint8_t)ScreenshotType::MultiShot);
	if (sessionStartResult != ScreenshotSessionStartReturnCode::AllOk)
	{
		displayScreenshotSessionStartError(sessionStartResult);
		return;
	}

	calculateShapePoints();

	{
		std::scoped_lock lock(_reshadeStateMutex);
		_reshadeStateAtStart.obtainReshadeState(runtime);
	}

	_state = DepthOfFieldControllerState::Start;
	_renderPaused = false;
	setUniformIntVariable(runtime, "SessionState", (int)_state);
	_onPresentWorkCounter = 3;
	_onPresentWorkFunc = [&](reshade::api::effect_runtime* r)
	{
		this->_state = DepthOfFieldControllerState::Setup;
		_cameraToolsConnector.moveCameraMultishot(_maxBokehSize, 0.0f, 0.0f, true);
	};
}


void DepthOfFieldController::endSession(reshade::api::effect_runtime* runtime)
{
	_state = DepthOfFieldControllerState::Off;
	_renderPaused = false;
	setUniformIntVariable(runtime, "SessionState", (int)_state);

	if(_cameraToolsConnector.cameraToolsConnected())
	{
		_cameraToolsConnector.endScreenshotSession();
	}
}


void DepthOfFieldController::reshadeBeginEffectsCalled(reshade::api::effect_runtime* runtime)
{
	if(nullptr==runtime || !_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}
	if(_onPresentWorkCounter<=0)
	{
		_onPresentWorkCounter = 0;

		if(nullptr!=_onPresentWorkFunc)
		{
			const std::function<void(reshade::api::effect_runtime*)> funcToCall = _onPresentWorkFunc;
			_onPresentWorkFunc = nullptr;
			funcToCall(runtime);
		}
	}
	else
	{
		_onPresentWorkCounter--;
	}

	if(DepthOfFieldControllerState::Rendering== _state)
	{
		handlePresentBeforeReshadeEffects();
	}

	writeVariableStateToShader(runtime);
}


void DepthOfFieldController::reshadeFinishEffectsCalled(reshade::api::effect_runtime* runtime)
{
	if(nullptr == runtime || !_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}

	if(DepthOfFieldControllerState::Rendering == _state)
	{
		handlePresentAfterReshadeEffects();
	}
}


void DepthOfFieldController::performRenderFrameSetupWork()
{
	if(_currentStepFrame < _cameraSteps.size())
	{
		const auto& currentStepFrameData = _cameraSteps[_currentStepFrame];
		_cameraToolsConnector.moveCameraMultishot(currentStepFrameData.xDelta, currentStepFrameData.yDelta, 0.0f, true);
		_currentStepFrame++;
		_stepCounter=_numberOfFramesToWait;
	}
	if(_currentBlendFrame >= 0)
	{
		const auto& currentBlendFrameData = _cameraSteps[_currentBlendFrame];
		_xAlignmentDelta = currentBlendFrameData.xAlignmentDelta;
		_yAlignmentDelta = currentBlendFrameData.yAlignmentDelta;
		_blendFactor = 1.0f / (static_cast<float>(_currentBlendFrame) + 1.0f);

		const float numSamples = static_cast<float>(_cameraSteps.size());
		_sampleWeightRGB[0] = currentBlendFrameData.sampleWeightRGB[0] * numSamples;
		_sampleWeightRGB[1] = currentBlendFrameData.sampleWeightRGB[1] * numSamples;
		_sampleWeightRGB[2] = currentBlendFrameData.sampleWeightRGB[2] * numSamples;
	}
	_renderFrameState = DepthOfFieldRenderFrameState::RenderingFrames;
}


void DepthOfFieldController::handlePresentBeforeReshadeEffects()
{
	if(_state!=DepthOfFieldControllerState::Rendering)
	{
		return;
	}

	switch(_renderFrameState)
	{
		case DepthOfFieldRenderFrameState::Off:
		case DepthOfFieldRenderFrameState::Start:
			break;
		case DepthOfFieldRenderFrameState::RenderingFrames:
			{
				if(!_renderPaused)
				{
					if(_blendCounter <= 0)
					{
						_blendCounter = 0;
						if(_currentBlendFrame >= 0)
						{
							_blendFrame = true;
						}
					}
					else
					{
						_blendCounter--;
					}
				}
			}
			break;
	}
}


void DepthOfFieldController::handlePresentAfterReshadeEffects()
{
	if(_state != DepthOfFieldControllerState::Rendering)
	{
		return;
	}

	switch(_renderFrameState)
	{
		case DepthOfFieldRenderFrameState::Off:
			break;
		case DepthOfFieldRenderFrameState::Start:
			performRenderFrameSetupWork();
			break;
		case DepthOfFieldRenderFrameState::RenderingFrames:
			{
				if(_blendFrame)
				{
					_blendFrame = false;
					_currentBlendFrame++;
					_blendCounter = _numberOfFramesToWait;
				}
				if(_currentBlendFrame >= _numberOfFramesToRender)
				{
					_renderFrameState = DepthOfFieldRenderFrameState::Off;
					_state = DepthOfFieldControllerState::Done;
					reshade::log::message(reshade::log::level::info, "Dof render session completed");
				}
				else
				{
					if(!_renderPaused)
					{
						if(_stepCounter <= 0)
						{
							_stepCounter = 0;
							performRenderFrameSetupWork();
						}
						else
						{
							_stepCounter--;
						}
					}
				}
			}
			break;
	}
}


void DepthOfFieldController::applySphericalAberration(float radiusNormalized, CameraLocation& sample)
{
	float aberrationCurve = radiusNormalized * radiusNormalized;
	aberrationCurve *= aberrationCurve;
	const float aberrationFactor = (1.0f - _sphericalAberrationDimFactor * 0.99f) + _sphericalAberrationDimFactor * aberrationCurve * 0.99f;

	sample.sampleWeightRGB[0] *= aberrationFactor;
	sample.sampleWeightRGB[1] *= aberrationFactor;
	sample.sampleWeightRGB[2] *= aberrationFactor;
}


float DepthOfFieldController::calculateChannelDimFactor(float angleSegment, float segmentAngleMin, int numberOfSegments)
{
	const float segmentSize = 1.0f / (float)(numberOfSegments ==0 ? 1 : numberOfSegments);
	const float angleToSegmentNormalized = IGCS::Utils::clampEx(angleSegment - segmentAngleMin, 0.0f, segmentSize) / segmentSize;
	return std::pow(4.0f * angleToSegmentNormalized * (1.0f - angleToSegmentNormalized), 0.5f);
}


void DepthOfFieldController::applyFringe(float ringRadiusNormalized, float sampleAngle, CameraLocation& sample)
{
	const float transitionWidth = 0.5f / (float)_quality;
	const float fringeRampStart = 1.0f - _fringeWidth - transitionWidth;
	const float fringeRampEnd   = 1.0f - _fringeWidth + transitionWidth;
	const float fringeMask = IGCS::Utils::clampEx((ringRadiusNormalized - fringeRampStart) / (fringeRampEnd - fringeRampStart), 0.0f, 1.0f);
	const float fringeFactor = (1.0f - _fringeIntensity) * (1.0f - fringeMask) + fringeMask;

	float blueFactor = 1.0f;
	float greenFactor = 1.0f;
	float redFactor = 1.0f;
	const float angleSegment = sampleAngle / 6.28318530717958f;

	DepthOfFieldColorChannel segmentOneProminentColor = DepthOfFieldColorChannel::Red;
	DepthOfFieldColorChannel segmentTwoProminentColor = DepthOfFieldColorChannel::Green;
	DepthOfFieldColorChannel segmentThreeProminentColor = DepthOfFieldColorChannel::Blue;

	int numberOfSegments = 3;
	switch(_caType)
	{
		case DepthOfFieldCAType::RGB:
			break;
		case DepthOfFieldCAType::RG:
			numberOfSegments = 2;
			break;
		case DepthOfFieldCAType::RB:
			numberOfSegments = 2;
			segmentTwoProminentColor = DepthOfFieldColorChannel::Blue;
			break;
		case DepthOfFieldCAType::BG:
			numberOfSegments = 2;
			segmentOneProminentColor = DepthOfFieldColorChannel::Blue;
			break;
	}
	const float segmentOneMaxAngle = 1.0f / (float)numberOfSegments;
	const float segmentTwoMaxAngle = 2.0f / (float)numberOfSegments;

	bool redChannelDimmable = true;
	bool greenChannelDimmable = true;
	bool blueChannelDimmable = true;
	float dimFactor = 0.0f;
	if(angleSegment <= segmentOneMaxAngle)
	{
		dimFactor = 1.0f - calculateChannelDimFactor(angleSegment, 0.0f, numberOfSegments);
		redChannelDimmable = segmentOneProminentColor != DepthOfFieldColorChannel::Red;
		greenChannelDimmable = segmentOneProminentColor != DepthOfFieldColorChannel::Green;
		blueChannelDimmable = segmentOneProminentColor != DepthOfFieldColorChannel::Blue;
	}
	else
	{
		if(angleSegment <= segmentTwoMaxAngle)
		{
			dimFactor = 1.0f - calculateChannelDimFactor(angleSegment, segmentOneMaxAngle, numberOfSegments);
			redChannelDimmable = segmentTwoProminentColor != DepthOfFieldColorChannel::Red;
			greenChannelDimmable = segmentTwoProminentColor != DepthOfFieldColorChannel::Green;
			blueChannelDimmable = segmentTwoProminentColor != DepthOfFieldColorChannel::Blue;
		}
		else
		{
			dimFactor = 1.0f - calculateChannelDimFactor(angleSegment, segmentTwoMaxAngle, numberOfSegments);
			redChannelDimmable = segmentThreeProminentColor != DepthOfFieldColorChannel::Red;
			greenChannelDimmable = segmentThreeProminentColor != DepthOfFieldColorChannel::Green;
			blueChannelDimmable = segmentThreeProminentColor != DepthOfFieldColorChannel::Blue;
		}
	}

	const float caRampStart = 1.0f - _caWidth - transitionWidth;
	const float caRampEnd = 1.0f - _caWidth + transitionWidth;
	const float caMask = IGCS::Utils::clampEx((ringRadiusNormalized - caRampStart) / (caRampEnd - caRampStart), 0.0f, 1.0f);
	const float caFactor = _caStrength * caMask;

	redFactor = redChannelDimmable ? IGCS::Utils::lerp(dimFactor, 1.0f, (1.0f - caFactor)) : redFactor;
	greenFactor = greenChannelDimmable ? IGCS::Utils::lerp(dimFactor, 1.0f, (1.0f - caFactor)) : greenFactor;
	blueFactor = blueChannelDimmable ? IGCS::Utils::lerp(dimFactor, 1.0f, (1.0f - caFactor)) : blueFactor;

	sample.sampleWeightRGB[0] *= fringeFactor * redFactor;
	sample.sampleWeightRGB[1] *= fringeFactor * greenFactor;
	sample.sampleWeightRGB[2] *= fringeFactor * blueFactor;
}


void DepthOfFieldController::applyAstigmatism(float& x, float& y)
{
	if(!_astigmatismEnabled || _astigmatismStrength <= FLT_EPSILON)
	{
		return;
	}

	const float angle = IGCS::Utils::degreesToRadians(_astigmatismRotation);
	const float cosAngle = cosf(angle);
	const float sinAngle = sinf(angle);

	const float localX = (x * cosAngle) + (y * sinAngle);
	const float localY = (-x * sinAngle) + (y * cosAngle);
	const float compressedY = localY * (1.0f - _astigmatismStrength);

	x = (localX * cosAngle) - (compressedY * sinAngle);
	y = (localX * sinAngle) + (compressedY * cosAngle);
}


void DepthOfFieldController::applyAstigmatismFocusShift(float x, float y, float focusDeltaHalf, float& xAlignmentDelta, float& yAlignmentDelta)
{
	if(!_astigmatismEnabled || _astigmatismFocusShiftStrength <= FLT_EPSILON || fabsf(focusDeltaHalf) <= FLT_EPSILON)
	{
		return;
	}

	// Split the focus compensation between the two astigmatic axes around the
	// normal focus plane: one axis focuses slightly nearer, the perpendicular
	// axis slightly farther. This keeps the average focus position unchanged.
	const float angle = IGCS::Utils::degreesToRadians(_astigmatismRotation);
	const float cosAngle = cosf(angle);
	const float sinAngle = sinf(angle);
	const float localX = (x * cosAngle) + (y * sinAngle);
	const float localY = (-x * sinAngle) + (y * cosAngle);
	const float halfStrength = 0.5f * _astigmatismFocusShiftStrength;

	const float localShiftX = -localX * halfStrength;
	const float localShiftY = localY * halfStrength;
	const float worldShiftX = (localShiftX * cosAngle) - (localShiftY * sinAngle);
	const float worldShiftY = (localShiftX * sinAngle) + (localShiftY * cosAngle);

	// Keep the same X/Y sign convention used by the existing focus alignment.
	xAlignmentDelta += -worldShiftX * focusDeltaHalf;
	yAlignmentDelta += worldShiftY * focusDeltaHalf;
}


void DepthOfFieldController::createCircleDoFPoints()
{
	_cameraSteps.clear();

	CameraLocation center = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
	applySphericalAberration(0.0f, center);
	applyFringe(0.0f, 0.0f, center);
	_cameraSteps.push_back(center);

	const float pointsFirstRing = (float)_numberOfPointsInnermostRing;
	float pointsOnRing = pointsFirstRing;
	const float maxBokehRadius = _maxBokehSize / 2.0f;
	const float focusDeltaHalf = _focusDelta / 2.0f;
	for(int ringNo = 1; ringNo <= _quality; ringNo++)
	{
		const float anglePerPoint = 6.28318530717958f / pointsOnRing;
		float angle = ((float)ringNo * _ringAngleOffset);
		const float ringDistance = (float)ringNo / (float)_quality;
		for(int pointNumber = 0;pointNumber<pointsOnRing;pointNumber++)
		{
			const float sinAngle = sin(angle);
			const float cosAngle = cos(angle);
			float x = ringDistance * cosAngle * _anamorphicFactor;
			float y = ringDistance * sinAngle;
			applyAstigmatism(x, y);
			const float xDelta = maxBokehRadius * x;
			const float yDelta = maxBokehRadius * y;
			float xAlignmentDelta = x * -focusDeltaHalf;
			float yAlignmentDelta = y * focusDeltaHalf;
			applyAstigmatismFocusShift(x, y, focusDeltaHalf, xAlignmentDelta, yAlignmentDelta);

			CameraLocation sample = {xDelta, yDelta, xAlignmentDelta, yAlignmentDelta, 1.0f, 1.0f, 1.0f};
			applySphericalAberration(ringDistance, sample);
			applyFringe(ringDistance, fmod((angle-(6.28318530717958f / 4.0f)) + 6.28318530717958f, 6.28318530717958f), sample);
			_cameraSteps.push_back(sample);

			angle += anglePerPoint;
			angle = fmod(angle, 6.28318530717958f);
		}

		pointsOnRing += pointsFirstRing;
	}

	renormalizeBokehWeights();
	applyRenderOrder();
}


void DepthOfFieldController::createApertureShapedDoFPoints()
{
	_cameraSteps.clear();

	CameraLocation center = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
	applySphericalAberration(0.0f, center);
	applyFringe(0.0f, 0.0f, center);
	_cameraSteps.push_back(center);

	if(4 == _apertureShapeSettings.NumberOfVertices)
	{
		if(_ringAngleOffset<-0.015f || _ringAngleOffset > 0.015f)
		{
			_ringAngleOffset = 0.0f;
		}
	}

	const float maxBokehRadius = _maxBokehSize / 2.0f;
	const float focusDeltaHalf = _focusDelta / 2.0f;
	const float anglePerVertex = 6.28318530717958f / (float)_apertureShapeSettings.NumberOfVertices;
	for(int ringNo = 1; ringNo <= _quality; ringNo++)
	{
		float vertexAngleForFringe = 0.0f;
		float vertexAngle = fmod((_apertureShapeSettings.RotationAngle * 6.28318530717958f) + ((float)(_quality-ringNo) * _ringAngleOffset), 6.28318530717958f);
		const float ringDistance = (float)ringNo / (float)_quality;
		for(int vertexNo = 0; vertexNo < _apertureShapeSettings.NumberOfVertices; vertexNo++)
		{
			const float sinAngleCurrentVertex = sin(vertexAngle);
			const float cosAngleCurrentVertex = cos(vertexAngle);
			const float nextVertexAngle = fmod(vertexAngle + anglePerVertex, 6.28318530717958f);
			const float sinAngleNextVertex = sin(nextVertexAngle);
			const float cosAngleNextVertex = cos(nextVertexAngle);
			const float xCurrentVertex = ringDistance * cosAngleCurrentVertex;
			const float yCurrentVertex = ringDistance * sinAngleCurrentVertex;
			const float xNextVertex = ringDistance * cosAngleNextVertex;
			const float yNextVertex = ringDistance * sinAngleNextVertex;
			const float pointStepSize = 1.0f / (float)ringNo;
			float pointStep = pointStepSize;
			for(int pointNumber = 0; pointNumber < ringNo; pointNumber++)
			{
				const float pointAngle = IGCS::Utils::lerp(vertexAngle, vertexAngle + anglePerVertex, pointStep);
				const float pointAngleForFringe = IGCS::Utils::lerp(vertexAngleForFringe, vertexAngleForFringe + anglePerVertex, pointStep);
				const float sinPointAngle = sin(pointAngle);
				const float cosPointAngle = cos(pointAngle);
				const float xRoundPoint = ringDistance * cosPointAngle;
				const float yRoundPoint = ringDistance * sinPointAngle;
				const float xLinePoint = IGCS::Utils::lerp(xCurrentVertex, xNextVertex, pointStep);
				const float yLinePoint = IGCS::Utils::lerp(yCurrentVertex, yNextVertex, pointStep);
				float x = IGCS::Utils::lerp(xLinePoint, xRoundPoint, _apertureShapeSettings.RoundFactor);
				float y = IGCS::Utils::lerp(yLinePoint, yRoundPoint, _apertureShapeSettings.RoundFactor);
				const float radiusNormalized = sqrtf(x * x + y * y);
				x *= _anamorphicFactor;
				applyAstigmatism(x, y);
				const float xDelta = maxBokehRadius * x;
				const float yDelta = maxBokehRadius * y;
				float xAlignmentDelta = x * -focusDeltaHalf;
				float yAlignmentDelta = y * focusDeltaHalf;
				applyAstigmatismFocusShift(x, y, focusDeltaHalf, xAlignmentDelta, yAlignmentDelta);
				CameraLocation sample = {xDelta, yDelta, xAlignmentDelta, yAlignmentDelta, 1.0f, 1.0f, 1.0f};
				applySphericalAberration(radiusNormalized, sample);
				applyFringe(ringDistance, pointAngleForFringe, sample);
				_cameraSteps.push_back(sample);
				pointStep += pointStepSize;
			}
			vertexAngle += anglePerVertex;
			vertexAngle = fmod(vertexAngle, 6.28318530717958f);
			vertexAngleForFringe += anglePerVertex;
			vertexAngleForFringe = fmod(vertexAngleForFringe, 6.28318530717958f);
		}
	}

	renormalizeBokehWeights();
	applyRenderOrder();
}


void DepthOfFieldController::renormalizeBokehWeights()
{
	float weightSumRGB[3] = { 0.0f, 0.0f, 0.0f };
	for(const auto& step : _cameraSteps)
	{
		weightSumRGB[0] += step.sampleWeightRGB[0];
		weightSumRGB[1] += step.sampleWeightRGB[1];
		weightSumRGB[2] += step.sampleWeightRGB[2];
	}
	for(auto& step : _cameraSteps)
	{
		step.sampleWeightRGB[0] /= weightSumRGB[0];
		step.sampleWeightRGB[1] /= weightSumRGB[1];
		step.sampleWeightRGB[2] /= weightSumRGB[2];
	}
}


void DepthOfFieldController::applyRenderOrder()
{
	switch(_renderOrder)
	{
		case DepthOfFieldRenderOrder::InnerRingToOuterRing:
			break;
		case DepthOfFieldRenderOrder::OuterRingToInnerRing:
			std::ranges::reverse(_cameraSteps);
			break;
		case DepthOfFieldRenderOrder::Randomized:
			std::ranges::shuffle(_cameraSteps, std::random_device());
			break;
		default:;
	}
}


void DepthOfFieldController::calculateShapePoints()
{
	switch(_blurType)
	{
		case DepthOfFieldBlurType::ApertureShape:
			createApertureShapedDoFPoints();
			break;
		case DepthOfFieldBlurType::Circular:
			createCircleDoFPoints();
			break;
	}
}


void DepthOfFieldController::startRender(reshade::api::effect_runtime* runtime)
{
	if(nullptr == runtime || !_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}

	if(_state!=DepthOfFieldControllerState::Setup)
	{
		return;
	}

	reshade::log::message(reshade::log::level::info, "Dof render session started");

	_blendFrame = false;
	_blendFactor = 0.0f;
	_currentStepFrame = 0;
	_currentBlendFrame = 0;
	_stepCounter = 0;
	switch(_frameWaitType)
	{
		case DepthOfFieldFrameWaitType::Classic:
			_numberOfFramesToWait = std::max(_numberOfFramesToWait, 1);
			_blendCounter = _numberOfFramesToWait;
			break;
		case DepthOfFieldFrameWaitType::Fast:
		{
			_numberOfFramesToWait = std::max(_numberOfFramesToWait, 0);
			int numberOfFramesInFlightToUse = std::max(_numberOfFramesInFlight-1, 0);
			_blendCounter = numberOfFramesInFlightToUse + _numberOfFramesToWait;
		}
			break;
	}
	_numberOfFramesToRender = _cameraSteps.size();
	_renderFrameState = DepthOfFieldRenderFrameState::Start;
	_frameWaitCounter = _numberOfFramesToWait;
	_state = DepthOfFieldControllerState::Rendering;
}


void DepthOfFieldController::migrateReshadeState(reshade::api::effect_runtime* runtime)
{
	if(!_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}
	switch(_state)
	{
		case DepthOfFieldControllerState::Cancelling:
			return;
	}
	if(isReshadeStateEmpty())
	{
		return;
	}

	{
		std::scoped_lock lock(_reshadeStateMutex);
		ReshadeStateSnapshot newState;
		newState.obtainReshadeState(runtime);
		_reshadeStateAtStart = newState;
	}

	if(!isReshadeStateEmpty() && _state == DepthOfFieldControllerState::Setup)
	{
		endSession(runtime);
		startSession(runtime);
	}
}


void DepthOfFieldController::drawShape(ImDrawList* drawList, ImVec2 topLeftScreenCoord, float canvasWidthHeight)
{
	if(_cameraSteps.size()<=0)
	{
		return;
	}

	const float x = canvasWidthHeight / 2.0f + topLeftScreenCoord.x;
	const float y = canvasWidthHeight / 2.0f + topLeftScreenCoord.y;
	const float maxRadius = (canvasWidthHeight / 2.0f)-5.0f;
	float maxBokehRadius = _maxBokehSize / 2.0f;
	maxBokehRadius = maxBokehRadius < FLT_EPSILON ? 1.0f : maxBokehRadius;

	float maxChannel = 0.0f;
	for(const auto& step : _cameraSteps)
	{
		maxChannel = std::max(maxChannel, step.sampleWeightRGB[0]);
		maxChannel = std::max(maxChannel, step.sampleWeightRGB[1]);
		maxChannel = std::max(maxChannel, step.sampleWeightRGB[2]);
	}

	for(const auto& step : _cameraSteps)
	{
		ImColor dotColor = ImColor(step.sampleWeightRGB[0] / maxChannel, step.sampleWeightRGB[1] / maxChannel, step.sampleWeightRGB[2] / maxChannel);
		drawList->AddCircleFilled(ImVec2(x + ((step.xDelta / maxBokehRadius) * maxRadius), y - ((step.yDelta / maxBokehRadius) * maxRadius)), 1.5f, dotColor);
	}
}


void DepthOfFieldController::renderProgressBar()
{
	const int totalAmountOfSteps = _cameraSteps.size();
	const float progress = (float)_currentBlendFrame / (float)totalAmountOfSteps;
	const float progress_saturated = IGCS::Utils::clampEx(progress, 0.0f, 1.0f);
	char buf[128];
	sprintf(buf, "%d/%d", (int)(progress_saturated * totalAmountOfSteps), totalAmountOfSteps);
	ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), buf);
}


void DepthOfFieldController::renderOverlay()
{
	if(_state!=DepthOfFieldControllerState::Rendering || _cameraSteps.size()<=0 || !_showProgressBarAsOverlay)
	{
		return;
	}

	ImGui::SetNextWindowBgAlpha(0.9f);
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	if(ImGui::Begin("IgcsConnector_DoFProgress", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
	{
		renderProgressBar();
	}
	ImGui::End();
}


void DepthOfFieldController::setUniformIntVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, int valueToWrite)
{
	std::scoped_lock lock(_reshadeStateMutex);
	_reshadeStateAtStart.setUniformIntVariable(runtime, "IgcsDof.fx", uniformName, valueToWrite);
}


void DepthOfFieldController::setUniformFloatVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float valueToWrite)
{
	std::scoped_lock lock(_reshadeStateMutex);
	_reshadeStateAtStart.setUniformFloatVariable(runtime, "IgcsDof.fx", uniformName, valueToWrite);
}


void DepthOfFieldController::setUniformBoolVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, bool valueToWrite)
{
	std::scoped_lock lock(_reshadeStateMutex);
	_reshadeStateAtStart.setUniformBoolVariable(runtime, "IgcsDof.fx", uniformName, valueToWrite);
}


void DepthOfFieldController::setUniformFloat2Variable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float value1ToWrite, float value2ToWrite)
{
	std::scoped_lock lock(_reshadeStateMutex);
	_reshadeStateAtStart.setUniformFloat2Variable(runtime, "IgcsDof.fx", uniformName, value1ToWrite, value2ToWrite);
}


void DepthOfFieldController::loadFloatFromIni(CDataFile& iniFile, const std::string& key, float* toWriteTo)
{
	if(nullptr == toWriteTo)
	{
		return;
	}
	const float value = iniFile.GetFloat(key, "DepthOfField");
	if(value != FLT_MIN)
	{
		*toWriteTo = value;
	}
}


void DepthOfFieldController::loadIntFromIni(CDataFile& iniFile, const std::string& key, int* toWriteTo)
{
	if(nullptr == toWriteTo)
	{
		return;
	}
	const int value = iniFile.GetInt(key, "DepthOfField");
	if(value != INT_MIN)
	{
		*toWriteTo = value;
	}
}


void DepthOfFieldController::loadBoolFromIni(CDataFile& iniFile, const std::string& key, bool* toWriteTo, bool defaultValue)
{
	if(nullptr==toWriteTo)
	{
		return;
	}
	const auto boolAsString = iniFile.GetValue(key, "DepthOfField");
	bool valueToUse = defaultValue;
	if(!boolAsString.empty())
	{
		valueToUse = iniFile.GetBool(key, "DepthOfField");
	}
	*toWriteTo = valueToUse;
}
