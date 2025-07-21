#pragma once
#include "Game/Entity.hpp"

#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex_PCUTBN.hpp"

#include <vector>

class Texture;
class VertexBuffer;
class IndexBuffer;

class ChessObject: public Entity
{
public:
	ChessObject();
	ChessObject(Game* owner);
	~ChessObject();

	void Update(float deltaSeconds)override;
	void Render()const override;

	virtual void InitializeLocalVerts();
	virtual void CreateBuffers();

public:
	std::vector<Vertex_PCUTBN> m_vertices;
	std::vector<unsigned int> m_indices;
	VertexBuffer* m_vertexBuffer = nullptr;
	IndexBuffer* m_indexBuffer = nullptr;
	unsigned int m_numIndices = 0;
	Texture* m_texture = nullptr;
	Texture* m_normalTexture = nullptr;
	Texture* m_sgeTexture = nullptr;
};

