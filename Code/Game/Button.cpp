#include "Game/Button.hpp"
#include "Game/GameCommon.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"

Button::Button()
:Widget(){
}

Button::Button(AABB2 const& widgetWindow, Rgba8 const& color, std::string const& text, Rgba8 const& textColor, ButtonType type)
:Widget(widgetWindow, WidgetType::BUTTON),m_buttonColor(color), m_buttonText(text),m_buttonTextColor(textColor),m_buttonType(type){
	m_isClickable = true;
	m_isHoverable = true;


}

Button::~Button()
{
	
}

void Button::Startup()
{
	InitLocalVerts();
	CreateBuffers();
}

void Button::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);
}

void Button::Render() const
{
	g_theRenderer->SetModelConstants(Mat44(), m_buttonColor);
	//background
	g_theRenderer->BindTexture(nullptr, 0);
	g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, m_numIndices);

	g_theRenderer->SetModelConstants(Mat44(), m_buttonTextColor);
	//text
	g_theRenderer->BindTexture(&g_theFont->GetTexture(), 0);
	g_theRenderer->DrawVertexArray(m_vertices);
}

void Button::InitLocalVerts()
{
	Vec2 textPivot = m_widgetWindow.GetCenter();
	
	AABB2 splitBox(m_widgetWindow);
	int textLength = (int)m_buttonText.length();
	if (textLength <= 10)
	{
		g_theFont->AddVertsForTextInBox2D(m_vertices, m_buttonText, m_widgetWindow, TEXT_HEIGHT, m_buttonTextColor, 0.75f, Vec2(0.5, 0.5));

	}
	else {
		//Strings splitStr = SplitStringEveryNCharacters(m_nameStr, 10);
		Strings splitStr = SplitStringOnDelimiter(m_buttonText, ' ', false);
		int numSplitStr = (int)splitStr.size();
		splitBox.Translate(Vec2(0, TEXT_HEIGHT * numSplitStr * 0.33f));
		for (int i = 0; i < numSplitStr; i++)
		{
			g_theFont->AddVertsForTextInBox2D(m_vertices, splitStr[i], splitBox, TEXT_HEIGHT, m_buttonTextColor, 0.75f, Vec2(0.5, 0.5));
			splitBox.Translate(Vec2(0, -TEXT_HEIGHT));
		}
	}

}

void Button::CreateBuffers()
{
	unsigned int singleVertSize = sizeof(Vertex_PCU);
	unsigned int uintSize = sizeof(unsigned int);

	unsigned int numVertexes = 4;
	unsigned int numIndices = 6;
	m_numIndices = numIndices;

	unsigned int totalVertSize = numVertexes * singleVertSize;
	unsigned int indexSize = numIndices * uintSize;

	std::vector<Vertex_PCU> tempVerts;
	std::vector<unsigned int> tempIndices;

	AddVertsForAABB2D(tempVerts, tempIndices,m_widgetWindow, m_buttonColor,Vec2::ZERO,Vec2::ONE);

	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(totalVertSize, singleVertSize);
	m_indexBuffer = g_theRenderer->CreateIndexBuffer(indexSize, uintSize);

	g_theRenderer->CopyCPUToGPU(tempVerts.data(), totalVertSize, m_vertexBuffer);
	g_theRenderer->CopyCPUToGPU(tempIndices.data(), indexSize, m_indexBuffer);


}

void Button::SetBGColor(Rgba8 const& color)
{
	m_buttonColor = color;
}

int Button::GetUniqueWidgetType() const
{
	return (int)m_buttonType;
}
