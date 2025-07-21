#include "Game/ChessMatch.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/App.hpp"
#include "Game/AudioDefinition.hpp"

#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Math/IntRange.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/VertexUtils.hpp"

ChessMoveResult ChessMatch::s_chessMoveResult = ChessMoveResult();
bool ChessMatch::s_hasPreDefinedResult = false;
IntVec2 ChessMatch::s_formerCoords = IntVec2(0, 0);
IntVec2 ChessMatch::s_desiredCoords = IntVec2(0, 0);
char ChessMatch::s_promoteToPiece = '?';
bool ChessMatch::s_autoPromoteToQueen = false;
bool ChessMatch::s_matchEnds = false;
int ChessMatch::s_result = -1;
bool ChessMatch::s_player1Turn = false;
bool ChessMatch::s_isTeleporting = false;


ChessMatch::ChessMatch()
{
}

ChessMatch::ChessMatch(Game* owner, int startingPlayerIndex)
	:m_game(owner)
{
	s_player1Turn = startingPlayerIndex == 1;
	Startup();
}

ChessMatch::~ChessMatch()
{
	m_game = nullptr;

	//g_theEventSystem->UnsubscribeEventCallbackFunction("chessmove", Event_ChessMove);
	//g_theEventSystem->UnsubscribeEventCallbackFunction("resign", Event_Resign);
}

void ChessMatch::Startup()
{
	SetBoardStateByString(m_game->GetDefaultBoardState());

	//EventArgs args;
	//args.SetValue("from", "e2");
	//args.SetValue("to", "e4");
	//args.SetValue("promoteTo", "queen(pawn only)");
	//args.SetValue("teleport", "true(optional)");
	//g_theEventSystem->SubscribeEventCallbackFunction("chessmove", Event_ChessMove, args);
	//g_theEventSystem->SubscribeEventCallbackFunction("resign", Event_Resign);

}

void ChessMatch::Update(float deltaSeconds)
{
	if (!s_matchEnds)
	{
		UpdatePlayerRaycastHit();
		UpdatePlayerHighlight();
	}
	UpdateMovePieceCommand();

	int numPieces = (int)m_pieces.size();
	for (int i = 0; i < numPieces; i++) {
		m_pieces[i].Update(deltaSeconds);
	}

	
}

void ChessMatch::UpdatePlayerRaycastHit()
{
	Vec3 rayStart = m_game->GetWorldCameraPos();
	Vec3 rayFwdNormal = m_game->GetWorldCameraFwdNormal();
	float rayLen = m_game->m_playerPointDistance;
	RaycastResult3D pieceResult = RaycastAgainstAllPieces(rayStart, rayFwdNormal, rayLen);
	bool wasPieceHit = pieceResult.m_didImpact;
	RaycastResult3D boardResult = RaycastAgainstAllBoardSquares(rayStart, rayFwdNormal, rayLen);
	bool wasBoardHit = boardResult.m_didImpact;

	if (!wasPieceHit && !wasBoardHit)//did not hit anything
	{
		m_raycastingCoords = IntVec2(-1,-1);
		return;
	}
	else if (!wasPieceHit) //only board was hit
	{
		m_raycastingCoords = GetBoardCoordsForWorldPos(boardResult.m_impactPos);
	}
	else if (!wasBoardHit) //only piece was hit
	{
		m_raycastingCoords = GetBoardCoordsForWorldPos(pieceResult.m_impactPos);
	}
	else {
		Vec3 hitPos = pieceResult.m_impactDist <= boardResult.m_impactDist ? pieceResult.m_impactPos : boardResult.m_impactPos;
		m_raycastingCoords = GetBoardCoordsForWorldPos(hitPos);
	}


}

RaycastResult3D ChessMatch::RaycastAgainstAllPieces(Vec3 const& rayStart, Vec3 const& rayFwdNormal, float rayLen)
{
	int numPieces = (int)m_pieces.size();
	RaycastResult3D tempResult;
	float tempHitDistance = FLT_MAX;

	Vec2 pieceCenter2D;
	float boardHeight = m_game->m_boardModel.m_boardHeight;
	FloatRange pieceHeightRange(boardHeight, boardHeight);
	float pieceRadius;

	for (int i = 0; i < numPieces; i++)
	{
		pieceCenter2D = GetFromVec3(m_pieces[i].m_position);
		FloatRange tempRange = pieceHeightRange;
		tempRange.m_max += m_pieces[i].m_pieceDef->m_modelHeight;
		pieceRadius = m_pieces[i].m_pieceDef->m_modelRadius;
		RaycastResult3D result = RaycastVsCylinder3D(rayStart, rayFwdNormal, rayLen, pieceCenter2D, tempRange, pieceRadius);

		if (result.m_didImpact && result.m_impactDist < tempHitDistance)
		{
			tempResult = result;
			tempHitDistance = result.m_impactDist;
		}
	}

	return tempResult;
}

