#include "Game/ChessPiece.hpp"
#include "Game/ChessMatch.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Audio/AudioSystem.hpp"

ChessPiece::ChessPiece():ChessObject()
{
}

ChessPiece::ChessPiece(Game* owner):ChessObject(owner)
{
}

ChessPiece::~ChessPiece()
{
	
	m_currentMatch = nullptr;
	m_pieceDef = nullptr;
}

void ChessPiece::Update(float deltaSeconds)
{//chessmove from=e2 to=e4 chessmove from=b1 to=a3
	Vec3 toWorldCoords = GetWorldCoordForBoardCoords(m_currentCoords);

	if (m_prevCoords != m_currentCoords && m_position != toWorldCoords)
	{
		//interpolate the movement
		m_secondsSinceMoved += deltaSeconds;
		Vec3 fromWorldCoords = GetWorldCoordForBoardCoords(m_prevCoords);

		Vec3 moveDirection = (toWorldCoords - fromWorldCoords);
		float moveDistance = moveDirection.GetLength();
		float moveDuration = 0.5f + (0.25f * moveDistance);

		if (m_secondsSinceMoved < moveDuration)
		{
			float timePercent = GetClamped(m_secondsSinceMoved / moveDuration, 0.f, 1.f);
			float lerpPercent = (float)SmoothStop(5, timePercent);

			//if (m_pieceDef->m_type == ChessPieceType::KNIGHT)
			m_position = fromWorldCoords + (moveDirection * lerpPercent);
			m_position.z += (float)Parabola(timePercent);

			if (m_position == toWorldCoords)
			{
				g_theAudio->StartSound(m_moveSFXID, false, Game::s_SFXVolume);
			}
		}
		else {
			m_secondsSinceMoved = 0;
			m_position = toWorldCoords;
			g_theAudio->StartSound(m_moveSFXID, false, Game::s_SFXVolume);
		}


		
	}
	
	
}

void ChessPiece::Render() const
{
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP, 0);
	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP, 1);
	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP, 2);

	g_theRenderer->SetModelConstants(GetModelToWorldTransform(), m_color);
	g_theRenderer->BindTexture(m_texture, 0);
	g_theRenderer->BindTexture(m_normalTexture, 1);
	g_theRenderer->BindTexture(m_sgeTexture, 2);

	int index = m_player1Side ? 1 : 0;
	g_theRenderer->DrawIndexedVertexBuffer(m_pieceDef->m_vertexBufferByPlayer[index], 
		m_pieceDef->m_indexBufferByPlayer[index], m_pieceDef->m_numIndices);
}

bool ChessPiece::IsValidMoveForCoords(IntVec2 nextCoords) const
{
	//assumes the nextcoords is valid coords
	//coords are not equal, and no piece is on the way

	ChessPieceType type = m_pieceDef->m_type;
	int absDistX = abs(nextCoords.x - m_currentCoords.x);
	int absDistY = abs(nextCoords.y - m_currentCoords.y);
	int taxicabDist = absDistX + absDistY;

	bool firstMove = m_turnLastMoved == 0;

	bool movesDiagonally = absDistX == absDistY;
	bool movesStraight = (nextCoords.x == m_currentCoords.x) || (nextCoords.y == m_currentCoords.y);
	bool diagonallyNear = (taxicabDist == 2) && movesDiagonally;
	
	int forwardSign = m_player1Side ? -1 : 1;
	int movesYDirection = (nextCoords.y - m_currentCoords.y) * forwardSign;
	bool movesForward = movesYDirection > 0;

	if (type == ChessPieceType::PAWN)
	{//moves forward 2 north, 1 north, or 1 diagonally
		if (!movesForward)
		{
			return false;
		}
		bool verticalValid = movesStraight && ((firstMove && absDistY == 2) || absDistY == 1);

		/*
		bool enPassant = false;
		const ChessPiece* capturingPiece = m_currentMatch->GetPieceAtCoords(nextCoords);
		bool hasPieceAtDestination = capturingPiece != nullptr;
		bool pieceAtDestinationIsEnemyPiece = hasPieceAtDestination && capturingPiece->m_player1Side != m_player1Side;
		
		if (!hasPieceAtDestination && movesDiagonally)
		{
			capturingPiece = m_currentMatch->GetPieceAtCoords(IntVec2(nextCoords.x, m_currentCoords.y));
			if (capturingPiece !=nullptr && capturingPiece->m_pieceDef->m_type == ChessPieceType::PAWN)
			{
				int absDistYOp = abs(capturingPiece->m_currentCoords.y - capturingPiece->m_prevCoords.y);
				bool isAdjacentToMeHoriztonally = m_currentCoords.y == capturingPiece->m_currentCoords.y && abs(m_currentCoords.x - capturingPiece->m_currentCoords.x) == 1;
				if (isAdjacentToMeHoriztonally && capturingPiece->m_turnLastMoved == m_currentMatch->GetTurnNum() - 1 && absDistYOp == 2)
				{
					enPassant = true;
				}
			}
		}

		return verticalValid || (diagonallyNear && (pieceAtDestinationIsEnemyPiece || enPassant));*/
		return verticalValid || diagonallyNear;


	}
	else if (type == ChessPieceType::KING)
	{//moves to nearby 8 tiles
		bool NSWE = taxicabDist == 1;
		bool castling = firstMove && absDistX == 2 && absDistY == 0;

		return NSWE || diagonallyNear || castling;
	}
	else if (type == ChessPieceType::ROOK)
	{//moves horizontally or vertically
		return movesStraight;
	}
	else if (type == ChessPieceType::BISHOP)
	{//moves diagonally
		return movesDiagonally;
	}
	else if (type == ChessPieceType::KNIGHT)
	{//moves L shape
		return (absDistX == 1 && absDistY == 2) || (absDistX == 2 && absDistY == 1);
	}
	else if (type == ChessPieceType::QUEEN)
	{//moves L shape
		return movesDiagonally || movesStraight;
	}
	

	ERROR_AND_DIE("piece type not fitting any of the definitions");
	//return false;
}

