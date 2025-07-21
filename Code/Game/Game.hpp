#pragma once
#include "Game/GameCommon.hpp"
#include "Game/Entity.hpp"
#include "Game/Player.hpp"
#include "Game/ChessObject.hpp"
#include "Game/ChessMatch.hpp"
#include "Game/ChessBoard.hpp"
#include "Game/Widget.hpp"
#include "Game/Button.hpp"
#include "Game/Slider.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/StaticMeshLoader.hpp"

#include <string>


class Clock;
class Shader;

enum class CameraMode {
	LOCKED,
	FREEFLY,
	COUNT
};

enum class GameState {
	ATTRACT,
	PLAYING,
	GAMEOVER,
	COUNT
};



std::string GetRenderModeStr(int mode);

class Game
{
public:
	Game();
	~Game();

	void Startup();
	void Update();
	void Render() const;
	void Shutdown();

	//attract mode related
	void EnterAttractMode();
	void InitializeAttractLocalVerts();
	void UpdateAttractMode();
	void UpdateProgressBar();

	//widgets
	void InitWidgets();
	void UpdateWidgets(float deltaSeconds);
	void HandleButtonClicked(int type);
	void HandleWidgetBeingPressed(Widget* widget, bool buttonDown);
	void HandleSliderDragged(int type, float newPercent);
	

	//gameplay related
	void StartMatch();
	void ResetMatch();

	//update functions
	void UpdateEntities();
	void UpdateCameras();
	void MoveCamTo(Camera& cam, Vec2 position);
	Mat44 MakeCameraToRenderTransform()const;

	//handle game state
	void EnterState(GameState state);
	void ExitState(GameState state);
	void HandleGameWin();
	void HandleGameLost();
	void HandleTurnChange();
	void PrintMatchOnlineStateBasedOnNetworkState();

	//check input
	void CheckInput();
	bool CheckUIAction(Vec2 const& mousePos, Vec2 const& mouseDelta);
	void PrintKeysToConsole();
	void PrintPlayerTurnToConsole();
	void PrintBoardStateToConsole();

	//render function
	void RenderAttractMode()const;
	void RenderWidgets()const;
	void RenderGameUI()const;
	void DebugRender()const;
	void RenderEntities()const;

	//get function
	bool IsInAttractMode()const;
	float GetGlobalVolume()const;
	Vec2 GetGameDimensions()const;
	Vec2 GetClientDimensions()const;
	std::string GetDefaultBoardState()const;
	Clock* GetGameClock()const;
	Vec3 GetWorldCameraPos()const;
	Vec3 GetWorldCameraFwdNormal()const;
	Player GetPlayerByIndex(int index)const;
	Vec2 GetGamePosition(Vec2 const& clientPos)const;
	Vec2 GetGameDirection(Vec2 const& clientDelta)const;

	//camera
	void UpdateCameraBasedOnInput();
	void UpdateFromKeyboard();
	void UpdateFromController();

	void ToggleCameraMode();
	void SetPosAndOrientationForPlayerTurn();

	
	//audio
	void StopSoundAtIndex(SoundPlaybackID& soundId);
	void LoadRandomBGMForCurrentGame();
	void LoadSFXs();
	void PlayBGMIfNotAlreadyPlaying(SoundID soundId, SoundPlaybackID& soundPlaybackId);
	void RollNewRandomBGM();
	void SetPlayingBGMToVolume();
	void SetBGMVolume(float volume);

	//event
	void SubscribeToLocalCommands();
	void UnsubscribeToLocalCommands();
	void SubscribeToRemoteCommands();
	void UnsubscribeToRemoteCommands();


public://static functions

	//network commands
	static bool Event_ChessServerInfo(EventArgs& args);
	static bool Event_ChessListen(EventArgs& args);
	static bool Event_ChessConnect(EventArgs& args);
	static bool Event_ChessDisconnect(EventArgs& args);
	static bool Event_ChessPlayerInfo(EventArgs& args);
	static bool Event_ChessBegin(EventArgs& args);
	static bool Event_ChessValidate(EventArgs& args);
	static bool Event_ChessMove(EventArgs& args);
	static bool Event_ChessResign(EventArgs& args);
	static bool Event_ChessOfferDraw(EventArgs& args);
	static bool Event_ChessAcceptDraw(EventArgs& args);
	static bool Event_ChessRejectDraw(EventArgs& args);
	static bool Event_RemoteCmd(EventArgs& args);


