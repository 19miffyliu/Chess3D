#pragma once
#include "Game/ChessPiece.hpp"
#include "Game/ChessPieceDefinition.hpp"
#include "Game/Player.hpp"

#include "Engine/Math/IntVec2.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/MathUtils.hpp"

#include <string>
#include <vector>

class Game;


struct ChessMoveResult {

	ChessMoveResult() = default;
	~ChessMoveResult() = default;

	int m_pieceIndexToErase = -1;
	int m_pieceIndexFrom = -1;
	int m_pieceIndexTo = -1;
	std::string m_fromChessPieceStr;
	std::string m_toChessPieceStr;
	std::string m_fromChessPieceCoordsStr;
	std::string m_toChessPieceCoordsStr;
	char m_fromChessPieceGlyph = '.';
	char m_toChessPieceGlyph = '.';

	std::string m_errorMessage = "UNKNOWN ERROR";

	//castling special
	int m_rookIndex = -1;
	IntVec2 m_rookFromCoords;
	IntVec2 m_rookToCoords;
	char m_rookGlyph = '.';

	//promotion special
	char m_promoteToPieceGlyph = '.';

	//special rules
	bool m_isValidMove = false;
	bool m_isCastling = false;
	bool m_isPromotion = false;
	bool m_isEnpassant = false;
	bool m_isCapturing = false;
};


class ChessMatch
{
public:
	ChessMatch();
	ChessMatch(Game* owner, int startingPlayerIndex = 0);
	~ChessMatch();

	void Startup();
	void Update(float deltaSeconds);

	//render
	void Render()const;
	void RenderPlayerSelections()const;

	//player selection
	void UpdatePlayerRaycastHit();
	RaycastResult3D RaycastAgainstAllPieces(Vec3 const& rayStart, Vec3 const& rayFwdNormal, float rayLen);
	RaycastResult3D RaycastAgainstAllBoardSquares(Vec3 const& rayStart, Vec3 const& rayFwdNormal, float rayLen);
	void UpdatePlayerHighlight();

	//input
	void HandlePlayerLeftClick();
	void HandlePlayerRightClick();
	void HandlePlayerTeleporting(bool isTeleporting);

	void UpdateMovePieceCommand();
	void MovePieceOnLayout(IntVec2 const& fromCoords, IntVec2 const& toCoords, char const glyph);
	void MovePieceInVector(int fromPieceIndex, IntVec2 const& toCoords);
	void ChangePieceForIndex(int pieceIndex, const ChessPieceDefinition* def, IntVec2 const& coords, char const glyph);
	bool CheckMoveValidity(IntVec2 const& fromCoords, IntVec2 const& toCoords, ChessMoveResult& result);
	void MovePiece(IntVec2 const& fromCoords, IntVec2 const& toCoords);

	//set function
	void SetBoardStateByString(std::string const& layout);
	void InitPiecesToMatchBoardState(Player const& player0, Player const& player1);
	void SetPieceToCoords(char piece, IntVec2 const& coords);
	void SetTurnNumber(int turnNumber);
	void SetMatchResultAndEndTheGame(int gameResult);

	//get function
	std::string GetBoardStateAsString()const;
	std::string GetGameStateAsString()const;
	char GetGlyphAtCoords(IntVec2 const& coords)const;
	std::vector<int> GetIndexArrForGlyph(char const glyph)const;
	int GetPlayerIndexForPieceAtCoords(IntVec2 const& coords)const;//0 = player 0, 1 = player 1, -1 = empty
	int GetPieceIndexForCoords(IntVec2 const& coords)const;
	const ChessPiece* GetPieceAtCoords(IntVec2 const& coords)const;
	const ChessPiece* GetPieceAtIndex(int index)const;
	bool GetIsPlayer1Turn()const;
	int GetTurnNum()const;
	bool IsMatchFinished()const;
	int GetResult()const;
	IntVec2 GetMoveCase(IntVec2 const& fromCoords, IntVec2 const& toCoords)const;
	IntVec2 GetBoardCoordsForWorldPos(Vec3 const& pos)const;

	static bool Event_ChessMove(EventArgs& args);
	static bool Event_Resign(EventArgs& args);
	static void HandleMatchEnd();
	static void ResetMatchData();

	static ChessMoveResult s_chessMoveResult;
	static bool s_hasPreDefinedResult;
	static IntVec2 s_formerCoords;
	static IntVec2 s_desiredCoords;
	static char s_promoteToPiece;
	static bool s_autoPromoteToQueen;
	static bool s_matchEnds;
	static int s_result; //0 = player 0 wins, 1 = player 1 wins, 2 = draw, -1 = non-determined
	static bool s_player1Turn;
	static bool s_isTeleporting;
	

private:

	Game* m_game = nullptr;
	char m_layout[64] = {};
	int m_turnNumber = 1;

	//player selection
	IntVec2 m_raycastingCoords = IntVec2(-1,-1);
	IntVec2 m_highlightingCoords = IntVec2(-1, -1);
	ChessPiece* m_selectedPiece = nullptr;


	std::vector<ChessPiece> m_pieces;


};