RaycastResult3D ChessMatch::RaycastAgainstAllBoardSquares(Vec3 const& rayStart, Vec3 const& rayFwdNormal, float rayLen)
{
	int boardEdgeNum = 8;
	RaycastResult3D tempResult;
	float tempHitDistance = FLT_MAX;

	float boardHeight = m_game->m_boardModel.m_boardHeight;
	AABB3 boardSquare;

	for (int i = 0; i < boardEdgeNum; i++)
	{
		for (int j = 0; j < boardEdgeNum; j++)
		{
			boardSquare.m_mins = Vec3((float)j, (float)i, 0);
			boardSquare.m_maxs = Vec3((float)(j+1), (float)(i+1), boardHeight);
			RaycastResult3D result = RaycastVsAABB3D(rayStart, rayFwdNormal, rayLen, boardSquare);
			if (result.m_didImpact && result.m_impactDist < tempHitDistance)
			{
				tempResult = result;
				tempHitDistance = result.m_impactDist;
			}
		}
	}

	return tempResult;
}

void ChessMatch::UpdatePlayerHighlight()
{
	if (Game::s_isSpectator)
	{//if is spectator
		return;
	}

	if (m_raycastingCoords == IntVec2(-1,-1)) {
		m_highlightingCoords = IntVec2(-1, -1);
		return;//not raycasting anything
	}
	int currentPlayerIndex = (int)s_player1Turn;
	int playerIndexOfPieceAtCoords = GetPlayerIndexForPieceAtCoords(m_raycastingCoords);


	//if (m_selectedPiece != nullptr && m_selectedPiece->IsValidMoveForCoords(m_raycastingCoords))
	if(m_selectedPiece != nullptr)//if a piece is already selected
	{
		ChessMoveResult tempResult;
		bool isMoveValid = CheckMoveValidity(m_selectedPiece->m_currentCoords, m_raycastingCoords, tempResult);

		if (isMoveValid || ChessMatch::s_isTeleporting)
		{
			m_highlightingCoords = m_raycastingCoords;

			//store the result globally
			ChessMatch::s_chessMoveResult = tempResult;
			ChessMatch::s_hasPreDefinedResult = true;
		}
		
	}
	else if (m_selectedPiece == nullptr && currentPlayerIndex == playerIndexOfPieceAtCoords)//no piece is selected but I'm raycasting a current player's piece
	{
		if (!Game::s_isPlayingRemotely //if I'm playing with myself (playing both player 0 and player 1)
			|| (Game::s_isPlayingRemotely && s_player1Turn == Game::s_myPlayerIndex))//if I'm raycasting my piece at my turn
		{
			m_highlightingCoords = m_raycastingCoords;
		}
		
	}
	else {
		m_highlightingCoords = IntVec2(-1, -1);
	}
}

void ChessMatch::HandlePlayerLeftClick()
{
	if (m_highlightingCoords == IntVec2(-1,-1)) {//if not selecting anything
		return;
	}

	const ChessPiece* highlightedPiece = GetPieceAtCoords(m_highlightingCoords);
	if (highlightedPiece != nullptr && highlightedPiece->m_player1Side == s_player1Turn)
	{//if selecting another friendly piece, de-select previous piece to select this piece
		if (m_selectedPiece != nullptr)
		{
			m_selectedPiece->m_color = Rgba8::WHITE;
		}
		int pieceIndex = GetPieceIndexForCoords(m_highlightingCoords);
		m_selectedPiece = &m_pieces[pieceIndex];
		m_selectedPiece->m_color = m_game->m_playerMouseSelectedColor;
	}
	else if (m_selectedPiece != nullptr)
	{//moving, or capturing

		ChessMatch::s_formerCoords = m_selectedPiece->m_currentCoords;
		ChessMatch::s_desiredCoords = m_highlightingCoords;
		ChessMatch::s_autoPromoteToQueen = true;

		Player player = m_game->GetPlayerByIndex((int)ChessMatch::s_player1Turn);
		m_selectedPiece->m_color = player.m_playerColor;
		m_selectedPiece = nullptr;
	}



}

void ChessMatch::HandlePlayerRightClick()
{
	if (m_selectedPiece != nullptr)
	{
		Player player = m_game->GetPlayerByIndex((int)ChessMatch::s_player1Turn);
		m_selectedPiece->m_color = player.m_playerColor;
	}
	m_selectedPiece = nullptr;

}

void ChessMatch::HandlePlayerTeleporting(bool isTeleporting)
{
	ChessMatch::s_isTeleporting = isTeleporting;
}

