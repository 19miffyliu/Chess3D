#include "Game/ChessObject.hpp"
#include "Game/GameCommon.hpp"

#include "Engine/Renderer/Renderer.hpp"

ChessObject::ChessObject():Entity()
{
}

ChessObject::ChessObject(Game* owner)
:Entity(owner){
}

ChessObject::~ChessObject()
{
	m_texture = nullptr;
	m_normalTexture = nullptr;

	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;
	delete m_indexBuffer;
	m_indexBuffer = nullptr;
}

void ChessObject::Update(float deltaSeconds)
{
	m_orientation += m_angularVelocity * deltaSeconds;
}

void ChessObject::Render() const
{
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP, 0);
	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP, 1);
	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP, 2);

	g_theRenderer->BindTexture(m_texture, 0);
	g_theRenderer->BindTexture(m_normalTexture, 1);
	g_theRenderer->BindTexture(m_sgeTexture, 2);

	g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, m_numIndices);
}

void ChessObject::InitializeLocalVerts()
{

}

void ChessObject::CreateBuffers()
{
	unsigned int numVertexes = (unsigned int)m_vertices.size();
	unsigned int numIndices = (unsigned int)m_indices.size();
	m_numIndices = numIndices;

	unsigned int singleVertSize = sizeof(Vertex_PCUTBN);
	unsigned int totalVertsSize = numVertexes * singleVertSize;
	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(totalVertsSize, singleVertSize);

	unsigned int uintSize = sizeof(unsigned int);
	unsigned int indexSize = numIndices * uintSize;
	m_indexBuffer = g_theRenderer->CreateIndexBuffer(indexSize, uintSize);

	g_theRenderer->CopyCPUToGPU(m_vertices.data(), totalVertsSize, m_vertexBuffer);
	g_theRenderer->CopyCPUToGPU(m_indices.data(), indexSize, m_indexBuffer);
}
