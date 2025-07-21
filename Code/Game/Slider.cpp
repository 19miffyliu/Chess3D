#include "Game/Slider.hpp"
#include "Game/GameCommon.hpp"
#include "Game/App.hpp"
#include "Game/Game.hpp"

#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Core/StringUtils.hpp"


Slider::Slider()
{
}

Slider::Slider(AABB2 const& widgetWindow,SliderType type, float initPercent, FloatRange const& range, bool clampedToInt)
:Widget(widgetWindow, WidgetType::SLIDER),m_sliderType(type), m_sliderRange(range), m_clampedToInt(clampedToInt){

	m_nameStr = GetSliderNameStrByType(type);
	

	m_isClickable = true;
	m_isHoverable = true;
	m_isDraggable = true;

	//set default vals for slider boxes
	Vec2 dimension = widgetWindow.GetDimensions();

	//background
	m_sliderBGBox = widgetWindow;
	m_sliderBGBox.m_mins.x = m_sliderBGBox.m_maxs.x - (dimension.x * 0.6f);

	//name text
	m_nameTextBox = widgetWindow;
	m_nameTextBox.m_maxs.x = m_sliderBGBox.m_mins.x;

	//foreground
	Vec2 boxDimension = m_sliderBGBox.GetDimensions();
	m_sliderFGBox = m_sliderBGBox;
	Vec2 FGDimension(boxDimension.x - 30.f, boxDimension.y - 30.f);
	m_sliderFGBox.SetDimensions(FGDimension);
	m_percentMaxXDim = FGDimension.x;
	m_sliderBorderBox = m_sliderFGBox;

	//sliderPin
	Vec2 center = m_sliderFGBox.GetCenter();
	Vec2 halfDim = boxDimension * 0.5;
	m_sliderPinPos = Vec2(center.x + halfDim.x,center.y);
	m_sliderPinRadius = halfDim.y * 0.8f;

	SetSliderVal(initPercent);

	
}

Slider::~Slider()
{
}

void Slider::Startup()
{

}

void Slider::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);

	
	

}

void Slider::Render() const
{
	g_theRenderer->BindTexture(nullptr, 0);
	g_theRenderer->SetModelConstants();
	g_theRenderer->DrawVertexArray(m_vertices);

	g_theRenderer->BindTexture(&g_theFont->GetTexture(), 0);
	std::vector<Vertex_PCU> tempVerts;

	AABB2 splitBox(m_nameTextBox);
	int textLength = (int)m_nameStr.length();


	if (m_sliderType == SliderType::LOADING_PROGRESS)
	{
		int numAssets = g_numAssetsLoaded;
		int totalAssets = (int)roundf(Game::s_totalAssetNum);
		std::string progressDetail = Stringf("%s %d/%d", m_nameStr.c_str(), numAssets, totalAssets);
		g_theFont->AddVertsForTextInBox2D(tempVerts, progressDetail, m_nameTextBox, TEXT_HEIGHT, m_nameTextColor, 0.75f, Vec2(0.5, 0.5));
	}
	else if (textLength <= 10)
	{
		g_theFont->AddVertsForTextInBox2D(tempVerts, m_nameStr, m_nameTextBox, TEXT_HEIGHT, m_nameTextColor, 0.75f, Vec2(0.5, 0.5));
	}
	else {
		//Strings splitStr = SplitStringEveryNCharacters(m_nameStr, 10);
		Strings splitStr = SplitStringOnDelimiter(m_nameStr, ' ', false);
		int numSplitStr = (int)splitStr.size();
		splitBox.Translate(Vec2(0, TEXT_HEIGHT * numSplitStr * 0.33f));
		for (int i = 0; i < numSplitStr; i++)
		{
			g_theFont->AddVertsForTextInBox2D(tempVerts, splitStr[i], splitBox, TEXT_HEIGHT, m_nameTextColor, 0.75f, Vec2(0.5, 0.5));
			splitBox.Translate(Vec2(0, -TEXT_HEIGHT));
		}



	}

	float valRangemapped = RangeMapClamped(m_sliderPercent, 0.f, 1.f, m_sliderRange.m_min, m_sliderRange.m_max);
	std::string val = m_clampedToInt? Stringf("%.0f", valRangemapped) : Stringf("%.01f", valRangemapped);
	AABB2 valBox(m_nameTextBox);
	valBox.SetDimensions(Vec2(m_sliderPinRadius * 2, m_sliderPinRadius * 2));
	valBox.SetCenter(m_sliderPinPos);
	g_theFont->AddVertsForTextInBox2D(tempVerts, val, valBox, TEXT_HEIGHT, Rgba8::BLACK, 0.75f, Vec2(0.5, 0.5));
	g_theRenderer->DrawVertexArray(tempVerts);

}

void Slider::SetSliderVal(float newPercent)
{
	m_sliderPercent = newPercent;

	//update foreground box and slider pin x position
	m_sliderFGBox.m_maxs.x = m_sliderFGBox.m_mins.x + m_percentMaxXDim * newPercent;
	m_sliderPinPos.x = m_sliderFGBox.m_maxs.x;

	UpdateLocalVerts();
}

void Slider::UpdateLocalVerts()
{
	m_vertices.clear();

	//background
	AddVertsForAABB2D(m_vertices, m_sliderBGBox, m_sliderBGColor, Vec2::ZERO, Vec2::ONE);

	//border
	//AddVertsForOutlineBox2D(m_vertices, m_sliderBorderBox, m_sliderBorderThickness, m_sliderBorderColor);
	AddVertsForAABB2D(m_vertices, m_sliderBorderBox, m_sliderBorderColor, Vec2::ZERO, Vec2::ONE);

	//draw FG box with outline
	AddVertsForAABB2D(m_vertices, m_sliderFGBox, m_sliderFGColor, Vec2::ZERO, Vec2::ONE);

	
	//pin
	AddVertsForDisc2D(m_vertices, m_sliderPinPos, m_sliderPinRadius, m_sliderPinColor);
}

float Slider::DragByOffset(Vec2 const& offset)
{
	float newPercent = GetClamped(m_sliderPercent + (offset.x / m_percentMaxXDim), 0.f, 1.f);
	SetSliderVal(newPercent);
	return newPercent;
}

void Slider::SetBGColor(Rgba8 const& color)
{
	m_sliderPinColor = color;
	UpdateLocalVerts();
}

void Slider::SetTextColor(Rgba8 const& color)
{
	m_nameTextColor = color;
}


bool Slider::IsInsideWidget(Vec2 const& pos) const
{
	return IsPointInsideDisc2D(pos, m_sliderPinPos, m_sliderPinRadius);
}

bool Slider::IsBeingClickedOn(Vec2 const& pos) const
{
	return m_isClickable && IsInsideWidget(pos);
}

int Slider::GetUniqueWidgetType() const
{
	return (int)m_sliderType;
}

std::string GetSliderNameStrByType(SliderType type)
{
	switch (type)
	{
	case SliderType::MUSIC:
		return "Music";
	case SliderType::SFX:
		return "SFX";
	case SliderType::IP_ADDR_1 :
		return "IP 1";
	case SliderType::IP_ADDR_2:
		return "IP 2";
	case SliderType::IP_ADDR_3:
		return "IP 3";
	case SliderType::IP_ADDR_4:
		return "IP 4";
	case SliderType::PORT_NUMBER:
		return "Port Number";
	case SliderType::LOADING_PROGRESS:
		return "Loading Progress";
	case SliderType::COUNT:
	case SliderType::INVALID:
	default:
		ERROR_AND_DIE("invalid slider type");
		break;
	}

}