void ChessMatch::UpdateMovePieceCommand()
{
	if (s_formerCoords == s_desiredCoords) return;

	IntVec2 fromCoords = s_formerCoords;
	IntVec2 toCoords = s_desiredCoords;

	s_formerCoords = IntVec2::ZERO;
	s_desiredCoords = IntVec2::ZERO;

	if (s_matchEnds)
	{
		PrintErrorMsgToConsole("Invalid command: Match Ended");
		return;
	}
	int currentPlayerIndex = (int)s_player1Turn;

	int playerIndexFrom = GetPlayerIndexForPieceAtCoords(fromCoords);
	if (playerIndexFrom == -1)
	{
		PrintErrorMsgToConsole("Invalid FROM args: FROM square has no piece");
		return;
	}
	else if (playerIndexFrom != currentPlayerIndex)
	{
		PrintErrorMsgToConsole("Invalid FROM args: FROM Piece is opponent's piece");
		return;
	}

	int playerIndexTo = GetPlayerIndexForPieceAtCoords(toCoords);
	if (playerIndexTo == currentPlayerIndex)
	{
		PrintErrorMsgToConsole("Invalid TO args: TO Piece is your piece");
		return;
	}


	

	MovePiece(fromCoords, toCoords);
	


}

void ChessMatch::Render() const
{
	
	

	int numPieces = (int)m_pieces.size();
	for (int i = 0; i < numPieces; i++) {
		
		m_pieces[i].Render();
	}

	RenderPlayerSelections();
}

void ChessMatch::RenderPlayerSelections() const
{
	if (m_highlightingCoords == IntVec2(-1, -1) && m_selectedPiece == nullptr)
	{
		return;
	}
	g_theRenderer->SetModelConstants();
	g_theRenderer->BindTexture(nullptr, 0);
	g_theRenderer->BindTexture(nullptr, 1);

	g_theRenderer->BindShader(nullptr);

	std::vector<Vertex_PCU> tempVerts;
	float tableHeight = m_game->m_boardModel.m_boardHeight;

	if (m_highlightingCoords != IntVec2(-1,-1))
	{
		//draw coords
		Vec3 coordsBL((float)m_highlightingCoords.x, (float)m_highlightingCoords.y, 0);
		Vec3 coordsTR(m_highlightingCoords.x + 1.f, m_highlightingCoords.y + 1.f, tableHeight + 0.02f);
		AABB3 tempBox(coordsBL, coordsTR);
		AddVertsForAABBWireframe3D(tempVerts, tempBox, 0.05f);


		//draw chess piece as cylinder
		int pieceIndex = GetPieceIndexForCoords(m_highlightingCoords);
		if (pieceIndex != -1)
		{
			Vec2 pieceCenter2D = GetFromVec3(m_pieces[pieceIndex].m_position);
			FloatRange pieceHeightRange(tableHeight, tableHeight + m_pieces[pieceIndex].m_pieceDef->m_modelHeight);
			float pieceRadius = m_pieces[pieceIndex].m_pieceDef->m_modelRadius;

			AddVertsForCylinderZWireframe3D(tempVerts, pieceCenter2D, pieceHeightRange, pieceRadius, 8, 0.05f);
		}

		g_theRenderer->DrawVertexArray(tempVerts);

	}

	

	g_theRenderer->BindShader(m_game->m_blinnPhongShader);


}

void ChessMatch::SetTurnNumber(int turnNumber)
{
	m_turnNumber = turnNumber;
}

//0 = player 0 wins, 1 = player 1 wins, 2 = draw
void ChessMatch::SetMatchResultAndEndTheGame(int gameResult)
{
	s_matchEnds = true;
	s_result = gameResult;
	HandleMatchEnd();
}

void ChessMatch::MovePieceOnLayout(IntVec2 const& fromCoords, IntVec2 const& toCoords, char const glyph)
{
	//move piece on layout
	SetPieceToCoords('.', fromCoords);
	SetPieceToCoords(glyph, toCoords);
}

void ChessMatch::MovePieceInVector(int fromPieceIndex, IntVec2 const& toCoords)
{
	//move piece in vector
	m_pieces[fromPieceIndex].m_prevCoords = m_pieces[fromPieceIndex].m_currentCoords;
	m_pieces[fromPieceIndex].m_currentCoords = toCoords;

	m_pieces[fromPieceIndex].m_secondsSinceMoved = 0;
	m_pieces[fromPieceIndex].m_turnLastMoved = m_turnNumber;
}

void ChessMatch::ChangePieceForIndex(int pieceIndex, const ChessPieceDefinition* def, IntVec2 const& coords, char const glyph)
{
	int layoutIndex = GetBoardStateIndexForBoardCoords(coords);
	m_layout[layoutIndex] = glyph;
	m_pieces[pieceIndex].m_pieceDef = def;
}