	static bool SetOutgoingData(std::string const& data);
	static bool SetOutgoingDataToAllSpectators(std::string const& data);
	static bool SendOutgoingDataImmediately(std::string const& data, bool disconnectRightAfter = false);
	static bool SendOutgoingDataImmediatelyToAllSpectators(std::string const& data, bool disconnectRightAfter = false);
	static bool SpectatorCheck(std::string const& actionStr, std::string const& fromServerStr);
	static bool s_isPlayingRemotely;
	static bool s_myPlayerIndex;
	static bool s_isSpectator;
	static float s_SFXVolume;
	static float s_totalAssetNum;

public:
	Texture* m_boardTexturePack[3] = {};
	Texture* m_pieceTexturePack[3] = {};

	//board data
	ChessBoard m_boardModel;

	//how far can player raycast
	float m_playerPointDistance = 5.f;
	Rgba8 m_playerMouseSelectedColor = Rgba8::BLUE;

	//shader
	Shader* m_diffuseShader = nullptr;
	Shader* m_blinnPhongShader = nullptr;

public:
	//definitions
	void LoadFromGameConfig();
	void InitializeDefs();
	void ClearAllDefs();

	//UI element display
	Player m_players[2]; //myself (local) = 0, opponent (remote) = 1
	GameState m_currentState = GameState::ATTRACT;
	GameState m_nextState = GameState::ATTRACT;
	bool m_gameWon = false;
	bool m_gameLost = false;
	bool m_canAcceptOrRejectDraw = false;

	//time management
	Clock* m_gameClock = nullptr;

	//lights
	int m_numLights = 1;
	Light m_spotLights[8];
	Vec3 m_sunDirection = Vec3(1, 0, -1);
	float m_sunIntensity = 0.3f;
	float m_ambientIntensity = 0.25f;
	

	//entity management
	EntityList m_allEntities;
	int m_currentPlayerIndex = 0;
	ChessMatch* m_currentMatch = nullptr;
	std::string m_defaultBoardState;


	//camera
	Camera m_screenCamera;
	Vec2 m_screenCamOffset = Vec2(SCREEN_SIZE_X * 0.5, SCREEN_SIZE_Y * 0.5);
	Vec2 m_gameDimensions = Vec2::ZERO;
	Vec2 m_windowsDimensions = Vec2::ZERO;
	AABB2 m_gameWindow = AABB2::ZERO;

	Camera m_worldCamera;
	CameraMode m_worldCamMode = CameraMode::LOCKED;
	//cursor move 1 pixel = ? degrees in yaw or pitch
	float m_pixelToDegreeRatio = 0.125f;
	//press Q or E for 1 second = ? degrees in roll
	float m_rollDegPerSecond = 90.f;

	//press WASDZC for one second = ? pixel in movement
	float m_moveUnitsPerSecond = 2.f;
	float m_shiftSpeedMultiplier = 10.f;

	Vec3 m_defaultPlayer0CamPos = Vec3(4, -2, 6);
	Vec3 m_defaultPlayer1CamPos = Vec3(4, 10, 6);
	Vec3 m_defaultGameOverCamPos = Vec3(4, 4, 8);
	EulerAngles m_defaultPlayer0CamOri = EulerAngles(90.f, 50.f, 0);
	EulerAngles m_defaultPlayer1CamOri = EulerAngles(-90.f, 50.f, 0);
	EulerAngles m_defaultGameOverCamOri = EulerAngles(0.f, 90.f, 0);
	Mat44 m_cameraToRenderTransform;


	//sound
	float m_musicVolume = 1.f;
	
	SoundID m_BGMID = MISSING_SOUND_ID;
	SoundPlaybackID m_BGMPlayIndex = MISSING_SOUND_ID;
	std::string m_BGMFileName;

	SoundID m_gameEndSFX = MISSING_SOUND_ID;
	SoundID m_gameStartSFX = MISSING_SOUND_ID;
	SoundID m_errorSFX = MISSING_SOUND_ID;
	SoundID m_infoSFX = MISSING_SOUND_ID;

	//UI element display
	std::vector<Widget*> m_widgets;
	Slider* m_progressBar = nullptr;
	FloatRange m_portRange = FloatRange(3000, 3200);
	int m_widgetTypeWhereMousePressed = -1;
	int m_widgetIndexWhenMousePressed = -1;
	Rgba8 m_playerSelectedColor = Rgba8::GRAY;
	Rgba8 m_playerUnselectedColor = Rgba8::WHITE;

	
	//multithreaded asset loading
	std::thread m_assetLoadingThread;
	
	bool m_areAllModelsLoaded = false;

	//mesh loader
	std::string m_meshName;
	Mat44 m_meshTransform;
	StaticMeshLoader m_meshLoader;
	bool m_renderModel = false;

	//state management
	int m_debugRenderMode = 0;
	bool m_debugDraw = false;
	

};

