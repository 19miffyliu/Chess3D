#pragma once
#include "Engine/Math/IntVec2.hpp"
#include "Game/GameCommon.hpp"

#include "Game/ChessObject.hpp"



class Game;


class ChessBoard : public ChessObject
{
public:
	ChessBoard();
	ChessBoard(Game* owner);


	void InitializeLocalVerts()override;
	

public:
	IntVec2 m_spritesheetIndex = IntVec2(7,1);

	float m_boardHeight = BOARD_HEIGHT;
	float m_boardMarginThickness = BOARD_HEIGHT;

};

