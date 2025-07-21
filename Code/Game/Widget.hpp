#pragma once


#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

#include <vector>

class IndexBuffer;
class VertexBuffer;

enum class WidgetType {
	INVALID = -1,
	BUTTON,
	SLIDER,
	COUNT
};

struct Widget
{
public:
	Widget();
	Widget(AABB2 const& widgetWindow);
	Widget(AABB2 const& widgetWindow, WidgetType type);
	~Widget();

	virtual void Update(float deltaSeconds);
	virtual void Render()const;

	virtual void Startup();


	//can choose to either create buffers or initLocalVerts
	virtual void InitLocalVerts();
	virtual void CreateBuffers();

	//set
	virtual float DragByOffset(Vec2 const& offset);
	virtual void UpdateLocalVerts();
	virtual void SetBGColor(Rgba8 const& color);
	virtual void SetTextColor(Rgba8 const& color);

	//get
	virtual bool IsInsideWidget(Vec2 const& pos)const;
	virtual bool IsBeingClickedOn(Vec2 const& pos)const;
	virtual bool IsBeingHovered(Vec2 const& pos)const;
	virtual int GetUniqueWidgetType()const;


public:
	WidgetType m_widgetType = WidgetType::INVALID;
	AABB2 m_widgetWindow;

	//render
	std::vector<Vertex_PCU> m_vertices;
	std::vector<unsigned int>m_indices;
	IndexBuffer* m_indexBuffer = nullptr;
	VertexBuffer* m_vertexBuffer = nullptr;
	unsigned int m_numIndices = 0;

	//state
	bool m_isHoverable = false;
	bool m_isClickable = false;
	bool m_isDraggable = false;


};

