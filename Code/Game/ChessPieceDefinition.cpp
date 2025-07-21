#include "Game/ChessPieceDefinition.hpp"
#include "Game/GameCommon.hpp"

#include "Engine/Math/OBB3.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/StringUtils.hpp"


std::vector<ChessPieceDefinition*> ChessPieceDefinition::s_chessPieceDefinitions;



ChessPieceDefinition::ChessPieceDefinition()
{


}

ChessPieceDefinition::~ChessPieceDefinition()
{
	delete m_vertexBufferByPlayer[0];
	m_vertexBufferByPlayer[0] = nullptr;
	delete m_vertexBufferByPlayer[1];
	m_vertexBufferByPlayer[1] = nullptr;

	delete m_indexBufferByPlayer[0];
	m_indexBufferByPlayer[0] = nullptr;
	delete m_indexBufferByPlayer[1];
	m_indexBufferByPlayer[1] = nullptr;

	
}

void ChessPieceDefinition::InitializeDefinitions(const char* path)
{
	XmlDocument document;
	XmlError result = document.LoadFile(path);
	if (result != 0)
	{
		ERROR_AND_DIE("ChessPieceDefinition::InitializeDefinitions: Error loading file");
	}
	XmlElement* rootElement = document.RootElement();
	XmlElement* chessPieceElement = rootElement->FirstChildElement();

	while (chessPieceElement != nullptr)
	{
		std::string elementName = chessPieceElement->Name();
		ChessPieceDefinition* chessDef = new ChessPieceDefinition();
		chessDef->LoadFromXmlElement(*chessPieceElement);
		chessDef->InitLocalVertsForType();
		s_chessPieceDefinitions.push_back(chessDef);
		g_numAssetsLoaded++;
		chessPieceElement = chessPieceElement->NextSiblingElement();
	}



}

void ChessPieceDefinition::CreateBufferForDefinitions()
{
	int numDefinitions = (int)s_chessPieceDefinitions.size();
	for (int i = 0; i < numDefinitions; i++)
	{
		//create buffers
		s_chessPieceDefinitions[i]->CreateBuffersForVertsAndIndices();
		s_chessPieceDefinitions[i]->ClearVerts();
	}
}

void ChessPieceDefinition::ClearDefinitions()
{
	for (int i = 0; i < (int)s_chessPieceDefinitions.size(); i++)
	{
		delete s_chessPieceDefinitions[i];
		s_chessPieceDefinitions[i] = nullptr;
	}
	s_chessPieceDefinitions.clear();
}

const ChessPieceDefinition* ChessPieceDefinition::GetByName(std::string const& name, bool ignoreCase)
{
	std::string tempName;
	for (int i = 0; i < (int)s_chessPieceDefinitions.size(); i++)
	{
		tempName = s_chessPieceDefinitions[i]->m_name;
		if (tempName == name || (ignoreCase && IsEqualStrIgnoreCase(tempName,name)))
		{
			return s_chessPieceDefinitions[i];
		}
	}
	return nullptr;
}

const ChessPieceDefinition* ChessPieceDefinition::GetByGlyph(char const glyph)
{
	for (int i = 0; i < (int)s_chessPieceDefinitions.size(); i++)
	{
		if (s_chessPieceDefinitions[i]->m_glyphlowerCase == glyph
			|| s_chessPieceDefinitions[i]->m_glyphUpperCase == glyph)
		{
			return s_chessPieceDefinitions[i];
		}
	}
	return nullptr;
}

bool ChessPieceDefinition::LoadFromXmlElement(XmlElement const& element)
{
	m_name = ParseXmlAttribute(element, "name", m_name);

	m_glyphUpperCase = ParseXmlAttribute(element, "glyphUpperCase", 'P');
	m_glyphlowerCase = ParseXmlAttribute(element, "glyphLowerCase", 'p');
	m_type = (ChessPieceType)ParseXmlAttribute(element, "type", 0);

	return true;
}

