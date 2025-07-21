#include "Game/Widget.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"

Widget::Widget()
{
}

Widget::Widget(AABB2 const& widgetWindow)
:m_widgetWindow(widgetWindow){
}

Widget::Widget(AABB2 const& widgetWindow, WidgetType type)
	:m_widgetWindow(widgetWindow),m_widgetType(type){
}

Widget::~Widget()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;
}

void Widget::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);
}

void Widget::Render() const
{
}

void Widget::Startup()
{
}

void Widget::InitLocalVerts()
{
}

void Widget::CreateBuffers()
{
}

float Widget::DragByOffset(Vec2 const& offset)
{
	m_widgetWindow.Translate(offset);

	float maxLength = m_widgetWindow.GetDimensions().x;
	return (offset.x) / maxLength;
}

void Widget::UpdateLocalVerts()
{
}

void Widget::SetBGColor(Rgba8 const& color)
{
	UNUSED(color);
}

void Widget::SetTextColor(Rgba8 const& color)
{
	UNUSED(color);
}

bool Widget::IsInsideWidget(Vec2 const& pos) const
{
	return m_widgetWindow.IsPointInside(pos);
}

bool Widget::IsBeingClickedOn(Vec2 const& pos) const
{
	return m_isClickable && IsInsideWidget(pos);
}

bool Widget::IsBeingHovered(Vec2 const& pos) const
{
	return m_isHoverable && IsInsideWidget(pos);
}

int Widget::GetUniqueWidgetType() const
{
	return -1;
}