bool ChessMatch::CheckMoveValidity(IntVec2 const& fromCoords, IntVec2 const& toCoords, ChessMoveResult& result)
{
	if (fromCoords == toCoords)
	{
		result.m_errorMessage = "Invalid format: FROM piece is equal to TO piece";
		return true;
	}

	int pieceNum = (int)m_pieces.size();
	int pieceIndexFrom = -1;
	int pieceIndexToErase = -1;

	std::string fromStr = GetSquareCoordsForBoardCoords(fromCoords);
	std::string toStr = GetSquareCoordsForBoardCoords(toCoords);
	result.m_fromChessPieceCoordsStr = fromStr;
	result.m_toChessPieceCoordsStr = toStr;

	std::string fromChessPiece, toChessPiece;
	char fromChessPieceGlyph = '.';
	char toChessPieceGlyph = '.';
	char promoteToPieceGlyph = s_promoteToPiece;

	//get glyph and def from vector of existing pieces
	for (int i = 0; i < pieceNum; i++)
	{
		if (m_pieces[i].m_currentCoords == fromCoords)
		{
			pieceIndexFrom = i;
			fromChessPiece = m_pieces[i].m_pieceDef->m_name;
			fromChessPieceGlyph = m_pieces[i].m_player1Side ? m_pieces[i].m_pieceDef->m_glyphlowerCase : m_pieces[i].m_pieceDef->m_glyphUpperCase;
		}
		else if (m_pieces[i].m_currentCoords == toCoords)
		{
			pieceIndexToErase = i;
			toChessPiece = m_pieces[i].m_pieceDef->m_name;
			toChessPieceGlyph = m_pieces[i].m_player1Side ? m_pieces[i].m_pieceDef->m_glyphlowerCase : m_pieces[i].m_pieceDef->m_glyphUpperCase;
		}
	}

	result.m_pieceIndexFrom = pieceIndexFrom;
	result.m_fromChessPieceStr = fromChessPiece;
	result.m_fromChessPieceGlyph = fromChessPieceGlyph;

	result.m_pieceIndexTo = pieceIndexToErase;
	result.m_toChessPieceStr = toChessPiece;
	result.m_toChessPieceGlyph = toChessPieceGlyph;

	result.m_pieceIndexToErase = pieceIndexToErase;
	result.m_isCapturing = pieceIndexToErase != -1;


	int playerIndexTo = GetPlayerIndexForPieceAtCoords(toCoords);
	if (playerIndexTo == (int)s_player1Turn)
	{
		result.m_errorMessage = "Invalid TO args: TO Piece is your piece";
		return false;
	}
	

	if (!s_isTeleporting)
	{
		ChessPieceType fromPieceType = m_pieces[pieceIndexFrom].m_pieceDef->m_type;
		if (fromPieceType != ChessPieceType::PAWN && promoteToPieceGlyph != '?')
		{
			result.m_errorMessage = "Invalid move: cannot promote non-pawn piece";
			return false;
		}
		else {
			s_promoteToPiece = '?';
		}

		//check if the move is valid for FROM piece
		bool isToCoordsValidForFromPiece = m_pieces[pieceIndexFrom].IsValidMoveForCoords(toCoords);
		if (!isToCoordsValidForFromPiece)
		{
			std::string fromPieceInvalidMove = Stringf("Invalid move: %s cannot move to this place by rule", fromChessPiece.c_str());
			result.m_errorMessage = fromPieceInvalidMove;
			return false;
		}

		IntVec2 fromToMoveCase = GetMoveCase(fromCoords, toCoords);
		int fromToDist = GetTaxicabDistance2D(fromCoords, toCoords);


		//check if the from piece is blocked. If is knight, ignore

		if (fromPieceType != ChessPieceType::KNIGHT)
		{
			//check if there is a piece between from and to piece
			IntVec2 tempMoveCase = IntVec2(0, 0);
			int tempDist = 0;

			for (int i = 0; i < pieceNum; i++)
			{
				IntVec2 tempCoords = m_pieces[i].m_currentCoords;
				if (tempCoords == fromCoords || tempCoords == toCoords)
				{//ignore the from piece and to piece
					continue;
				}
				tempMoveCase = GetMoveCase(fromCoords, m_pieces[i].m_currentCoords);
				tempDist = GetTaxicabDistance2D(fromCoords, tempCoords);

				if (tempDist < fromToDist
					&& tempMoveCase == fromToMoveCase
					&& m_pieces[pieceIndexFrom].IsValidMoveForCoords(tempCoords))
				{
					std::string fromPieceBlocked = Stringf("Invalid move: %s is blocked by %s at %s",
						fromChessPiece.c_str(),
						m_pieces[i].m_pieceDef->m_name.c_str(),
						GetSquareCoordsForBoardCoords(m_pieces[i].m_currentCoords).c_str());
					result.m_errorMessage = fromPieceBlocked;
					return false;
				}
			}

			if (fromPieceType == ChessPieceType::PAWN)
			{

				if (fromToMoveCase.x == 0 && toChessPieceGlyph != '.')
				{//something is in to piece
					result.m_errorMessage = "Invalid move: pawn cannot move vertically to capture TO piece";
					return false;
				}

				//pawn promotion
				bool moveNorth = fromToMoveCase.y == 1;
				bool moveSouth = fromToMoveCase.y == -1;
				bool reachEndOfBoard =
					(fromChessPieceGlyph == 'P' && moveNorth && toCoords.y == 7)
					|| (fromChessPieceGlyph == 'p' && moveSouth && toCoords.y == 0);
				if (reachEndOfBoard)
				{
					if (promoteToPieceGlyph == '?' && !ChessMatch::s_autoPromoteToQueen)
					{
						result.m_errorMessage = "Invalid args: moving pawn to far rank without promoteTo args";
						return false;
					}

					if (ChessMatch::s_autoPromoteToQueen)
					{
						result.m_promoteToPieceGlyph = ChessMatch::s_player1Turn ? 'q' : 'Q';
						ChessMatch::s_autoPromoteToQueen = false;
					}
					else {
						result.m_promoteToPieceGlyph = promoteToPieceGlyph;
					}

					result.m_isPromotion = true;

				}


				//capturing
				if (fromToMoveCase.GetTaxicabLength() == 2)
				{
					//en passant
					//check left right to see if there is an opponent pawn
					char opponentPawnGlyph = fromChessPieceGlyph == 'P' ? 'p' : 'P';
					IntVec2 adjCoord(fromCoords.x + fromToMoveCase.x, fromCoords.y);

					char tempGlyph = GetGlyphAtCoords(adjCoord);
					if (tempGlyph == opponentPawnGlyph)
					{
						int tempIndex = GetPieceIndexForCoords(adjCoord);
						int absDistYOp = abs(m_pieces[tempIndex].m_currentCoords.y - m_pieces[tempIndex].m_prevCoords.y);


						if (m_pieces[tempIndex].m_turnLastMoved == m_turnNumber - 1
							&& absDistYOp == 2)
						{
							result.m_isCapturing = true;
							result.m_isEnpassant = true;
							result.m_pieceIndexToErase = tempIndex;
						}
					}

					if (!result.m_isEnpassant && toChessPieceGlyph == '.')
					{//nothing is in the diagonal TO coords
						result.m_errorMessage = "Invalid move: pawn cannot move diagonally to empty TO coords";
						return false;
					}
				}


			}
			else if (fromPieceType == ChessPieceType::KING)
			{//check Kings Apart rule

				IntVec2 nearby8Coords[8];
				toCoords.GetNearby8Coords(nearby8Coords);
				char opponentKingGlyph = fromChessPieceGlyph == 'K' ? 'k' : 'K';
				char tempGlyph = '?';
				for (int i = 0; i < 8; i++)
				{
					tempGlyph = GetGlyphAtCoords(nearby8Coords[i]);
					if (tempGlyph == opponentKingGlyph)
					{
						result.m_errorMessage = "Invalid move: kings cannot be adjacent to each other";
						return false;
					}
				}
				tempMoveCase = IntVec2(0, 0);

				//check castling
				if (fromToDist == 2)
				{

					//if there is a rook in the king's moving direction
					char rookGlyph = m_pieces[pieceIndexFrom].m_player1Side ? 'r' : 'R';
					result.m_rookGlyph = rookGlyph;
					std::vector<int> rookIndexArr = GetIndexArrForGlyph(rookGlyph);
					int rookNum = (int)rookIndexArr.size();

					bool isToPieceEmpty = GetGlyphAtCoords(toCoords) == '.';

					if (!isToPieceEmpty)
					{
						result.m_errorMessage = "Invalid move: cannot perform castling, a piece is between the king and rook";
						return false;
					}

					//check if there is a piece between king's to coords and rook
					int distBWToAndRook = 0;
					IntVec2 rookFromCoords, rookToCoords;
					for (int i = 0; i < rookNum; i++)
					{
						if (m_pieces[rookIndexArr[i]].m_turnLastMoved != 0)
						{
							continue;
						}
						tempMoveCase = GetMoveCase(toCoords, m_pieces[rookIndexArr[i]].m_currentCoords);
						distBWToAndRook = GetTaxicabDistance2D(toCoords, m_pieces[rookIndexArr[i]].m_currentCoords);

						if (tempMoveCase != fromToMoveCase)
						{
							continue;
						}

						IntVec2 tempCoordsBWToAndRook;
						for (int j = 1; j < distBWToAndRook; j++)
						{
							tempCoordsBWToAndRook = toCoords + tempMoveCase;
							if (GetGlyphAtCoords(tempCoordsBWToAndRook) != '.')
							{
								result.m_errorMessage = "Invalid move: cannot perform castling, a piece is between the king and rook";
								return false;
							}

						}

						if (tempMoveCase == fromToMoveCase)
						{
							result.m_isCastling = true;
							result.m_rookFromCoords = m_pieces[rookIndexArr[i]].m_currentCoords;
							result.m_rookToCoords = toCoords - tempMoveCase;
							result.m_rookIndex = rookIndexArr[i];
							break;
						}
					}

					if (!result.m_isCastling)
					{
						result.m_errorMessage = "Invalid move: cannot perform castling, the rook has moved";
						return false;
					}
				}



			}

		}

	}

	

	s_isTeleporting = false;
	result.m_isValidMove = true;

	return true;
}

