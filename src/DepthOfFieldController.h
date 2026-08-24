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

#pragma once

#include <functional>
#include <imgui.h>
#include <mutex>

#include "CameraToolsConnector.h"
#include "ConstantsEnums.h"
#include <reshade.hpp>

#include "CDataFile.h"
#include "Utils.h"

#include "ReshadeStateSnapshot.h"

class DepthOfFieldController
{
	struct CameraLocation
	{
		float xDelta = 0.0f;
		float yDelta = 0.0f;
		float xAlignmentDelta = 0.0f;
		float yAlignmentDelta = 0.0f;

		float sampleWeightRGB[3] = {1.0f, 1.0f, 1.0f};
		float tiltPassSign = 1.0f;
	};

	struct MagnifierSettings
	{
		bool ShowMagnifier = false;
		float MagnificationFactor = 2.0f;
		float XMagnifierLocation = 0.5f;
		float YMagnifierLocation = 0.5f;
		float WidthMagnifierArea = 0.19f;
		float HeightMagnifierArea = 0.1f;
	};

	struct ApertureShapeSettings
	{
		int NumberOfVertices = 4;
		float RotationAngle = 0.0f;
		float RoundFactor = 0.25f;
	};

public:
	DepthOfFieldController(CameraToolsConnector& connector);
	~DepthOfFieldController() = default;

	DepthOfFieldControllerState getState() { return _state; }

	void setMaxBokehSize(reshade::api::effect_runtime* runtime, float newValue);
	void setXFocusDelta(reshade::api::effect_runtime* runtime, float newValueX);
	void startSession(reshade::api::effect_runtime* runtime);
	void endSession(reshade::api::effect_runtime* runtime);
	void reshadeBeginEffectsCalled(reshade::api::effect_runtime* runtime);
	void reshadeFinishEffectsCalled(reshade::api::effect_runtime* runtime);
	void startRender(reshade::api::effect_runtime* runtime);
	void migrateReshadeState(reshade::api::effect_runtime* runtime);
	void calculateShapePoints();
	void renderOverlay();
	void drawShape(ImDrawList* drawList, ImVec2 topLeftScreenCoord, float canvasWidthHeight);
	void drawAstigmatismPreview(ImDrawList* drawList, ImVec2 topLeftScreenCoord, float width, float height);
	void renderProgressBar();
	void writeVariableStateToShader(reshade::api::effect_runtime* runtime);
	void loadIniFileData(CDataFile& iniFile);
	void saveIniFileData(CDataFile& iniFile);
	void invalidateShapePoints() { calculateShapePoints(); }

