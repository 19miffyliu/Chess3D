#pragma once

#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/Vertex_PCUTBN.hpp"

#include <vector>
#include <string>


class VertexBuffer;
class IndexBuffer;

enum class ChessPieceType {
	NONE = -1,
	PAWN,
	ROOK,
	KNIGHT,
	BISHOP,
	QUEEN,
	KING,
	COUNT
};


class ChessPieceDefinition
{
public:
	ChessPieceDefinition();
	~ChessPieceDefinition();

	//static variable
	static void InitializeDefinitions(const char* path);
	static void CreateBufferForDefinitions();
	static void ClearDefinitions();
	static const ChessPieceDefinition* GetByName(std::string const& name, bool ignoreCase = true);
	static const ChessPieceDefinition* GetByGlyph(char const glyph);
	static std::vector<ChessPieceDefinition*> s_chessPieceDefinitions;

	bool LoadFromXmlElement(XmlElement const& element);

	void InitLocalVertsForType();
	void CreateBuffersForVertsAndIndices();
	void ClearVerts();

public:
	std::string m_name;
	ChessPieceType m_type = ChessPieceType::NONE;
	char m_glyphUpperCase = '?';
	char m_glyphlowerCase = '?';
	
	IndexBuffer* m_indexBufferByPlayer[2] = {};
	VertexBuffer* m_vertexBufferByPlayer[2] = {};
	unsigned int m_numIndices = 0;

	//temp
	std::vector<Vertex_PCUTBN> m_vertices;
	std::vector<unsigned int> m_indices;

	//debug draw
	float m_modelRadius = 0;
	float m_modelHeight = 0;
	

};