void ChessMatch::MovePiece(IntVec2 const& fromCoords, IntVec2 const& toCoords)
{
	ChessMoveResult result;
	bool isValid = false;
	if (ChessMatch::s_hasPreDefinedResult)
	{
		result = ChessMatch::s_chessMoveResult;
		isValid = result.m_isValidMove;

		ChessMatch::s_chessMoveResult = ChessMoveResult();
		ChessMatch::s_hasPreDefinedResult = false;
	}
	else {
		isValid = CheckMoveValidity(fromCoords, toCoords, result);
	}


	if (!isValid)
	{
		PrintErrorMsgToConsole(result.m_errorMessage);
		return;
	}

	//if it is my turn and I'm not a spectator
	if (Game::s_isPlayingRemotely && (s_player1Turn == Game::s_myPlayerIndex) && !Game::s_isSpectator)
	{
		//IntVec2 fromFlippedVertically(fromCoords.x, 7 - fromCoords.y);
		//IntVec2 toFlippedVertically(toCoords.x, 7 - toCoords.y);
		//std::string fromStr = GetSquareCoordsForBoardCoords(fromFlippedVertically);
		//std::string toStr = GetSquareCoordsForBoardCoords(toFlippedVertically);

		EventArgs args;
		args.SetValue("from", result.m_fromChessPieceCoordsStr.c_str());
		args.SetValue("to", result.m_toChessPieceCoordsStr.c_str());
		if (s_isTeleporting)
		{
			args.SetValue("teleport", "true");
		}
		if (result.m_isPromotion)
		{
			args.SetValue("promoteTo", "Queen");
		}
		Game::Event_ChessMove(args);
	}
	

	if (result.m_isCastling)
	{
		//move piece on layout
		MovePieceOnLayout(result.m_rookFromCoords, result.m_rookToCoords, result.m_rookGlyph);

		//move piece in vector
		MovePieceInVector(result.m_rookIndex, result.m_rookToCoords);
	}

	//move piece on layout
	MovePieceOnLayout(fromCoords, toCoords, result.m_fromChessPieceGlyph);

	//move piece in vector
	MovePieceInVector(result.m_pieceIndexFrom, toCoords);


	if (result.m_isCapturing)
	{
		std::string capturedPieceStr = result.m_toChessPieceStr;
		std::string coordsString = result.m_toChessPieceCoordsStr;
		if (result.m_isEnpassant)
		{
			capturedPieceStr = m_pieces[result.m_pieceIndexToErase].m_pieceDef->m_name;
			coordsString = GetSquareCoordsForBoardCoords(m_pieces[result.m_pieceIndexToErase].m_currentCoords);
		}
		std::string captureText = Stringf("%s at %s captures %s at %s", 
			result.m_fromChessPieceStr.c_str(), result.m_fromChessPieceCoordsStr.c_str(), capturedPieceStr.c_str(), coordsString.c_str());
		
		if (result.m_isEnpassant)
		{
			captureText.append(": En Passant");
		}
		
		g_theDevConsole->Addline(DevConsole::INFO_MAJOR, captureText);

		if (result.m_toChessPieceGlyph == 'K')
		{
			s_matchEnds = true;
			s_result = 1;

		}
		else if (result.m_toChessPieceGlyph == 'k')
		{
			s_matchEnds = true;
			s_result = 0;
		}

		m_pieces.erase(m_pieces.begin() + result.m_pieceIndexToErase);
	}
	else {
		std::string moveText = Stringf("%s at %s moves to %s",
			result.m_fromChessPieceStr.c_str(), result.m_fromChessPieceCoordsStr.c_str(), result.m_toChessPieceCoordsStr.c_str());

		if (result.m_isCastling)
		{
			moveText.append(": Castling");
		}
		g_theDevConsole->Addline(DevConsole::INFO_MAJOR, moveText);
		
	}

	if (result.m_isPromotion)
	{
		const ChessPieceDefinition* promoteToDef = ChessPieceDefinition::GetByGlyph(result.m_promoteToPieceGlyph);
		GUARANTEE_OR_DIE(promoteToDef != nullptr, "Error: promoteTo piece glyph cannot be found");
		ChangePieceForIndex(result.m_pieceIndexFrom, promoteToDef, toCoords, result.m_promoteToPieceGlyph);

		std::string promoteText = Stringf("%s is promoted to %s",
			result.m_fromChessPieceStr.c_str(), promoteToDef->m_name.c_str());
		g_theDevConsole->Addline(DevConsole::INFO_MAJOR, promoteText);
	}

	
	if (s_matchEnds)
	{
		HandleMatchEnd();
		//de-highlight the pieces
		m_highlightingCoords = IntVec2(-1, -1);
	}
	else {
		m_turnNumber++;
		s_player1Turn = !s_player1Turn;
		m_game->HandleTurnChange();
	}
}