	void setNumberOfFramesToWaitPerFrame(int newValue) { _numberOfFramesToWait = IGCS::Utils::clampEx(newValue, 0, 20); }
	void setNumberOfFramesInFlight(int newValue) { _numberOfFramesInFlight = IGCS::Utils::clampEx(newValue, 0, 20); }
	void setFrameWaitType(DepthOfFieldFrameWaitType newValue)
	{
		_frameWaitType = newValue;
		switch(_frameWaitType)
		{
			case DepthOfFieldFrameWaitType::Fast:
				_numberOfFramesInFlight = _numberOfFramesToWait;
				_numberOfFramesToWait = 0;
				break;
			case DepthOfFieldFrameWaitType::Classic:
				_numberOfFramesToWait = std::max(1, _numberOfFramesToWait);
				break;
		}
	}
	void setCatEyeRadiusStart(float newValue) { _catEyeRadiusStart = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
	void setCatEyeRadiusEnd(float newValue) { _catEyeRadiusEnd = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
	void setCatEyeBokehIntensity(float newValue) { _catEyeBokehIntensity = IGCS::Utils::clampEx(newValue, -1.0f, 1.0f); }
	void setAddCatEyeVignette(bool newValue) { _addCatEyeVignette = newValue; }

	void setQuality(int newValue)
	{
		_quality = IGCS::Utils::clampEx(newValue, 1, 100);
		calculateShapePoints();
	}
	void setNumberOfPointsInnermostRing(int newValue)
	{
		_numberOfPointsInnermostRing = IGCS::Utils::clampEx(newValue, 1, 100);
		calculateShapePoints();
	}
	void setBlurType(DepthOfFieldBlurType newValue)
	{
		_blurType = newValue;
		calculateShapePoints();
	}
	void setAnamorphicFactor(float newValue)
	{
		_anamorphicFactor = IGCS::Utils::clampEx(newValue, 0.01f, 1.0f);
		calculateShapePoints();
	}
	void setAstigmatismEnabled(bool newValue)
	{
		_astigmatismEnabled = newValue;
		calculateShapePoints();
	}
	void setAstigmatismStrength(float newValue)
	{
		_astigmatismStrength = IGCS::Utils::clampEx(newValue, 0.0f, 2.0f);
		calculateShapePoints();
	}
	void setAstigmatismRotation(float newValue)
	{
		_astigmatismRotation = IGCS::Utils::clampEx(newValue, 0.0f, 180.0f);
		calculateShapePoints();
	}
	void setTiltEnabled(bool newValue) { _tiltEnabled = newValue; }
	void setTiltMode(int newValue) { _tiltMode = IGCS::Utils::clampEx(newValue, 0, 2); }
	void setTiltHorizontal(float newValue) { _tiltHorizontal = IGCS::Utils::clampEx(newValue, -45.0f, 45.0f); }
	void setTiltVertical(float newValue) { _tiltVertical = IGCS::Utils::clampEx(newValue, -45.0f, 45.0f); }
	void setTiltCrossSaddle(float newValue) { _tiltCrossSaddle = IGCS::Utils::clampEx(newValue, -45.0f, 45.0f); }
	void setTiltCrossSaddleRotation(float newValue) { _tiltCrossSaddleRotation = IGCS::Utils::clampEx(newValue, 0.0f, 180.0f); }
	void setTiltCornerTL(float newValue) { _tiltCornerTL = IGCS::Utils::clampEx(newValue, -45.0f, 45.0f); }
	void setTiltCornerTR(float newValue) { _tiltCornerTR = IGCS::Utils::clampEx(newValue, -45.0f, 45.0f); }
	void setTiltCornerBL(float newValue) { _tiltCornerBL = IGCS::Utils::clampEx(newValue, -45.0f, 45.0f); }
	void setTiltCornerBR(float newValue) { _tiltCornerBR = IGCS::Utils::clampEx(newValue, -45.0f, 45.0f); }
	void setTiltPivotX(float newValue) { _tiltPivotX = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
	void setTiltPivotY(float newValue) { _tiltPivotY = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
	void setTiltShowOverlay(bool newValue) { _tiltShowOverlay = newValue; }
	void setTiltTwoPass(bool newValue)
	{
		_tiltTwoPass = newValue;
		calculateShapePoints();
	}
	void setDistortionEnabled(bool newValue) { _distortionEnabled = newValue; }
	void setDistortionShowGuide(bool newValue) { _distortionShowGuide = newValue; }
	void setDistortionAutoFill(bool newValue) { _distortionAutoFill = newValue; }
	void setDistortionFillCrop(float newValue) { _distortionFillCrop = IGCS::Utils::clampEx(newValue, 1.0f, 2.0f); }
	void setDistortionStrength(float newValue) { _distortionStrength = IGCS::Utils::clampEx(newValue, -0.75f, 0.75f); }
	void setDistortionCurve(float newValue) { _distortionCurve = IGCS::Utils::clampEx(newValue, -0.75f, 0.75f); }
	void setDistortionCenterX(float newValue) { _distortionCenterX = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
	void setDistortionCenterY(float newValue) { _distortionCenterY = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
	void setDistortionStartRadius(float newValue) { _distortionStartRadius = IGCS::Utils::clampEx(newValue, 0.0f, 0.999f); }
	void setDistortionEndRadius(float newValue) { _distortionEndRadius = IGCS::Utils::clampEx(newValue, 0.001f, 1.0f); }
	void setPetzvalEnabled(bool newValue) { _petzvalEnabled = newValue; }
	void setPetzvalTangential(float newValue) { _petzvalTangential = IGCS::Utils::clampEx(newValue, -3.0f, 3.0f); }
	void setPetzvalSagittal(float newValue) { _petzvalSagittal = IGCS::Utils::clampEx(newValue, -3.0f, 3.0f); }
	void setPetzvalStrength(float newValue) { _petzvalStrength = IGCS::Utils::clampEx(newValue, 0.0f, 2.0f); }
	void setPetzvalCenterX(float newValue) { _petzvalCenterX = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
	void setPetzvalCenterY(float newValue) { _petzvalCenterY = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
	void setPetzvalShowGuide(bool newValue) { _petzvalShowGuide = newValue; }
	void setVignettingEnabled(bool newValue) { _vignettingEnabled = newValue; }
	void setVignettingStart(float newValue)
	{
		_vignettingStart = IGCS::Utils::clampEx(newValue, 0.0f, 0.999f);
		if(_vignettingEnd <= _vignettingStart)
		{
			_vignettingEnd = std::min(1.0f, _vignettingStart + 0.001f);
		}
	}
	void setVignettingEnd(float newValue)
	{
		_vignettingEnd = IGCS::Utils::clampEx(newValue, 0.001f, 1.0f);
		if(_vignettingEnd <= _vignettingStart)
		{
			_vignettingStart = std::max(0.0f, _vignettingEnd - 0.001f);
		}
	}
	void setVignettingStrength(float newValue) { _vignettingStrength = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
void setVignettingCenterX(float newValue) { _vignettingCenterX = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
void setVignettingCenterY(float newValue) { _vignettingCenterY = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
void setVignettingShowGuide(bool newValue) { _vignettingShowGuide = newValue; }
	void setRingAngleOffset(float newValue)
	{
		_ringAngleOffset = IGCS::Utils::clampEx(newValue, -2.0f, 2.0f);
		calculateShapePoints();
	}
	void setSphericalAberrationDimFactor(float newValue)
	{
		_sphericalAberrationDimFactor = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f);
		calculateShapePoints();
	}
	void setFringeIntensity(float newValue)
	{
		_fringeIntensity = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f);
		calculateShapePoints();
	}
	void setFringeWidth(float newValue)
	{
		_fringeWidth = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f);
		calculateShapePoints();
	}
	void setCAStrength(float newValue)
	{
		_caStrength = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f);
		calculateShapePoints();
	}
	void setCAWidth(float newValue)
	{
		_caWidth = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f);
		calculateShapePoints();
	}
	void setRenderOrder(DepthOfFieldRenderOrder newValue)
	{
		_renderOrder = newValue;
		calculateShapePoints();
	}
	void setCAType(DepthOfFieldCAType newValue)
	{
		_caType = newValue;
		calculateShapePoints();
	}
	void setHighlightBoostFactor(float newValue) { _highlightBoostFactor = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
	void setHighlightGammaFactor(float newValue) { _highlightGammaFactor = IGCS::Utils::clampEx(newValue, 0.1f, 5.0f); }
	void setRenderPaused(bool newValue) { _renderPaused = newValue; }
	void setShowProgressBarAsOverlay(bool newValue) { _showProgressBarAsOverlay = newValue; }

	DepthOfFieldRenderOrder getRenderOrder() { return _renderOrder; }
	float getMaxBokehSize() { return _maxBokehSize; }
	float getXFocusDelta() { return _focusDelta; }
	int getQuality() { return _quality; }
	float getHighlightBoostFactor() { return _highlightBoostFactor; }
	float getHighlightGammaFactor() { return _highlightGammaFactor; }
	DepthOfFieldBlurType getBlurType() { return _blurType; }
	int getNumberOfPointsInnermostRing() { return _numberOfPointsInnermostRing; }
	int getNumberOfFramesToWaitPerFrame() { return _numberOfFramesToWait; }
	int getNumberOfFramesInFlight() { return _numberOfFramesInFlight; }
	bool getRenderPaused() { return _renderPaused; }
	int getTotalNumberOfStepsToTake() { return _cameraSteps.size(); }
	bool getShowProgressBarAsOverlay() { return _showProgressBarAsOverlay; }
	float getAnamorphicFactor() { return _anamorphicFactor; }
	bool getAstigmatismEnabled() { return _astigmatismEnabled; }
	float getAstigmatismStrength() { return _astigmatismStrength; }
	float getAstigmatismRotation() { return _astigmatismRotation; }
	bool getTiltEnabled() { return _tiltEnabled; }
	int getTiltMode() { return _tiltMode; }
	float getTiltHorizontal() { return _tiltHorizontal; }
	float getTiltVertical() { return _tiltVertical; }
	float getTiltCrossSaddle() { return _tiltCrossSaddle; }
	float getTiltCrossSaddleRotation() { return _tiltCrossSaddleRotation; }
	float getTiltCornerTL() { return _tiltCornerTL; }
	float getTiltCornerTR() { return _tiltCornerTR; }
	float getTiltCornerBL() { return _tiltCornerBL; }
	float getTiltCornerBR() { return _tiltCornerBR; }
	float getTiltPivotX() { return _tiltPivotX; }
	float getTiltPivotY() { return _tiltPivotY; }
	bool getTiltShowOverlay() { return _tiltShowOverlay; }
	bool getTiltTwoPass() { return _tiltTwoPass; }
	bool getDistortionEnabled() { return _distortionEnabled; }
	bool getDistortionShowGuide() { return _distortionShowGuide; }
	bool getDistortionAutoFill() { return _distortionAutoFill; }
	float getDistortionFillCrop() { return _distortionFillCrop; }
	float getDistortionStrength() { return _distortionStrength; }
	float getDistortionCurve() { return _distortionCurve; }
	float getDistortionCenterX() { return _distortionCenterX; }
	float getDistortionCenterY() { return _distortionCenterY; }
	float getDistortionStartRadius() { return _distortionStartRadius; }
	float getDistortionEndRadius() { return _distortionEndRadius; }
	bool getPetzvalEnabled() { return _petzvalEnabled; }
	float getPetzvalTangential() { return _petzvalTangential; }
	float getPetzvalSagittal() { return _petzvalSagittal; }
	float getPetzvalStrength() { return _petzvalStrength; }
	float getPetzvalCenterX() { return _petzvalCenterX; }
	float getPetzvalCenterY() { return _petzvalCenterY; }
	bool getPetzvalShowGuide() { return _petzvalShowGuide; }
	bool getVignettingEnabled() { return _vignettingEnabled; }
	float getVignettingStart() { return _vignettingStart; }
	float getVignettingEnd() { return _vignettingEnd; }
	float getVignettingStrength() { return _vignettingStrength; }
	float getVignettingCenterX() { return _vignettingCenterX; }
	float getVignettingCenterY() { return _vignettingCenterY; }
	bool getVignettingShowGuide() { return _vignettingShowGuide; }
	float getRingAngleOffset() { return _ringAngleOffset; }
	float getSphericalAberrationDimFactor() { return _sphericalAberrationDimFactor; }
	float getFringeIntensity() { return _fringeIntensity; }
	float getFringeWidth() { return _fringeWidth; }
	float getCAStrength() { return _caStrength; }
	float getCAWidth() { return _caWidth; }
	DepthOfFieldCAType getCAType() { return _caType; }
	DepthOfFieldFrameWaitType getFrameWaitType() { return _frameWaitType; }
	float getCatEyeRadiusStart() { return _catEyeRadiusStart; }
	float getCatEyeRadiusEnd() { return _catEyeRadiusEnd; }
	float getCatEyeBokehIntensity() { return _catEyeBokehIntensity; }
	bool getAddCatEyeVignette() { return _addCatEyeVignette; }

	MagnifierSettings& getMagnifierSettings() { return _magnificationSettings; }
	ApertureShapeSettings& getApertureShapeSettings() { return _apertureShapeSettings; }

	void setDebugBool1(bool newVal) { _debugBool1 = newVal; }
	void setDebugBool2(bool newVal) { _debugBool2 = newVal; }
	void setDebugVal1(float newVal) { _debugVal1 = newVal; }
	void setDebugVal2(float newVal) { _debugVal2 = newVal; }
	bool getDebugBool1() { return _debugBool1; }
	bool getDebugBool2() { return _debugBool2; }
	float getDebugVal1() { return _debugVal1; }
	float getDebugVal2() { return _debugVal2; }

private:
	void setUniformIntVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, int valueToWrite);
	void setUniformFloatVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float valueToWrite);
	void setUniformBoolVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, bool valueToWrite);
	void setUniformFloat2Variable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float value1ToWrite, float value2ToWrite);
	void loadFloatFromIni(CDataFile& iniFile, const std::string& key, float* toWriteTo);
	void loadIntFromIni(CDataFile& iniFile, const std::string& key, int* toWriteTo);
	void loadBoolFromIni(CDataFile& iniFile, const std::string& key, bool* toWriteTo, bool defaultValue);
	void createCircleDoFPoints();
	void applyRenderOrder();
	void renormalizeBokehWeights();
	void createApertureShapedDoFPoints();
	void applyAstigmatismFocusPlane(float x, float y, float focusDeltaHalf, float& xAlignmentDelta, float& yAlignmentDelta);

	void displayScreenshotSessionStartError(const ScreenshotSessionStartReturnCode sessionStartResult);
	void handlePresentBeforeReshadeEffects();
	void handlePresentAfterReshadeEffects();
	void applySphericalAberration(float radiusNormalized, CameraLocation& sample);
	void applyFringe(float ringRadiusNormalized, float sampleAngle, CameraLocation& sample);
	void performRenderFrameSetupWork();
	float calculateChannelDimFactor(float angleSegment, float segmentAngleMin, int numberOfSegments);

	bool isReshadeStateEmpty()
	{
		std::scoped_lock lock(_reshadeStateMutex);
		return _reshadeStateAtStart.isEmpty();
	}

	CameraToolsConnector& _cameraToolsConnector;
	DepthOfFieldControllerState _state;
	std::vector<CameraLocation> _cameraSteps;

	std::function<void(reshade::api::effect_runtime*)> _onPresentWorkFunc = nullptr;

	float _maxBokehSize = 0.25;
	float _focusDelta = 0.0f;
	bool _blendFrame = false;
	float _blendFactor = 0.0f;
	float _xAlignmentDelta = 0.0f;
	float _yAlignmentDelta = 0.0f;
	float _tiltPassSign = 1.0f;
	float _highlightBoostFactor = 0.9f;
	float _highlightGammaFactor = 2.2f;
	float _sphericalAberrationDimFactor = 0.5f;
	float _fringeIntensity = 0.0f;
	float _fringeWidth = 0.1f;
	float _caStrength = 0.0f;
	float _caWidth = 0.1f;
	float _sampleWeightRGB[3] = {1.0f, 1.0f, 1.0f};
	DepthOfFieldCAType _caType = DepthOfFieldCAType::RGB;
	float _catEyeRadiusStart = 0.2f;
	float _catEyeRadiusEnd = 0.7f;
	float _catEyeBokehIntensity = 0.0f;
	bool _addCatEyeVignette = false;

	MagnifierSettings _magnificationSettings;

	int _onPresentWorkCounter = 0;

	DepthOfFieldBlurType _blurType = DepthOfFieldBlurType::Circular;
	DepthOfFieldRenderFrameState _renderFrameState = DepthOfFieldRenderFrameState::Off;
	int _frameWaitCounter = 0;
	int _currentStepFrame = -1;
	int _currentBlendFrame = -1;
	int _stepCounter = 0;
	int _blendCounter = 0;
	bool _renderPaused = false;

	int _numberOfFramesToRender = 0;
	int _numberOfFramesToWait = 0;
	int _numberOfFramesInFlight = 1;
	int _quality;
	int _numberOfPointsInnermostRing;
	float _ringAngleOffset = 0.0f;
	float _anamorphicFactor = 1.0f;
	bool _astigmatismEnabled = false;
	float _astigmatismStrength = 0.0f;
	float _astigmatismRotation = 0.0f;
	bool _tiltEnabled = false;
	int _tiltMode = 0;
	float _tiltHorizontal = 0.0f;
	float _tiltVertical = 0.0f;
	float _tiltCrossSaddle = 0.0f;
	float _tiltCrossSaddleRotation = 0.0f;
	float _tiltCornerTL = 0.0f;
	float _tiltCornerTR = 0.0f;
	float _tiltCornerBL = 0.0f;
	float _tiltCornerBR = 0.0f;
	float _tiltPivotX = 0.5f;
	float _tiltPivotY = 0.5f;
	bool _tiltShowOverlay = true;
	bool _tiltTwoPass = false;
	bool _distortionEnabled = false;
	bool _distortionShowGuide = false;
	bool _distortionAutoFill = true;
	float _distortionFillCrop = 1.0f;
	float _distortionStrength = 0.0f;
	float _distortionCurve = 0.0f;
	float _distortionCenterX = 0.5f;
	float _distortionCenterY = 0.5f;
	float _distortionStartRadius = 0.0f;
	float _distortionEndRadius = 1.0f;
	bool _petzvalEnabled = false;
	float _petzvalTangential = 0.0f;
	float _petzvalSagittal = 0.0f;
	float _petzvalStrength = 1.0f;
	float _petzvalCenterX = 0.5f;
	float _petzvalCenterY = 0.5f;
	bool _petzvalShowGuide = false;
	bool _vignettingEnabled = false;
	float _vignettingStart = 0.65f;
	float _vignettingEnd = 1.0f;
	float _vignettingStrength = 0.5f;
	float _vignettingCenterX = 0.5f;
	float _vignettingCenterY = 0.5f;
	bool _vignettingShowGuide = false;
	DepthOfFieldRenderOrder _renderOrder = DepthOfFieldRenderOrder::InnerRingToOuterRing;
	bool _showProgressBarAsOverlay = true;
	ApertureShapeSettings _apertureShapeSettings;
	DepthOfFieldFrameWaitType _frameWaitType = DepthOfFieldFrameWaitType::Fast;

	ReshadeStateSnapshot _reshadeStateAtStart;
	std::mutex _reshadeStateMutex;

	float _debugVal1 = 0.0f;
	float _debugVal2 = 0.0f;
	bool _debugBool1 = false;
	bool _debugBool2 = false;
};