#pragma once

#include "Game/Widget.hpp"

#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/FloatRange.hpp"


#include <string>

enum class SliderType {
	INVALID = -1,
	MUSIC,
	SFX,
	IP_ADDR_1,//0-255
	IP_ADDR_2,
	IP_ADDR_3,
	IP_ADDR_4,
	PORT_NUMBER,//1024-65535, or 1024-4000
	LOADING_PROGRESS,
	COUNT
};


std::string GetSliderNameStrByType(SliderType type);

struct Slider : public Widget
{
public:
	Slider();
	Slider(AABB2 const& widgetWindow, SliderType type, float initPercent, FloatRange const& range = FloatRange(0.f,1.f), bool clampedToInt = false);
	~Slider();

	void Startup()override;
	void Update(float deltaSeconds)override;
	void Render()const override;

	void SetSliderVal(float newVal);
	void UpdateLocalVerts()override;
	float DragByOffset(Vec2 const& offset)override;
	void SetBGColor(Rgba8 const& color)override;
	void SetTextColor(Rgba8 const& color)override;

	//get
	bool IsInsideWidget(Vec2 const& pos)const override;
	bool IsBeingClickedOn(Vec2 const& pos)const override;
	int GetUniqueWidgetType()const override;

public:
	SliderType m_sliderType = SliderType::INVALID;
	float m_sliderPercent = 1;
	FloatRange m_sliderRange = FloatRange::ZERO_TO_ONE;

	//name text
	std::string m_nameStr;
	AABB2 m_nameTextBox;
	Rgba8 m_nameTextColor = Rgba8::BLACK;

	//border
	float m_sliderBorderThickness = 2.f;
	Rgba8 m_sliderBorderColor = Rgba8::BLACK;
	AABB2 m_sliderBorderBox;


	//background
	AABB2 m_sliderBGBox;
	Rgba8 m_sliderBGColor = Rgba8::DARK_GRAY;

	//foreground
	AABB2 m_sliderFGBox;
	Rgba8 m_sliderFGColor = Rgba8::LIGHT_GRAY;
	float m_percentMaxXDim = 0;

	//sliderPin
	Vec2 m_sliderPinPos;
	float m_sliderPinRadius = 0;
	Rgba8 m_sliderPinColor = Rgba8::WHITE;

	bool m_clampedToInt = false;
};