void ChessPieceDefinition::InitLocalVertsForType()
{
	m_vertices.reserve(2000);
	m_indices.reserve(4000);

	Vec2 centerXY = Vec2::ZERO;

	if (m_type == ChessPieceType::PAWN)
	{
		//1 sphere, 3 cylinders

		//base cylinder
		FloatRange baseMinMaxZ(0,0.15f);
		float baseRadius = 0.2f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, baseMinMaxZ, baseRadius, 16);

		//body cylinder
		FloatRange bodyMinMaxZ(0.15f, 0.4f);
		float bodyRadius = 0.1f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, bodyMinMaxZ, bodyRadius, 16);

		//neck cylinder
		FloatRange neckMinMaxZ(0.4f, 0.5f);
		float neckRadius = 0.15f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, neckMinMaxZ, neckRadius, 16);

		//head sphere
		float sphereRadius = 0.13f;
		Vec3 sphereCenter(0,0,0.6f);
		AddVertsForSphere3D(m_vertices, m_indices, sphereCenter,sphereRadius);


		m_modelRadius = bodyRadius;
		m_modelHeight = sphereCenter.z + sphereRadius;

	}
	else if (m_type == ChessPieceType::ROOK)
	{
		//1 cylinder, 3 AABB3

		//base cylinder
		FloatRange baseMinMaxZ(0, 0.15f);
		float baseRadius = 0.25f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, baseMinMaxZ, baseRadius, 16);


		//body AABB3
		AABB3 tempBox;
		tempBox.m_mins = Vec3(-0.15f, -0.15f, 0.15f);
		tempBox.m_maxs = Vec3(0.15f, 0.15f, 0.5f);
		AddVertsForAABBZ3D(m_vertices, m_indices, tempBox);

		//neck AABB3
		tempBox.m_mins = Vec3(-0.17f,-0.17f, 0.5f);
		tempBox.m_maxs = Vec3(0.17f, 0.17f, 0.55f);
		AddVertsForAABBZ3D(m_vertices, m_indices, tempBox);


		//head AABB3
		tempBox.m_mins = Vec3(-0.15f, -0.15f, 0.55f);
		tempBox.m_maxs = Vec3(0.15f, 0.15f, 0.6f);
		AddVertsForAABBZ3D(m_vertices, m_indices, tempBox);


		m_modelRadius = 0.15f;
		m_modelHeight = tempBox.m_maxs.z;


	}
	else if (m_type == ChessPieceType::KNIGHT)
	{
		//1 cylinder, 1 AABB3, 2 OBB3

		//base cylinder
		FloatRange baseMinMaxZ(0, 0.15f);
		float baseRadius = 0.3f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, baseMinMaxZ, baseRadius, 16);

		//body AABB3
		AABB3 tempBox;
		tempBox.m_mins = Vec3(-0.2f, -0.15f, 0.15f);
		tempBox.m_maxs = Vec3(0.2f, 0.15f, 0.6f);
		AddVertsForAABBZ3D(m_vertices, m_indices, tempBox);

		//head OBB3
		Vec3 headBoxCenter(0,0,0.65f);
		float radians = ConvertDegreesToRadians(15.f);
		float yaw = cosf(radians);
		float pitch = sinf(radians);
		Vec3 headBoxI(yaw,0,-pitch);
		Vec3 headBoxJ(0,1,0);
		Vec3 headBoxHalfDim(0.1f,0.07f,0.15f);
		OBB3 headBox(headBoxCenter, headBoxI, headBoxJ, headBoxHalfDim);
		AddVertsForOBB3D(m_vertices, m_indices, headBox);


		//mouth OBB3
		Vec3 mouthBoxCenter(0.15f, 0, 0.65f);
		Vec3 mouthBoxI(yaw, 0, -pitch);
		Vec3 mouthBoxJ(0, 1, 0);
		Vec3 mouthBoxHalfDim(0.1f, 0.06f, 0.04f);
		OBB3 mouthBox(mouthBoxCenter, mouthBoxI, mouthBoxJ, mouthBoxHalfDim);
		AddVertsForOBB3D(m_vertices, m_indices, mouthBox);


		m_modelRadius = tempBox.m_maxs.x;
		m_modelHeight = headBoxCenter.z + headBoxHalfDim.z;

	}
	else if (m_type == ChessPieceType::BISHOP)
	{
		//2 sphere, 3 cylinder

		//base cylinder
		FloatRange baseMinMaxZ(0, 0.15f);
		float baseRadius = 0.3f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, baseMinMaxZ, baseRadius, 16);

		//body cylinder
		FloatRange bodyMinMaxZ(0.15f, 0.5f);
		float bodyRadius = 0.12f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, bodyMinMaxZ, bodyRadius, 16);

		//neck cylinder
		FloatRange neckMinMaxZ(0.5f, 0.57f);
		float neckRadius = 0.2f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, neckMinMaxZ, neckRadius, 16);

		//head sphere
		float headSphereRadius = 0.17f;
		Vec3 headSphereCenter(0, 0, 0.67f);
		AddVertsForSphere3D(m_vertices, m_indices, headSphereCenter, headSphereRadius);

		//tip sphere
		float tipSphereRadius = 0.04f;
		Vec3 tipSphereCenter(0, 0, 0.84f);
		AddVertsForSphere3D(m_vertices, m_indices, tipSphereCenter, tipSphereRadius,Rgba8::WHITE,AABB2::ZERO_TO_ONE,8,4);

		m_modelRadius = bodyRadius;
		m_modelHeight = tipSphereCenter.z + tipSphereRadius;

	}
	else if (m_type == ChessPieceType::QUEEN)
	{
		//3 cylinder, 2 AABB3, 2 OBB3

		//base cylinder
		FloatRange baseMinMaxZ(0, 0.15f);
		float baseRadius = 0.3f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, baseMinMaxZ, baseRadius, 16);

		//body cylinder
		FloatRange bodyMinMaxZ(0.15f, 0.65f);
		float bodyRadius = 0.13f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, bodyMinMaxZ, bodyRadius, 16);

		//neck cylinder
		FloatRange neckMinMaxZ(0.65f, 0.75f);
		float neckRadius = 0.2f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, neckMinMaxZ, neckRadius, 16);

		//head AABB3 1
		AABB3 tempBox;
		tempBox.m_mins = Vec3(-0.2f, -0.05f, 0.75f);
		tempBox.m_maxs = Vec3(0.2f, 0.05f, 0.85f);
		AddVertsForAABBZ3D(m_vertices, m_indices, tempBox);

		//head AABB3 2
		tempBox.m_mins = Vec3(-0.05f, -0.2f, 0.75f);
		tempBox.m_maxs = Vec3(0.05f, 0.2f, 0.85f);
		AddVertsForAABBZ3D(m_vertices, m_indices, tempBox);

		//head OBB3 1
		Vec3 headBoxCenter(0, 0, 0.8f);
		Vec3 headBoxI(sqrt2by2, sqrt2by2, 0);
		Vec3 headBoxJ(-sqrt2by2, sqrt2by2, 0);
		Vec3 headBoxHalfDim(0.2f, 0.05f, 0.05f);
		OBB3 headBox(headBoxCenter, headBoxI, headBoxJ, headBoxHalfDim);
		AddVertsForOBB3D(m_vertices, m_indices, headBox);

		//head OBB3 2
		headBoxI = Vec3(sqrt2by2, -sqrt2by2, 0);
		headBoxJ = Vec3(sqrt2by2, sqrt2by2, 0);
		OBB3 headBox2(headBoxCenter, headBoxI, headBoxJ, headBoxHalfDim);
		AddVertsForOBB3D(m_vertices, m_indices, headBox2);

		m_modelRadius = bodyRadius;
		m_modelHeight = tempBox.m_maxs.z + headBoxHalfDim.z;
	}
	else if (m_type == ChessPieceType::KING)
	{
		//3 cylinder, 2 AABB3

		//base cylinder
		FloatRange baseMinMaxZ(0, 0.15f);
		float baseRadius = 0.3f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, baseMinMaxZ, baseRadius, 16);

		//body cylinder
		FloatRange bodyMinMaxZ(0.15f, 0.7f);
		float bodyRadius = 0.13f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, bodyMinMaxZ, bodyRadius, 16);

		//neck cylinder
		FloatRange neckMinMaxZ(0.7f, 0.8f);
		float neckRadius = 0.23f;
		AddVertsForCylinderZ3D(m_vertices, m_indices, centerXY, neckMinMaxZ, neckRadius, 16);

		//head AABB3 1
		AABB3 tempBox;
		tempBox.m_mins = Vec3(-0.05f, -0.05f, 0.8f);
		tempBox.m_maxs = Vec3(0.05f, 0.05f, 1.0f);
		AddVertsForAABBZ3D(m_vertices, m_indices, tempBox);

		//head AABB3 2
		tempBox.m_mins = Vec3(-0.05f, -0.2f, 0.85f);
		tempBox.m_maxs = Vec3(0.05f, 0.2f, 0.95f);
		AddVertsForAABBZ3D(m_vertices, m_indices, tempBox);

		m_modelRadius = bodyRadius;
		m_modelHeight = tempBox.m_maxs.z;

	}
	else {
		ERROR_AND_DIE("Invalid chess piece type!");
	}

}