void ChessMatch::HandleMatchEnd()
{
	std::string winningPlayer = "Non-Determined";

	switch (s_result)
	{
	case 0: winningPlayer = App::s_theGame->m_players[0].m_playerName;
		break;
	case 1: winningPlayer = App::s_theGame->m_players[1].m_playerName;
		break;
	case 2: winningPlayer = "No One (Draw)";
		break;
	case -1:
	default:
		break;
	}
	g_theDevConsole->Addline(DevConsole::INFO_MAJOR, "+++++++++++++++++++++++++++++++++++");
	std::string winText = Stringf("Match Ends, the Winner is %s", winningPlayer.c_str());
	g_theDevConsole->Addline(DevConsole::INFO_MAJOR, winText);
	g_theDevConsole->Addline(DevConsole::INFO_MAJOR, "+++++++++++++++++++++++++++++++++++");


	
}

void ChessMatch::ResetMatchData()
{
	ChessMatch::s_formerCoords = IntVec2(0, 0);
	ChessMatch::s_desiredCoords = IntVec2(0, 0);
	ChessMatch::s_promoteToPiece = '?';
	ChessMatch::s_matchEnds = false;
	ChessMatch::s_result = -1;
	ChessMatch::s_player1Turn = false;
	
}

void ChessMatch::SetBoardStateByString(std::string const& layout)
{
	layout.copy(m_layout, 64, 0);
}

