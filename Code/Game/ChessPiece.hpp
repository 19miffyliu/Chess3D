#pragma once
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Audio/AudioSystem.hpp"

#include "Game/ChessObject.hpp"
#include "Game/ChessPieceDefinition.hpp"

class ChessMatch;


class ChessPiece : public ChessObject
{
public:
	ChessPiece();
	ChessPiece(Game* owner);
	~ChessPiece();

	void Update(float deltaSeconds)override;
	void Render()const override;

	//does not check for coord validity, only check for rules of the piece
	bool IsValidMoveForCoords(IntVec2 nextCoords)const;

public:
	ChessMatch* m_currentMatch = nullptr;
	const ChessPieceDefinition* m_pieceDef = nullptr;

	//temp var
	IntVec2 m_prevCoords = IntVec2(0, 0);
	IntVec2 m_currentCoords; //(0,0) to (7,7)

	float m_secondsSinceMoved = 0;
	int m_turnLastMoved = 0;
	SoundID m_moveSFXID = MISSING_SOUND_ID;

	bool m_player1Side = false; //0 = player 0, 1 = player 1
};