void ChessPieceDefinition::CreateBuffersForVertsAndIndices()
{
	unsigned int numVertexes = (unsigned int)m_vertices.size();
	unsigned int numIndices = (unsigned int)m_indices.size();
	m_numIndices = numIndices;

	unsigned int singleVertSize = sizeof(Vertex_PCUTBN);
	unsigned int totalVertSize = numVertexes * singleVertSize;
	m_vertexBufferByPlayer[0] = g_theRenderer->CreateVertexBuffer(totalVertSize, singleVertSize);
	m_vertexBufferByPlayer[1] = g_theRenderer->CreateVertexBuffer(totalVertSize, singleVertSize);

	unsigned int uintSize = sizeof(unsigned int);
	unsigned int indexSize = numIndices * uintSize;
	m_indexBufferByPlayer[0] = g_theRenderer->CreateIndexBuffer(indexSize, uintSize);
	m_indexBufferByPlayer[1] = g_theRenderer->CreateIndexBuffer(indexSize, uintSize);

	
	//player 0
	g_theRenderer->CopyCPUToGPU(m_vertices.data(), totalVertSize, m_vertexBufferByPlayer[0]);
	g_theRenderer->CopyCPUToGPU(m_indices.data(), indexSize, m_indexBufferByPlayer[0]);

	//player 1
	g_theRenderer->CopyCPUToGPU(m_vertices.data(), totalVertSize, m_vertexBufferByPlayer[1]);
	g_theRenderer->CopyCPUToGPU(m_indices.data(), indexSize, m_indexBufferByPlayer[1]);
}

void ChessPieceDefinition::ClearVerts()
{
	m_vertices.clear();
	m_indices.clear();
}