void ChessMatch::InitPiecesToMatchBoardState(Player const& player0, Player const& player1)
{
	const ChessPieceDefinition* tempDef = nullptr;
	m_pieces.clear();
	m_pieces.reserve(64);
	int layoutNum = 64;
	IntVec2 tempBoardCoords;
	Texture* tempTexture = m_game->m_pieceTexturePack[0];
	Texture* tempNormalTexture = m_game->m_pieceTexturePack[1];
	Texture* tempSGETexture = m_game->m_pieceTexturePack[2];

	for (int i = 0; i < layoutNum; i++) {
		if (m_layout[i] == '.')
		{
			continue;
		}
		tempDef = ChessPieceDefinition::GetByGlyph(m_layout[i]);
		if (tempDef == nullptr)
		{
			continue;
		}
		tempBoardCoords = GetBoardCoordsForBoardStateIndex(i);

		ChessPiece tempPiece;
		tempPiece.m_currentMatch = this;
		tempPiece.m_pieceDef = tempDef;
		tempPiece.m_moveSFXID = AudioDefinition::GetRandomSFXByType(SFXType::MOVE);
		tempPiece.m_prevCoords = tempBoardCoords;
		tempPiece.m_currentCoords = tempBoardCoords;
		tempPiece.m_position = Vec3(tempBoardCoords.x + 0.5f, tempBoardCoords.y + 0.5f, BOARD_HEIGHT);
		bool player1Side = islower(m_layout[i]) != 0; //if is lower case, returns nonzero
		tempPiece.m_player1Side = player1Side;
		tempPiece.m_color = player1Side ? player1.m_playerColor : player0.m_playerColor;
		tempPiece.m_orientation = player1Side ? EulerAngles(-90, 0, 0) : EulerAngles(90, 0, 0);
		tempPiece.m_texture = tempTexture;
		tempPiece.m_normalTexture = tempNormalTexture;
		tempPiece.m_sgeTexture = tempSGETexture;
		tempPiece.m_numIndices = tempDef->m_numIndices;
		m_pieces.push_back(tempPiece);
	}
}

void ChessMatch::SetPieceToCoords(char piece, IntVec2 const& coords)
{
	int index = GetBoardStateIndexForBoardCoords(coords);
	m_layout[index] = piece;

}


std::string ChessMatch::GetBoardStateAsString() const
{
	return std::string(m_layout);
}

std::string ChessMatch::GetGameStateAsString() const
{
	if (s_matchEnds)
	{
		return std::string("GameOver");
	}
	if (s_player1Turn)
	{
		return std::string("Player2Moving");
	}else
	{
		return std::string("Player1Moving");
	}
	
}

char ChessMatch::GetGlyphAtCoords(IntVec2 const& coords) const
{
	int index = GetBoardStateIndexForBoardCoords(coords);
	if (index < 0 || index > 63)
	{
		return '?';
	}
	return m_layout[index];
}

std::vector<int> ChessMatch::GetIndexArrForGlyph(char const glyph) const
{
	std::vector<int> arr;
	int layoutNum = 64;
	for (int i = 0; i < layoutNum; i++) {
		if (m_layout[i] == '.')
		{
			continue;
		}
		if (m_layout[i] == glyph)
		{
			arr.push_back(i);
		}
	}
	return arr;
}

int ChessMatch::GetPlayerIndexForPieceAtCoords(IntVec2 const& coords) const
{
	char glyph = GetGlyphAtCoords(coords);

	if (glyph == 'P' || glyph == 'R' || glyph == 'N' || glyph == 'B' || glyph == 'Q'||  glyph == 'K')
	{
		return 0;
	}
	if (glyph == 'p' || glyph == 'r' || glyph == 'n' || glyph == 'b' || glyph == 'q' || glyph == 'k')
	{
		return 1;
	}

	return -1;
}

int ChessMatch::GetPieceIndexForCoords(IntVec2 const& coords) const
{
	int numPieces = (int)m_pieces.size();
	for (int i = 0; i < numPieces; i++)
	{
		if (m_pieces[i].m_currentCoords == coords)
		{
			return i;
		}
	}
	return -1;
}

