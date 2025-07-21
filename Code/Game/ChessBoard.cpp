#include "Game/ChessBoard.hpp"
#include "Game/Game.hpp"

#include "Engine/Math/Vec2.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Core/VertexUtils.hpp"

ChessBoard::ChessBoard():ChessObject()
{
}

ChessBoard::ChessBoard(Game* owner):ChessObject(owner)
{
}


void ChessBoard::InitializeLocalVerts()
{
	m_texture = m_game->m_boardTexturePack[0];
	m_normalTexture = m_game->m_boardTexturePack[1];
	m_sgeTexture = m_game->m_boardTexturePack[2];

	//one giant board
	/*
	Vec2 UVMins, UVMaxs;
	m_game->GetDefaultSpriteSheet()->GetSpriteUVs(UVMins, UVMaxs, m_spritesheetIndex);

	AABB3 boardBox(Vec3::ZERO, Vec3(8, 8, 1));
	Rgba8 boardColor = Rgba8::WHITE;
	std::vector<AABB2> boardUVArr;

	AABB2 borderUVs = AABB2::ZERO_TO_ONE;
	AABB2 faceUVs(Vec2::ZERO, Vec2(8,8));

	for (int i = 0; i < 6; i++)
	{
		if (i < 4)
		{
			boardUVArr.push_back(borderUVs);
		}
		else
		{
			boardUVArr.push_back(faceUVs);
		}
		
	}

	AddVertsForCube3D(m_vertices, m_indices, boardBox, boardColor, boardUVArr);
	*/

	//64 cubes
	Rgba8 boardColor = Rgba8::WHITE;
	Rgba8 boardColorBlack = Rgba8::DARK_GRAY;
	Rgba8 tempColor;
	std::vector<AABB2> boardUVArr;
	AABB2 UVs = AABB2::ZERO_TO_ONE;
	AABB3 boardBox;

	for (int i = 0; i < 6; i++)
	{
		boardUVArr.push_back(UVs);
	}

	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 8; j++)
		{
			float ifl = (float)i;
			float jfl = (float)j;
			boardBox.m_mins = Vec3(jfl, ifl, 0);
			boardBox.m_maxs = Vec3(jfl + 1, ifl + 1, m_boardHeight);

			if ((i + j) % 2 == 0)
			{
				tempColor = boardColorBlack;
			}
			else {
				tempColor = boardColor;
			}

			AddVertsForCube3D(m_vertices, m_indices, boardBox, tempColor, boardUVArr);

		}
	}


	//4 margins
	Rgba8 marginColor = Rgba8::GRAY;

	AABB3 marginBox1(Vec3(0,-m_boardMarginThickness,0), Vec3(8 + m_boardMarginThickness, 0, m_boardMarginThickness));
	AddVertsForCube3D(m_vertices, m_indices, marginBox1, marginColor, boardUVArr);

	AABB3 marginBox2(Vec3(-m_boardMarginThickness, -m_boardMarginThickness,  0), Vec3(0, 8,  m_boardMarginThickness));
	AddVertsForCube3D(m_vertices, m_indices, marginBox2, marginColor, boardUVArr);

	AABB3 marginBox3(Vec3(-m_boardMarginThickness, 8, 0), Vec3(8, 8 + m_boardMarginThickness, m_boardMarginThickness));
	AddVertsForCube3D(m_vertices, m_indices, marginBox3, marginColor, boardUVArr);

	AABB3 marginBox4(Vec3(8, 0, 0), Vec3(8 + m_boardMarginThickness, 8 + m_boardMarginThickness, m_boardMarginThickness));
	AddVertsForCube3D(m_vertices, m_indices, marginBox4, marginColor, boardUVArr);

	

}