const ChessPiece* ChessMatch::GetPieceAtCoords(IntVec2 const& coords) const
{
	int numPieces = (int)m_pieces.size();
	for (int i = 0; i < numPieces; i++)
	{
		if (m_pieces[i].m_currentCoords == coords)
		{
			return &m_pieces[i];
		}
	}
	return nullptr;
}

const ChessPiece* ChessMatch::GetPieceAtIndex(int index) const
{
	return &m_pieces[index];
}

bool ChessMatch::GetIsPlayer1Turn() const
{
	return s_player1Turn;
}

int ChessMatch::GetTurnNum() const
{
	return m_turnNumber;
}

bool ChessMatch::IsMatchFinished() const
{
	return s_matchEnds;
}

int ChessMatch::GetResult() const
{
	return s_result;
}

IntVec2 ChessMatch::GetMoveCase(IntVec2 const& fromCoords, IntVec2 const& toCoords) const
{
	int distX = toCoords.x - fromCoords.x;
	int distY = toCoords.y - fromCoords.y;

	bool hori = distX != 0;
	bool verti = distY != 0;

	IntVec2 moveCase = IntVec2::ZERO;
	if (hori && !verti)
	{
		moveCase = distX > 0? IntVec2::EAST : IntVec2::WEST;
	}
	else if (!hori && verti)
	{
		moveCase = distY > 0 ? IntVec2::NORTH : IntVec2::SOUTH;
	}
	else if (hori && verti)
	{
		if (distY > 0)
		{
			moveCase = distX > 0 ? IntVec2::NORTHEAST : IntVec2::NORTHWEST;
		}
		else {
			moveCase = distX > 0 ? IntVec2::SOUTHEAST : IntVec2::SOUTHWEST;
		}
	}
	return moveCase;
}

IntVec2 ChessMatch::GetBoardCoordsForWorldPos(Vec3 const& pos) const
{
	IntVec2 coords((int)floorf(pos.x), (int)floorf(pos.y));

	if (coords.x > 7)
	{
		coords.x = 7;
	}
	if (coords.y > 7)
	{
		coords.y = 7;
	}
	return coords;
}

bool ChessMatch::Event_ChessMove(EventArgs& args)
{
	if (args.GetKeyNums() < 2)// && args.GetKeyNums() > 4)
	{
		PrintErrorMsgToConsole("Invalid command args number, Correct format: chessmove from=e2 to=e4");
		return false;
	}

	std::string fromSquareCoords = args.GetValue("from", "");
	if (!IsSquareCoordsValid(fromSquareCoords))
	{
		PrintErrorMsgToConsole("Invalid FROM args format, Correct format: from=e2");
		return false;

	}

	std::string toSquareCoords = args.GetValue("to", "");
	if (!IsSquareCoordsValid(toSquareCoords))
	{
		PrintErrorMsgToConsole("Invalid TO args format, Correct format: to=e4");
		return false;
	}

	if (fromSquareCoords == toSquareCoords)
	{
		PrintErrorMsgToConsole("Invalid format: FROM piece is equal to TO piece, Correct format: chessmove from=e2 to=e4");
		return false;

	}

	std::string promoteToPiece = args.GetValue("promoteTo", "?");
	bool hasPromoteToArgs = promoteToPiece != "?";
	bool invalidPromotePiece = hasPromoteToArgs &&
		!(promoteToPiece == "knight" ||
		promoteToPiece == "bishop" ||
		promoteToPiece == "rook" ||
		promoteToPiece == "queen");
	if (invalidPromotePiece)
	{
		PrintErrorMsgToConsole("Invalid promoteTo args format, Correct format: promoteTo=queen. Can only promote to knight, bishop, rook, or queen");
		return false;
	}

	
	std::string isTeleporting = args.GetValue("teleport", "");
	if (isTeleporting != "" && isTeleporting != "true" && isTeleporting != "false")
	{
		PrintErrorMsgToConsole("Invalid teleport args format, Correct format: teleport=true");
		return false;
	}
	s_isTeleporting = isTeleporting == "true";


	ChessMatch::s_formerCoords = GetBoardCoordsForUserString(fromSquareCoords);
	ChessMatch::s_desiredCoords = GetBoardCoordsForUserString(toSquareCoords);

	if (hasPromoteToArgs)
	{
		const ChessPieceDefinition* def = ChessPieceDefinition::GetByName(promoteToPiece);
		ChessMatch::s_promoteToPiece = s_player1Turn ? def->m_glyphlowerCase : def->m_glyphUpperCase;
	}
	
	return true;
}

bool ChessMatch::Event_Resign(EventArgs& args)
{
	if (s_matchEnds)
	{
		PrintErrorMsgToConsole("Invalid command: Match Ended");
		return true;
	}

	if (args.GetKeyNums() > 0)
	{
		PrintErrorMsgToConsole("Invalid command args number, Correct format: resign");
		return true;
	}

	s_matchEnds = true;
	s_result = s_player1Turn? 0 : 1;
	HandleMatchEnd();

	return true;
}
