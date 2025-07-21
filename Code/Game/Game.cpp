#include "Game/Game.hpp"
#include "Game/App.hpp"
#include "Game/ChessPieceDefinition.hpp"
#include "Game/AudioDefinition.hpp"

#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/NetworkSystem/NetworkSystem.hpp"


#include <map>

bool Game::s_isPlayingRemotely = false;
bool Game::s_myPlayerIndex = false;
bool Game::s_isSpectator = false;
float Game::s_SFXVolume = 1.f;
float Game::s_totalAssetNum = 31.f;

std::atomic<bool> g_isAssetLoadingCompleted = false;
std::atomic<int> g_numAssetsLoaded = 0;

Game::Game()
{
	m_gameDimensions = Vec2(SCREEN_SIZE_X,SCREEN_SIZE_Y);
	m_windowsDimensions = Vec2(g_gameconfigBlackBoard.GetValue("screenSizeX", 1600.f), g_gameconfigBlackBoard.GetValue("screenSizeY", 800.f));
	m_gameClock = new Clock(*g_theSysClock);

	//cameras
	m_cameraToRenderTransform = MakeCameraToRenderTransform();
	AABB2 viewport(Vec2::ZERO, m_windowsDimensions); 
	m_gameWindow = AABB2(Vec2(0.f, 0.f), m_gameDimensions);
	m_screenCamera.SetOrthographicView(m_gameWindow.m_mins, m_gameWindow.m_maxs);
	m_screenCamera.SetViewport(viewport);
	DebugSetScreenCamera(m_screenCamera);

	m_worldCamera.SetPerspectiveView(2, 60.f,0.1f,100.f);
	m_worldCamera.SetViewport(viewport);
	m_worldCamera.SetAspect(2);
	m_worldCamera.SetCameraToRenderTransform(m_cameraToRenderTransform);

	//players
	m_players[0] = Player(0, "Server", Rgba8::WHITE);
	m_players[1] = Player(1, "Client", Rgba8::BLACK);
	
	//init shader
	m_diffuseShader = g_theRenderer->CreateOrGetShader("Data/Shaders/Diffuse", VertexType::VERTEX_PCUTBN);
	m_blinnPhongShader = g_theRenderer->CreateOrGetShader("Data/Shaders/BlinnPhong", VertexType::VERTEX_PCUTBN);

	//init defs
	
	m_assetLoadingThread = std::thread(&Game::InitializeDefs, this);
	//InitializeDefs();
	LoadFromGameConfig();

	//add data for spotlights
	m_numLights = 2;

	m_spotLights[0].c_innerRadius = 1;
	m_spotLights[0].c_outerRadius = 3;
	m_spotLights[0].c_lightColor = Vec4(1,0,1,1);
	m_spotLights[0].c_lightWorldPosition = Vec3(1,0,0);
	m_spotLights[0].c_lightFwdNormal = (Vec3(0, 0, -1)).GetNormalized();
	m_spotLights[0].c_innerDotThreshold = -1.f;
	m_spotLights[0].c_outerDotThreshold = -2.f;


	m_spotLights[1].c_innerRadius = 2;
	m_spotLights[1].c_outerRadius = 7;
	m_spotLights[1].c_lightColor = Vec4(0, 1, 1, 1);
	m_spotLights[1].c_lightWorldPosition = Vec3(0, 0, 2);
	m_spotLights[1].c_lightFwdNormal = (Vec3(1, 1, 0)).GetNormalized();
	m_spotLights[1].c_innerDotThreshold = 0.9f;
	m_spotLights[1].c_outerDotThreshold = 0.7f;

	
	SubscribeToLocalCommands();
	SubscribeToRemoteCommands();
	


}

Game::~Game()
{
	delete m_gameClock;
	m_gameClock = nullptr;

	UnsubscribeToLocalCommands();
	UnsubscribeToRemoteCommands();

	m_assetLoadingThread.join();
}

void Game::Startup()
{
	//chess board
	m_boardModel.m_game = this;
	
	

	if (m_meshName != "?" && m_meshName != "")
	{
		m_renderModel = true;
		//m_meshName = "Data/Models/Cube/Cube_vni";
		//m_meshName = "Data/Models/Woman/Woman";
		//m_meshTransform = Mat44::MakeTranslation3D(Vec3(-1,-1,0));
		m_meshLoader.LoadMeshFromFilePath(m_meshName.c_str());
		m_meshLoader.CreateBufferAndTexturesForAllMeshes(g_theRenderer);
	}

	PrintKeysToConsole();
	PlayBGMIfNotAlreadyPlaying(m_BGMID, m_BGMPlayIndex);
	InitWidgets();

	/*
	bool isServer = g_gameconfigBlackBoard.GetValue("isServer", true);
	EventArgs args;
	if (isServer) {
		Event_ChessListen(args);
	}
	else {
		Event_ChessConnect(args);
		s_myPlayerIndex = true;
	}*/

	
	

}

void Game::Update()
{	

	//debug
	float time = (float)m_gameClock->GetTotalSeconds();
	float cosTime = cosf(time);
	float sinTime = sinf(time);
	m_spotLights[0].c_lightWorldPosition = Vec3(4 + 2 * cosTime, 4 + 2 * sinTime, 1);

	m_spotLights[1].c_lightFwdNormal = (Vec3(cosTime, sinTime, -1)).GetNormalized();
	//m_spotLights[1].c_lightColor = Vec4(cosTime, sinTime, cosTime * sinTime, 1);

	Game::s_isPlayingRemotely = g_theNetwork->IsConnected();

	if (g_isAssetLoadingCompleted && !m_areAllModelsLoaded)
	{
		ChessPieceDefinition::CreateBufferForDefinitions();
		m_boardModel.CreateBuffers();
		LoadSFXs();

		g_randGen.SetRandomSeed();
		RollNewRandomBGM();

		m_areAllModelsLoaded = true;
		DebugAddMessage("All assets are loaded! You can now play the game.", 3);
	}
	
	CheckInput();
	if (m_currentState == GameState::ATTRACT){
		UpdateAttractMode();
		
		UpdateWidgets((float)m_gameClock->GetDeltaSeconds());
	}
	else if (m_currentState == GameState::PLAYING || m_currentState == GameState::GAMEOVER)
	{

		//update entities
		UpdateEntities();
		
	}
	//camera should update last
	UpdateCameras();
}

void Game::Render()const
{
	g_theRenderer->SetLightConstants(m_sunDirection.GetNormalized(), m_sunIntensity, m_ambientIntensity, m_numLights, m_spotLights);
	//g_theRenderer->BindShader(m_diffuseShader);
	g_theRenderer->BindShader(m_blinnPhongShader);


	if (m_currentState == GameState::ATTRACT) {
		g_theRenderer->BeginCamera(m_screenCamera);
		RenderAttractMode();
		RenderWidgets();
		g_theRenderer->EndCamera(m_screenCamera);
		
	}
	else if (m_currentState == GameState::PLAYING || m_currentState == GameState::GAMEOVER) {
		//render world size
		g_theRenderer->BeginCamera(m_worldCamera);
		//Render Entities
		RenderEntities();
		if (m_renderModel)
		{
			m_meshLoader.RenderMeshByNameAndModelConstant(m_meshName, m_meshTransform, Rgba8::WHITE);
		}
		g_theRenderer->EndCamera(m_worldCamera);
		//DebugRenderWorld(m_worldCamera);
		if (m_debugDraw)
		{
			DebugRender();
		}
		//render screen size
		g_theRenderer->BeginCamera(m_screenCamera);
		//render UI
		RenderGameUI();
		g_theRenderer->EndCamera(m_screenCamera);
		DebugRenderScreen(m_screenCamera);
	}

	g_theRenderer->BeginCamera(m_screenCamera);
	//render DevConsole
	DebugRenderScreen(m_screenCamera);
	g_theDevConsole->Render(m_gameWindow, nullptr);
	g_theRenderer->EndCamera(m_screenCamera);

}

void Game::Shutdown()
{
	delete m_progressBar;
	m_progressBar = nullptr;

	for (int i = 0; i < (int)m_allEntities.size(); i++) {
		delete m_allEntities[i];
		m_allEntities[i] = nullptr;
	}

	int numWidget = (int)m_widgets.size();
	for (int j = 0; j < numWidget; j++)
	{
		delete m_widgets[j];
		m_widgets[j] = nullptr;
	}

	delete m_currentMatch;
	m_currentMatch = nullptr;

	ClearAllDefs();
}

void Game::EnterAttractMode()
{
	//initialize attract mode vertices
	InitializeAttractLocalVerts();
}

void Game::InitializeAttractLocalVerts()
{
	//set attract local verts

}


void Game::UpdateAttractMode()
{
	//vertex animation in attract mode

}

void Game::InitWidgets()
{
	Vec2 gameScreenDimensions(1600,800);



	//for title

	AABB2 buttonBox(Vec2(), gameScreenDimensions);

	Vec2 buttonDim = gameScreenDimensions * 0.1f;
	Vec2 buttonOffset(0, -buttonDim.y * 1.2f);

	buttonBox.SetDimensions(buttonDim);
	buttonBox.Translate(Vec2(0, buttonDim.y * 2));

	Rgba8 buttonBGColor = Rgba8::WHITE;
	Rgba8 buttonTextColor = Rgba8::BLACK;

	buttonBox.Translate(buttonOffset);
	m_widgets.push_back(new Button(buttonBox, buttonBGColor, "Connect as Client", buttonTextColor, ButtonType::CONNECT_CLIENT));

	buttonBox.Translate(buttonOffset);
	m_widgets.push_back(new Button(buttonBox, buttonBGColor, "Connect as Spectator", buttonTextColor, ButtonType::CONNECT_SPECTATOR));

	//disconnect and change BGM
	AABB2 disconnectBox(buttonBox);
	disconnectBox.Translate(Vec2(-buttonDim.x * 1.2f, 0));
	m_widgets.push_back(new Button(disconnectBox, buttonBGColor, "Disconnect", buttonTextColor, ButtonType::DISCONNECT));

	disconnectBox.Translate(buttonOffset);
	m_widgets.push_back(new Button(disconnectBox, buttonBGColor, "Change BGM", buttonTextColor, ButtonType::SWITCH_BGM));

	buttonBox.Translate(buttonOffset);
	m_widgets.push_back(new Button(buttonBox, buttonBGColor, "Listen As Server", buttonTextColor, ButtonType::LISTEN_SERVER));

	buttonBox.Translate(buttonOffset);
	m_widgets.push_back(new Button(buttonBox, buttonBGColor, "Play", buttonTextColor, ButtonType::CHESS_BEGIN));


	buttonBox.Translate(buttonOffset);
	m_widgets.push_back(new Button(buttonBox, buttonBGColor, "Quit", buttonTextColor, ButtonType::QUIT));

	//for options
	AABB2 optionSliderBox(Vec2(), gameScreenDimensions);

	Vec2 sliderDim(gameScreenDimensions.x * 0.2f, gameScreenDimensions.y * 0.05f);
	Vec2 sliderOffset(0, -sliderDim.y * 1.2f);

	optionSliderBox.SetDimensions(sliderDim);
	optionSliderBox.Translate(Vec2(sliderDim.x,0));

	Rgba8 sliderBGColor = Rgba8::WHITE;
	Rgba8 sliderTextColor = Rgba8::BLACK;


	optionSliderBox.Translate(-0.5f * sliderOffset);
	m_widgets.push_back(new Slider(optionSliderBox, SliderType::MUSIC, m_musicVolume));

	optionSliderBox.Translate(1.f * sliderOffset);
	m_widgets.push_back(new Slider(optionSliderBox, SliderType::SFX, s_SFXVolume));

	//IP address
	optionSliderBox.Translate(1.5f * sliderOffset);
	m_widgets.push_back(new Slider(optionSliderBox, SliderType::IP_ADDR_1, 127.f / 255,FloatRange(0,255),true));
	optionSliderBox.Translate(1.f * sliderOffset);
	m_widgets.push_back(new Slider(optionSliderBox, SliderType::IP_ADDR_2, 0, FloatRange(0, 255), true));
	optionSliderBox.Translate(1.f * sliderOffset);
	m_widgets.push_back(new Slider(optionSliderBox, SliderType::IP_ADDR_3, 0, FloatRange(0, 255), true));
	optionSliderBox.Translate(1.f * sliderOffset);
	m_widgets.push_back(new Slider(optionSliderBox, SliderType::IP_ADDR_4, 1.f / 255, FloatRange(0, 255), true));


	//port 
	optionSliderBox.Translate(1.f * sliderOffset);
	float port = (float)g_theNetwork->GetCurrentListeningPort();
	float portPercent = GetFractionWithinRange(port, m_portRange.m_min, m_portRange.m_max);
	m_widgets.push_back(new Slider(optionSliderBox, SliderType::PORT_NUMBER, portPercent, m_portRange, true));

	int numWidget = (int)m_widgets.size();
	for (int j = 0; j < numWidget; j++)
	{
		m_widgets[j]->Startup();
		m_widgets[j]->SetTextColor(Rgba8::WHITE);
	}

	//loading progress bar
	AABB2 loadingSliderBox(Vec2(), Vec2(gameScreenDimensions.x * 0.75f, gameScreenDimensions.y * 0.05f));
	m_progressBar = new Slider(loadingSliderBox, SliderType::LOADING_PROGRESS, 0, FloatRange(0, s_totalAssetNum), true);
	m_progressBar->m_isClickable = false;
	
}

void Game::UpdateWidgets(float deltaSeconds)
{
	UpdateProgressBar();

	if (m_currentState == GameState::ATTRACT)
	{
		int numWidget = (int)m_widgets.size();
		for (int i = 0; i < numWidget; i++)
		{
			m_widgets[i]->Update(deltaSeconds);
		}
	}
	
}

void Game::HandleButtonClicked(int type)
{
	ButtonType buttonType = (ButtonType)type;
	EventArgs args;

	switch (buttonType)
	{
	case ButtonType::CONNECT_CLIENT:
	{
		std::string text = Stringf("Connecting...");
		DebugAddMessage(text, 2.f);
		Event_ChessConnect(args);
		break;
	}
	case ButtonType::CONNECT_SPECTATOR:
	{
		args.SetValue("isSpectator", "true");
		if (g_theNetwork->IsConnected())
		{
			g_theNetwork->Disconnect();
		}
		std::string text = Stringf("Connecting as spectator...");
		DebugAddMessage(text, 2.f);
		Event_ChessConnect(args);
		break;
	}
	case ButtonType::LISTEN_SERVER:
	{
		std::string text = Stringf("Start listening as server...");
		DebugAddMessage(text, 2.f);
		Event_ChessListen(args);
		break;
	}
	case ButtonType::DISCONNECT:
	{
		Event_ChessDisconnect(args);
		break;
	}
	case ButtonType::SWITCH_BGM:
	{
		if(m_areAllModelsLoaded)RollNewRandomBGM();
		break;
	}
	case ButtonType::CHESS_BEGIN:
	{
		Event_ChessBegin(args);
		break;
	}
	case ButtonType::QUIT:
	{
		g_theApp->HandleQuitRequested();
		break;
	}
	case ButtonType::COUNT:
	case ButtonType::INVALID:
	default:
		break;
	}
}

void Game::HandleWidgetBeingPressed(Widget* widget, bool buttonDown)
{
	if (buttonDown)
	{
		widget->SetBGColor(m_playerSelectedColor);
	}
	else {
		widget->SetBGColor(m_playerUnselectedColor);
	}
}

void Game::HandleSliderDragged(int type, float newPercent)
{
	SliderType sliderType = (SliderType)type;

	int zeroTo255 = DenormalizeByte(newPercent);
	std::string IPSegment = std::to_string(zeroTo255);

	float portRange = m_portRange.GetSpan();
	unsigned short port = (unsigned short)roundf((newPercent * portRange) + m_portRange.m_min);

	std::string currentIPAddr = g_theNetwork->GetCurrentIPAddress();
	size_t firstDotIndex = currentIPAddr.find_first_of('.');
	std::string stringWOFirstSeg = currentIPAddr.substr(firstDotIndex);

	std::string stringWOSecondSeg1 = currentIPAddr.substr(0, firstDotIndex + 1);
	std::string stringWOSecondSeg2 = stringWOFirstSeg.substr(1);
	stringWOSecondSeg2 = stringWOSecondSeg2.substr(stringWOSecondSeg2.find_first_of('.'));
	
	size_t lastDotIndex = currentIPAddr.find_last_of('.');
	

	switch (sliderType)
	{
	case SliderType::MUSIC:
		SetBGMVolume(newPercent);
		break;
	case SliderType::SFX:
		s_SFXVolume = newPercent;
		break;
	case SliderType::IP_ADDR_1:
	{
		currentIPAddr = IPSegment + stringWOFirstSeg;
		break;
	}
	case SliderType::IP_ADDR_2:
	{
		std::string copyStr(stringWOSecondSeg1);
		copyStr.append(IPSegment);
		copyStr.append(stringWOSecondSeg2);
		currentIPAddr = copyStr;
		break;
	}
	case SliderType::IP_ADDR_3:
	{
		std::string stringWOThirdSeg2 = currentIPAddr.substr(lastDotIndex);
		std::string stringWOThirdSeg1 = currentIPAddr.substr(0, lastDotIndex);
		size_t middleDotIndex = stringWOThirdSeg1.find_last_of('.');
		stringWOThirdSeg1 = currentIPAddr.substr(0, middleDotIndex + 1);

		std::string copyStr(stringWOThirdSeg1);
		copyStr.append(IPSegment);
		copyStr.append(stringWOThirdSeg2);
		currentIPAddr = copyStr;
		break;
	}
	case SliderType::IP_ADDR_4:
	{
		std::string stringWOFouthSeg = currentIPAddr.substr(0, lastDotIndex + 1);
		currentIPAddr = stringWOFouthSeg + IPSegment;
		break;
	}
	case SliderType::PORT_NUMBER:
	{
		g_theNetwork->SetServerListeningPort(port);
		break;
	}
	case SliderType::INVALID:
	case SliderType::COUNT:
	default:
		break;
	}

	
	g_theNetwork->SetServerIPAddr(currentIPAddr);


}

void Game::UpdateProgressBar()
{
	int numAssets = g_numAssetsLoaded;
	float newPercent = numAssets / s_totalAssetNum;
	m_progressBar->SetSliderVal(newPercent);

	Rgba8 color;
	color.r = DenormalizeByte(1 - newPercent);
	color.g = DenormalizeByte(newPercent);
	color.b = 0;
	m_progressBar->m_nameTextColor = color;
}

void Game::StartMatch()
{
	
	m_currentMatch = new ChessMatch(this, m_currentPlayerIndex);
	PrintMatchOnlineStateBasedOnNetworkState();
	SetPosAndOrientationForPlayerTurn();
	m_currentMatch->InitPiecesToMatchBoardState(m_players[0],m_players[1]);
}

void Game::ResetMatch()
{
	m_gameWon = false;
	m_gameLost = false;
	m_canAcceptOrRejectDraw = false;
	m_debugDraw = false;
	
	m_gameClock->SetTimeScale(1.0);
	m_gameClock->Reset();

	ChessMatch::ResetMatchData();

	delete m_currentMatch;
	m_currentMatch = nullptr;

}

void Game::EnterState(GameState state)
{
	if (state == GameState::ATTRACT)
	{
		ResetMatch();
		g_theAudio->StartSound(m_gameEndSFX, false, s_SFXVolume);
	}
	else if (state == GameState::PLAYING)
	{
		g_theAudio->StartSound(m_gameStartSFX, false, s_SFXVolume);
		PrintPlayerTurnToConsole();
		PrintBoardStateToConsole();
		StartMatch();
	}
	m_nextState = state;
}

void Game::ExitState(GameState state)
{
	m_nextState = (GameState)((unsigned int)state - 1);
}

void Game::HandleGameWin()
{
	m_gameWon = true;

}

void Game::HandleGameLost()
{
	m_gameLost = true;
	
}

void Game::HandleTurnChange()
{
	m_canAcceptOrRejectDraw = false;
	m_currentPlayerIndex = (int)m_currentMatch->GetIsPlayer1Turn();
	if (m_worldCamMode == CameraMode::LOCKED)
	{
		SetPosAndOrientationForPlayerTurn();
	}
	PrintPlayerTurnToConsole();
	PrintBoardStateToConsole();
}

void Game::PrintMatchOnlineStateBasedOnNetworkState()
{
	if (App::s_theGame->m_currentMatch == nullptr)
	{
		return;
	}
	std::string onoffline = Game::s_isPlayingRemotely ? "online" : "offline";
	std::string infoStr = Stringf("Starting the match in %s mode...", onoffline.c_str());
	PrintInfoMsgToConsole(infoStr);
}

void Game::StopSoundAtIndex(SoundPlaybackID& soundPlaybackId)
{
	if (soundPlaybackId != MISSING_SOUND_ID) {
		g_theAudio->StopSound(soundPlaybackId);
		soundPlaybackId = MISSING_SOUND_ID;
	}
}

void Game::LoadRandomBGMForCurrentGame()
{
	StopSoundAtIndex(m_BGMPlayIndex);
	m_BGMID = AudioDefinition::GetRandomBGM();
	m_BGMFileName = AudioDefinition::GetAudioDefBySoundID(m_BGMID)->m_fileName;
}

void Game::LoadSFXs()
{
	m_gameEndSFX = AudioDefinition::GetRandomSFXByType(SFXType::GAMEOVER);
	m_gameStartSFX = AudioDefinition::GetRandomSFXByType(SFXType::GAMESTART);
	m_errorSFX = AudioDefinition::GetRandomSFXByType(SFXType::ERRORMSG);
	m_infoSFX = AudioDefinition::GetRandomSFXByType(SFXType::INFOMSG);
}

void Game::PlayBGMIfNotAlreadyPlaying(SoundID soundId, SoundPlaybackID& soundPlaybackId)
{
	if (soundPlaybackId == MISSING_SOUND_ID)
	{
		soundPlaybackId = g_theAudio->StartSound(soundId, true, m_musicVolume);
	}
}

void Game::RollNewRandomBGM()
{
	LoadRandomBGMForCurrentGame();
	PlayBGMIfNotAlreadyPlaying(m_BGMID, m_BGMPlayIndex);
}

void Game::SetPlayingBGMToVolume()
{
	if (m_BGMPlayIndex != MISSING_SOUND_ID)
	{
		g_theAudio->SetSoundPlaybackVolume(m_BGMPlayIndex, m_musicVolume);
	}
}

void Game::LoadFromGameConfig()
{
	XmlDocument document;
	XmlError result = document.LoadFile("Data/GameConfig.xml");
	if (result != 0)
	{
		ERROR_AND_DIE("Game::LoadFromGameConfig: Error loading file");
	}
	XmlElement* rootElement = document.RootElement();

	m_musicVolume = ParseXmlAttribute(*rootElement, "globalVolume", 1.f);
	m_defaultBoardState = ParseXmlAttribute(*rootElement, "defaultBoardState", "RNBQKBNRPPPPPPPP................................pppppppprnbqkbnr");
	
	std::string texturePath = "Data/Images/";
	std::string boardDT = ParseXmlAttribute(*rootElement, "chessBoardDiffuseTexture", "?");
	if (boardDT != "?")
	{
		m_boardTexturePack[0] = g_theRenderer->CreateOrGetTextureFromFile((texturePath + boardDT).c_str());
	}
	std::string boardNT = ParseXmlAttribute(*rootElement, "chessBoardNormalTexture", "?");
	if (boardNT != "?")
	{
		m_boardTexturePack[1] = g_theRenderer->CreateOrGetTextureFromFile((texturePath + boardNT).c_str());
	}

	std::string boardST = ParseXmlAttribute(*rootElement, "chessBoardSGETexture", "?");
	if (boardST != "?")
	{
		m_boardTexturePack[2] = g_theRenderer->CreateOrGetTextureFromFile((texturePath + boardST).c_str());
	}

	std::string pieceDT = ParseXmlAttribute(*rootElement, "chessPieceDiffuseTexture", "?");
	if (pieceDT != "?")
	{
		m_pieceTexturePack[0] = g_theRenderer->CreateOrGetTextureFromFile((texturePath + pieceDT).c_str());
	}
	std::string pieceNT = ParseXmlAttribute(*rootElement, "chessPieceNormalTexture", "?");
	if (pieceNT != "?")
	{
		m_pieceTexturePack[1] = g_theRenderer->CreateOrGetTextureFromFile((texturePath + pieceNT).c_str());
	}
	std::string pieceST = ParseXmlAttribute(*rootElement, "chessPieceSGETexture", "?");
	if (pieceST != "?")
	{
		m_pieceTexturePack[2] = g_theRenderer->CreateOrGetTextureFromFile((texturePath + pieceST).c_str());
	}


	//model loading
	std::string modelFileName = ParseXmlAttribute(*rootElement, "modelFileName", "?");
	if (modelFileName != "?")
	{
		m_meshName = modelFileName;
	}

	Vec3 modelPos = ParseXmlAttribute(*rootElement, "modelTransform", Vec3::ZERO);
	if (modelPos != Vec3::ZERO)
	{
		m_meshTransform = Mat44::MakeTranslation3D(modelPos);
	}

}

void Game::InitializeDefs()
{
	AudioDefinition::InitializeDefinitions("Data/Definitions/AudioDefinitions.xml");
	m_boardModel.InitializeLocalVerts();
	g_numAssetsLoaded++;
	ChessPieceDefinition::InitializeDefinitions("Data/Definitions/ChessPieceDefinitions.xml");
	g_isAssetLoadingCompleted = true;
}

void Game::ClearAllDefs()
{
	ChessPieceDefinition::ClearDefinitions();
	AudioDefinition::ClearDefinitions();
}


void Game::CheckInput()
{
	

	XboxController const& controller = g_theInput->GetController(0);
	//pressing Space Bar, the ‘N’ key, or the controller Start button or controller A button
	if (m_currentState == GameState::ATTRACT) {
		
		if (g_theInput->WasKeyJustPressed('N') || g_theInput->WasKeyJustPressed(' ')|| controller.IsButtonDown(XBOX_BUTTON_A) || controller.IsButtonDown(XBOX_BUTTON_START)) {
			if (s_isPlayingRemotely)
			{
				EventArgs args;
				Event_ChessBegin(args);
			}
			else {
				
				EnterState(GameState::PLAYING);
			}
			
		}
		else {
			if (g_theInput->IsKeyDown(KEYCODE_LEFT_MOUSE) || g_theInput->WasKeyJustReleased(KEYCODE_LEFT_MOUSE))
			{
				Vec2 mousePos = GetGamePosition(g_theInput->GetCursorClientPosition());
				Vec2 mouseDelta = GetGameDirection(g_theInput->GetCursorClientDelta());
				CheckUIAction(mousePos, mouseDelta);
			}
		}
		

		if (g_theInput->WasKeyJustPressed(KEYCODE_ESC) || controller.WasButtonJustPressed(XBOX_BUTTON_BACK)) {
			g_theApp->HandleQuitRequested();
		}

		
	}
	else if (m_currentState == GameState::PLAYING || m_currentState == GameState::GAMEOVER) {
		//if pressing esc, goes back to attract mode
		if (g_theInput->WasKeyJustPressed(KEYCODE_ESC) || controller.WasButtonJustPressed(XBOX_BUTTON_BACK)) {
			if (s_isPlayingRemotely)
			{
				EventArgs args;
				args.SetValue("reason", "quitting_match");
				Event_ChessDisconnect(args);
			}
			else {
				EnterState(GameState::ATTRACT);
			}
		}
		
		//if pressing LMB, select piece
		if (g_theInput->WasKeyJustPressed(KEYCODE_LEFT_MOUSE)) {
			m_currentMatch->HandlePlayerLeftClick();
		}
		//if pressing RMB, de-select piece
		if (g_theInput->WasKeyJustPressed(KEYCODE_RIGHT_MOUSE)) {
			m_currentMatch->HandlePlayerRightClick();
		}

		//if holding ctrl, teleporting
		if (g_theInput->IsKeyDown(KEYCODE_LCTRL)) {
			m_currentMatch->HandlePlayerTeleporting(true);
		}
		else {
			m_currentMatch->HandlePlayerTeleporting(false);
		}


		if (g_theInput->WasKeyJustPressed('T')|| g_theInput->WasKeyJustPressed(KEYCODE_SHIFT)|| controller.WasButtonJustPressed(XBOX_BUTTON_LSHOULDER))
		{
			m_gameClock->SetTimeScale(0.1);


		}

		if (g_theInput->WasKeyJustReleased('T') || g_theInput->WasKeyJustReleased(KEYCODE_SHIFT) || controller.WasButtonJustReleased(XBOX_BUTTON_LSHOULDER))
		{
			m_gameClock->SetTimeScale(1.0);

		}


		if (g_theInput->WasKeyJustPressed('O'))
		{
			m_gameClock->StepSingleFrame();


		}

		if (g_theInput->WasKeyJustPressed('P'))
		{
			 m_gameClock->TogglePause();
		}

		//if pressing f2, decrease the sun direction x-component by 1 
		if (g_theInput->WasKeyJustPressed(KEYCODE_F2)) {
			m_sunDirection.x -= 1;
			std::string text = Stringf("Sun Direction X: %f", m_sunDirection.x);
			DebugAddMessage(text, 2.f);
		}
		//if pressing f3, increase the sun direction x-component by 1 
		if (g_theInput->WasKeyJustPressed(KEYCODE_F3)) {
			m_sunDirection.x += 1;
			std::string text = Stringf("Sun Direction X: %f", m_sunDirection.x);
			DebugAddMessage(text, 2.f);
		}
		//if pressing f4, decrease the sun direction y-component by 1 
		if (g_theInput->WasKeyJustPressed(KEYCODE_F5)) {
			m_sunDirection.y -= 1;
			std::string text = Stringf("Sun Direction Y: %f", m_sunDirection.y);
			DebugAddMessage(text, 2.f);
		}
		//if pressing f5, increase the sun direction y-component by 1 
		if (g_theInput->WasKeyJustPressed(KEYCODE_F6)) {
			m_sunDirection.y += 1;
			std::string text = Stringf("Sun Direction Y: %f", m_sunDirection.y);
			DebugAddMessage(text, 2.f);
		}
		//if pressing f6, decrease the sun intensity by 0.05 
		if (g_theInput->WasKeyJustPressed(KEYCODE_F7)) {
			m_sunIntensity -= 0.05f;
			m_sunIntensity = GetClamped(m_sunIntensity, 0.f, 1.f);
			std::string text = Stringf("Sun Intensity: %f", m_sunIntensity);
			DebugAddMessage(text, 2.f);
		}
		//if pressing f7, increase the sun intensity by 0.05 
		if (g_theInput->WasKeyJustPressed(KEYCODE_F8)) {
			m_sunIntensity += 0.05f;
			m_sunIntensity = GetClamped(m_sunIntensity, 0.f, 1.f);
			std::string text = Stringf("Sun Intensity: %f", m_sunIntensity);
			DebugAddMessage(text, 2.f);
		}
		//if pressing f8, decrease the ambient intensity by 0.05 
		if (g_theInput->WasKeyJustPressed(KEYCODE_F9)) {
			m_ambientIntensity -= 0.05f;
			m_ambientIntensity = GetClamped(m_ambientIntensity, 0.f, 1.f);
			std::string text = Stringf("Ambient Intensity: %f", m_ambientIntensity);
			DebugAddMessage(text, 2.f);
		}
		//if pressing f9, increase the ambient intensity by 0.05 
		if (g_theInput->WasKeyJustPressed(KEYCODE_F10)) {
			m_ambientIntensity += 0.05f;
			m_ambientIntensity = GetClamped(m_ambientIntensity, 0.f, 1.f);
			std::string text = Stringf("Ambient Intensity: %f", m_ambientIntensity);
			DebugAddMessage(text, 2.f);
		}


		//pressing F1 enable debug render
		if (g_theInput->WasKeyJustPressed(KEYCODE_F1)) {
			m_debugDraw = !m_debugDraw;
		}

		if (g_theInput->WasKeyJustPressed('R')) {//default
			m_debugRenderMode = 0;
		}
		if (g_theInput->WasKeyJustPressed('1')) {//vertex color
			m_debugRenderMode = 1;
		}
		if (g_theInput->WasKeyJustPressed('2')) {//model constant color
			m_debugRenderMode = 2;
		}
		if (g_theInput->WasKeyJustPressed('3')) {//UV
			m_debugRenderMode = 3;
		}
		if (g_theInput->WasKeyJustPressed('4')) {//Model Tangent
			m_debugRenderMode = 4;
		}
		if (g_theInput->WasKeyJustPressed('5')) {//Model Bitangent
			m_debugRenderMode = 5;
		}
		if (g_theInput->WasKeyJustPressed('6')) {//Model Normal
			m_debugRenderMode = 6;
		}
		if (g_theInput->WasKeyJustPressed('7')) {//Diffuse texture color
			m_debugRenderMode = 7;
		}
		if (g_theInput->WasKeyJustPressed('8')) {//Normal texture color
			m_debugRenderMode = 8;
		}
		if (g_theInput->WasKeyJustPressed('9')) {//Pixel Normal TBN Space
			m_debugRenderMode = 9;
		}
		if (g_theInput->WasKeyJustPressed('0')) {//Pixel Normal World Space
			m_debugRenderMode = 10;
		}
		if (g_theInput->WasKeyJustPressed(KEYCODE_NUMPAD_1)) {//lit no normal map
			m_debugRenderMode = 11;
		}
		if (g_theInput->WasKeyJustPressed(KEYCODE_NUMPAD_2)) {//light intensity
			m_debugRenderMode = 12;
		}
		if (g_theInput->WasKeyJustPressed(KEYCODE_NUMPAD_3)) {//unused
			m_debugRenderMode = 13;
		}
		if (g_theInput->WasKeyJustPressed(KEYCODE_NUMPAD_4)) {//World Tangent
			m_debugRenderMode = 14;
		}
		if (g_theInput->WasKeyJustPressed(KEYCODE_NUMPAD_5)) {//World Bitangent
			m_debugRenderMode = 15;
		}
		if (g_theInput->WasKeyJustPressed(KEYCODE_NUMPAD_6)) {//World Normal
			m_debugRenderMode = 16;
		}
		if (g_theInput->WasKeyJustPressed(KEYCODE_NUMPAD_7)) {//modelIIBasisWorld
			m_debugRenderMode = 17;
		}
		if (g_theInput->WasKeyJustPressed(KEYCODE_NUMPAD_8)) {//modelIJBasisWorld
			m_debugRenderMode = 18;
		}
		if (g_theInput->WasKeyJustPressed(KEYCODE_NUMPAD_9)) {//modelIKBasisWorld
			m_debugRenderMode = 19;
		}
		if (g_theInput->WasKeyJustPressed(KEYCODE_NUMPAD_0)) {//unused
			m_debugRenderMode = 20;
		}

		
		//pressing F4 toggle camera mode
		if (g_theInput->WasKeyJustPressed(KEYCODE_F4)) {
			ToggleCameraMode();
		}
	}
	


	m_currentState = m_nextState;

}

bool Game::CheckUIAction(Vec2 const& mousePos, Vec2 const& mouseDelta)
{
	int numWidget = (int)m_widgets.size();
	bool isInsideAWidget = false;
	bool wasJustPressed = g_theInput->WasKeyJustPressed(KEYCODE_LEFT_MOUSE);
	bool wasJustReleased = g_theInput->WasKeyJustReleased(KEYCODE_LEFT_MOUSE);
	bool isHolding = !wasJustPressed && !wasJustReleased;

	for (int i = 0; i < numWidget; i++)
	{
		isInsideAWidget = m_widgets[i]->IsBeingClickedOn(mousePos);
		if (isInsideAWidget && m_widgetIndexWhenMousePressed == -1 && wasJustPressed)
		{
			int type = m_widgets[i]->GetUniqueWidgetType();
			m_widgetTypeWhereMousePressed = type;
			m_widgetIndexWhenMousePressed = i;

			HandleWidgetBeingPressed(m_widgets[i], true);
		}

		//handle event when mouse is release for button, and is holding for sliders
		if (i == m_widgetIndexWhenMousePressed)
		{
			if (m_widgets[i]->m_widgetType == WidgetType::BUTTON && wasJustReleased)
			{
				HandleButtonClicked(m_widgetTypeWhereMousePressed);
			}
			else if (m_widgets[i]->m_widgetType == WidgetType::SLIDER && isHolding)
			{
				float newPercent = m_widgets[i]->DragByOffset(mouseDelta);
				HandleSliderDragged(m_widgetTypeWhereMousePressed, newPercent);
			}
			if (wasJustReleased)
			{
				HandleWidgetBeingPressed(m_widgets[i], false);
				m_widgetTypeWhereMousePressed = -1;
				m_widgetIndexWhenMousePressed = -1;
			}
		}



	}

	return false;
}


void Game::PrintKeysToConsole()
{
	std::string fileString;
	std::string fileName("Data/Controls/ControlKeys.txt");
	FileReadToString(fileString, fileName);

	std::string delimiter("[Debug Controls]\r");
	Strings paragraphs = SplitStringOnString(fileString, delimiter);

	Strings gameplayControls = SplitStringOnDelimiter(paragraphs[0], '\n', false);
	Strings debugControls = SplitStringOnDelimiter(paragraphs[1], '\n', false);
	debugControls[0] = "[Debug Controls]";

	Rgba8 color = DevConsole::INFO_HINT;
	g_theDevConsole->AddParagraph(color, gameplayControls);
	//g_theDevConsole->AddParagraph(color, debugControls);

}

void Game::PrintPlayerTurnToConsole()
{
	Strings playerTurnParagraph;

	std::string divideLine("=========================");
	std::string whichPlayer = m_currentPlayerIndex == 0 ?  "0:(White)": "1(Black)";
	std::string playerTurn = Stringf("Player %s -- it's your move!", whichPlayer.c_str());

	playerTurnParagraph.push_back(divideLine);
	playerTurnParagraph.push_back(playerTurn);

	Rgba8 color(200, 120, 20);
	g_theDevConsole->AddParagraph(color, playerTurnParagraph);
}

void Game::PrintBoardStateToConsole()
{
	Strings boardState;

	std::string whichPlayer = m_players[m_currentPlayerIndex].m_playerName;
	int turnNum = m_currentMatch == nullptr ? 1 : m_currentMatch->GetTurnNum();
	std::string playerTurn = Stringf("Game State: Turn #%d, %s's Turn", turnNum, whichPlayer.c_str());
	boardState.push_back(playerTurn);

	std::string columnLabel1("  ABCDEFGH");
	std::string columnLabel2(" +--------+");

	boardState.push_back(columnLabel1);
	boardState.push_back(columnLabel2);


	std::string boardCurrentLayout = m_currentMatch == nullptr ? m_defaultBoardState : m_currentMatch->GetBoardStateAsString();
	std::string boardLayoutByLine;
	int rowNum = 0;
	for (int i = 7; i > -1; i--) {
		boardLayoutByLine = boardCurrentLayout.substr(i*8, 8);
		rowNum = i + 1;
		std::string currentLine = Stringf("%d|%s|%d", rowNum, boardLayoutByLine.c_str(),rowNum);
		boardState.push_back(currentLine);
	}

	boardState.push_back(columnLabel2);
	boardState.push_back(columnLabel1);

	Rgba8 color(120,160,200);
	g_theDevConsole->AddParagraph(color, boardState);
}



void Game::RenderAttractMode() const
{
	
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->BindTexture(nullptr);
	//change radius and thickness over time
	float timeAsT = static_cast<float>(m_gameClock->GetTotalSeconds());
	float radiusVariation = 30.f * sinf(0.5f * timeAsT) + 200.f;
	float thicknessVariation = 10.f * sinf(5.f * timeAsT) + 15.f;
	DebugDrawRing(Vec2(800.f, 400.f), radiusVariation, thicknessVariation, Rgba8(234, 123, 12));


	//print IP address and port number
	std::vector<Vertex_PCU> tempVerts;
	g_theRenderer->BindTexture(&g_theFont->GetTexture());

	Vec2 topLine(m_gameDimensions.x, m_gameDimensions.y);
	Vec2 bottomLine(0, topLine.y - TEXT_HEIGHT_LARGE);
	AABB2 infoBox(bottomLine, topLine);
	Vec2 infoBoxOffset(0, -TEXT_HEIGHT_LARGE);

	//game time info
	float ds = (float)m_gameClock->GetDeltaSeconds();
	float FPS = ds == 0 ? 0 : 1.f / ds;
	std::string timeInfo =
		Stringf("Time: %.02f FPS: %.02f Scale: %.02f",
			m_gameClock->GetTotalSeconds(), FPS,
			m_gameClock->GetTimeScale());
	g_theFont->AddVertsForTextInBox2D(tempVerts, timeInfo, infoBox, TEXT_HEIGHT, Rgba8::WHITE, 1.f, Vec2(1, 1));

	infoBox.m_mins.y -= TEXT_HEIGHT;
	infoBox.m_maxs.y -= TEXT_HEIGHT;

	//render BGM info
	std::string BGMName = m_areAllModelsLoaded ? m_BGMFileName.c_str() : "Loading...";
	std::string BGMInfo = Stringf("BGM: %s", BGMName.c_str());
	g_theFont->AddVertsForTextInBox2D(tempVerts, BGMInfo, infoBox, TEXT_HEIGHT, Rgba8::WHITE, 1.f, Vec2(1, 1));

	
	infoBox.Translate(infoBoxOffset * 2.f);
	AABB2 ipBox = infoBox;
	std::string IPInfo = Stringf("IP Address: %s",g_theNetwork->GetCurrentIPAddress().c_str());
	g_theFont->AddVertsForTextInBox2D(tempVerts, IPInfo, ipBox, TEXT_HEIGHT_LARGE);

	infoBox.Translate(infoBoxOffset);
	std::string portInfo = Stringf("Port Number: %d", g_theNetwork->GetCurrentListeningPort());
	g_theFont->AddVertsForTextInBox2D(tempVerts, portInfo, infoBox, TEXT_HEIGHT_LARGE);

	infoBox.Translate(infoBoxOffset);
	std::string stateInfo = g_theNetwork->GetConnectionStateString();
	size_t firstComma = stateInfo.find_first_of(',');
	std::string networkInfo = stateInfo.substr(0, firstComma);
	g_theFont->AddVertsForTextInBox2D(tempVerts, networkInfo, infoBox, TEXT_HEIGHT_LARGE);

	

	g_theRenderer->DrawVertexArray(tempVerts);




}

void Game::RenderWidgets() const
{
	g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);

	int numWidget = (int)m_widgets.size();
	for (int i = 0; i < numWidget; i++)
	{
		m_widgets[i]->Render();
	}

	m_progressBar->Render();
}

void Game::RenderGameUI() const
{
	g_theRenderer->BeginCamera(m_screenCamera);

	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetDepthMode(DepthMode::READ_ONLY_LESS_EQUAL);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);

	g_theRenderer->SetModelConstants();
	std::vector<Vertex_PCU> UIVerts;
	AABB2 box = m_screenCamera.GetOrthographicBox();
	box.m_mins.y = box.m_maxs.y - TEXT_HEIGHT;


	//game time info
	float ds = (float)m_gameClock->GetDeltaSeconds();
	float FPS = ds == 0 ? 0 : 1.f / ds;
	std::string timeInfo = 
		Stringf("Time: %.02f FPS: %.02f Scale: %.02f",
		m_gameClock->GetTotalSeconds(), FPS,
		m_gameClock->GetTimeScale());
	g_theFont->AddVertsForTextInBox2D(UIVerts, timeInfo, box, TEXT_HEIGHT, Rgba8::WHITE, 1.f, Vec2(1, 0));

	//render mode info
	std::string mode = GetRenderModeStr(m_debugRenderMode);
	std::string modeInfo = Stringf("Debug Render Mode #%d: ", m_debugRenderMode);
	modeInfo += mode;
	Rgba8 modeCol = Rgba8::WHITE;
	g_theFont->AddVertsForTextInBox2D(UIVerts, modeInfo, box, TEXT_HEIGHT, modeCol, 1.f, Vec2(0, 0));

	box.m_mins.y -= TEXT_HEIGHT;
	box.m_maxs.y -= TEXT_HEIGHT;

	//render mode info
	std::string lightDebugMode = Stringf("[F1] Toggle Light debug draw, [R] Disable debug render");
	Rgba8 lightDebugModeCol = m_debugDraw? Rgba8::BLUE : Rgba8::WHITE;
	g_theFont->AddVertsForTextInBox2D(UIVerts, lightDebugMode, box, TEXT_HEIGHT, lightDebugModeCol, 1.f, Vec2(0, 0));

	//render BGM info
	AABB2 BGMBox(box);
	BGMBox.Translate(Vec2(0, -TEXT_HEIGHT * 3));
	std::string BGMName = m_areAllModelsLoaded ? m_BGMFileName.c_str() : "Loading...";
	std::string BGMInfo = Stringf("BGM: %s", BGMName.c_str());
	g_theFont->AddVertsForTextInBox2D(UIVerts, BGMInfo, BGMBox, TEXT_HEIGHT, Rgba8::WHITE, 1.f, Vec2(1, 0));

	box.m_mins.y -= TEXT_HEIGHT;
	box.m_maxs.y -= TEXT_HEIGHT;

	//cam and player info
	int playerIndex = (int)m_currentMatch->GetIsPlayer1Turn();
	std::string currentPlayer = m_players[playerIndex].m_playerName;
	std::string camMode = m_worldCamMode == CameraMode::FREEFLY ? "Freefly" : "Locked";
	std::string camInfo = Stringf("Current Player: %s, Camera Mode: %s",
		currentPlayer.c_str(), camMode.c_str());
	g_theFont->AddVertsForTextInBox2D(UIVerts, camInfo, box, TEXT_HEIGHT, Rgba8::WHITE, 1.f, Vec2(0, 0));


	box.m_mins.y -= TEXT_HEIGHT;
	box.m_maxs.y -= TEXT_HEIGHT;

	//light info
	std::string lightInfo = Stringf("Sun Direction: %s, Sun Intensity: %.02f, Ambient Intensity: %.02f",
		GetAsString(m_sunDirection).c_str(), m_sunIntensity, m_ambientIntensity);
	g_theFont->AddVertsForTextInBox2D(UIVerts, lightInfo, box, TEXT_HEIGHT, Rgba8::WHITE, 1.f, Vec2(0, 0));




	g_theRenderer->BindTexture(&g_theFont->GetTexture());
	g_theRenderer->DrawVertexArray(UIVerts);

	g_theRenderer->EndCamera(m_screenCamera);

	

}

void Game::DebugRender() const
{
	//render lights

	g_theRenderer->SetModelConstants();
	g_theRenderer->BindTexture(nullptr, 0);
	g_theRenderer->BindTexture(nullptr, 1);
	g_theRenderer->BindShader(nullptr);



	std::vector<Vertex_PCU> tempVerts;

	//sunlight: arrow
	Vec3 sunArrowStart(-1,4,3);
	AddVertsForArrow3D(tempVerts, sunArrowStart, sunArrowStart + m_sunDirection, 0.1f, Rgba8::WHITE);
	
	for (int i = 0; i < m_numLights; i++)
	{
		if (m_spotLights[i].c_innerDotThreshold == -1)	//point light: two spheres
		{
			Rgba8 color = Rgba8::WHITE * m_spotLights[i].c_lightColor;
			AddVertsForUVSphereZWireframe3D(tempVerts, m_spotLights[i].c_lightWorldPosition, m_spotLights[i].c_innerRadius, 16, 16, DEBUG_THICKNESS, color);
			AddVertsForUVSphereZWireframe3D(tempVerts, m_spotLights[i].c_lightWorldPosition, m_spotLights[i].c_outerRadius, 16, 16, DEBUG_THICKNESS, color);
		}
		else {//spot light: two cones

			Vec3 coneStart = m_spotLights[i].c_lightWorldPosition;
			float h = ACosDegrees(m_spotLights[i].c_innerDotThreshold);
			float circleInnerRadius = m_spotLights[i].c_innerRadius * TanDegrees(h);
			Vec3 coneEnd = coneStart + m_spotLights[i].c_lightFwdNormal * m_spotLights[i].c_innerRadius;
			Rgba8 color = Rgba8::WHITE * m_spotLights[i].c_lightColor;
			AddVertsForWirframeCone3D(tempVerts, coneEnd, coneStart, circleInnerRadius, color, DEBUG_THICKNESS);

			h = ACosDegrees(m_spotLights[i].c_outerDotThreshold);
			float circleOuterRadius = m_spotLights[i].c_outerRadius * TanDegrees(h);
			coneEnd = coneStart + m_spotLights[i].c_lightFwdNormal * m_spotLights[i].c_outerRadius;
			AddVertsForWirframeCone3D(tempVerts, coneEnd, coneStart, circleOuterRadius, color, DEBUG_THICKNESS);
		}
	}


	g_theRenderer->DrawVertexArray(tempVerts);


}

//render all entities, including player and props
void Game::RenderEntities() const
{
	g_theRenderer->SetPerFrameConstants((float)m_gameClock->GetTotalSeconds(), m_debugRenderMode, 0, 0);

	int numEntities = (int)m_allEntities.size();
	for (int i = 0; i < numEntities; i++) {
		
		g_theRenderer->SetModelConstants(m_allEntities[i]->GetModelToWorldTransform(), m_allEntities[i]->m_color);
		m_allEntities[i]->Render();
	}

	g_theRenderer->SetModelConstants(m_boardModel.GetModelToWorldTransform(), m_boardModel.m_color);
	m_boardModel.Render();

	m_currentMatch->Render();



}

bool Game::IsInAttractMode() const
{
	return m_currentState == GameState::ATTRACT;
}

float Game::GetGlobalVolume() const
{
	return m_musicVolume;
}

Vec2 Game::GetGameDimensions() const
{
	return m_gameDimensions;
}

Vec2 Game::GetClientDimensions() const
{
	return m_windowsDimensions;
}

std::string Game::GetDefaultBoardState() const
{
	return m_defaultBoardState;
}

Clock* Game::GetGameClock() const
{
	return m_gameClock;
}

Vec3 Game::GetWorldCameraPos() const
{
	return m_worldCamera.GetPosition();
}

Vec3 Game::GetWorldCameraFwdNormal() const
{
	return m_worldCamera.GetFwdNormal();
}

Player Game::GetPlayerByIndex(int index) const
{
	return m_players[index];
}
Vec2 Game::GetGamePosition(Vec2 const& clientPos) const
{
	Vec2 pos = RangeMapClampedVec2(clientPos, 
		AABB2(Vec2(),m_windowsDimensions), AABB2(Vec2(), m_gameDimensions));
	pos.y = m_gameDimensions.y - pos.y;
	return pos;
}

Vec2 Game::GetGameDirection(Vec2 const& clientDelta) const
{
	AABB2 clientDimension;
	clientDimension.SetDimensions(m_windowsDimensions);
	AABB2 computerDimension;
	computerDimension.SetDimensions(m_gameDimensions);
	Vec2 scaledDir = RangeMapClampedVec2(clientDelta, clientDimension, computerDimension);
	scaledDir.y = -scaledDir.y;
	return scaledDir;
}

void Game::UpdateCameraBasedOnInput()
{
	//does not update camera if in pointer mode
	if (g_theInput->GetCursorMode() == CursorMode::POINTER)return;


	if (m_worldCamMode == CameraMode::FREEFLY)
	{
		UpdateFromKeyboard();
		UpdateFromController();
	}
}

void Game::UpdateFromKeyboard()
{
	float sysDs = static_cast<float>(g_theSysClock->GetDeltaSeconds());
	Vec3 position = m_worldCamera.GetPosition();
	EulerAngles orientation = m_worldCamera.GetOrientation();

	//orientation
	Vec2 cursorDelta = g_theInput->GetCursorClientDelta();
	if (cursorDelta != Vec2::ZERO) {
		orientation.m_yawDegees -= cursorDelta.x * m_pixelToDegreeRatio;
		orientation.m_pitchDegees += cursorDelta.y * m_pixelToDegreeRatio;
		orientation.m_pitchDegees = GetClamped(orientation.m_pitchDegees, -85.f, 85.f);
	}
	/*
	float rollDelta = sysDs * m_rollDegPerSecond;

	if (g_theInput->IsKeyDown('Z')) {
		orientation.m_rollDegees -= rollDelta;

	}
	if (g_theInput->IsKeyDown('C')) {
		orientation.m_rollDegees += rollDelta;
	}
	orientation.m_rollDegees = GetClamped(orientation.m_rollDegees, -45.f, 45.f);
	*/
	//movement
	float movementdelta = sysDs * m_moveUnitsPerSecond;
	//increase speed
	if (g_theInput->IsKeyDown(KEYCODE_SHIFT)) {
		movementdelta *= m_shiftSpeedMultiplier;
	}

	//Vec3 fowardRelative = Vec3::MakeFromPolarDegreesAroundZ(orientation.m_yawDegees, movementdelta);
	//Vec3 leftRelative = Vec3::MakeFromPolarDegreesAroundZ(orientation.m_yawDegees + 90.f, movementdelta);
	Mat44 transform = Mat44::MakeTranslationThenOrientation(Vec3(), orientation);
	Vec3 fowardRelative = transform.GetIBasis3D() * movementdelta;
	Vec3 leftRelative = transform.GetJBasis3D() * movementdelta;

	if (g_theInput->IsKeyDown('A')) {
		position += leftRelative;
	}
	if (g_theInput->IsKeyDown('D')) {
		position -= leftRelative;
	}
	if (g_theInput->IsKeyDown('W')) {
		position += fowardRelative;
	}
	if (g_theInput->IsKeyDown('S')) {
		position -= fowardRelative;

	}


	if (g_theInput->IsKeyDown('Q')) {
		position.z -= movementdelta;
	}
	if (g_theInput->IsKeyDown('E')) {
		position.z += movementdelta;
	}


	//reset position and orientation
	if (g_theInput->IsKeyDown('H')) {
		SetPosAndOrientationForPlayerTurn();
	}

	

	m_worldCamera.SetPosition(position);
	m_worldCamera.SetOrientation(orientation);
}

void Game::UpdateFromController()
{
	XboxController const& controller = g_theInput->GetController(0);
	float sysDs = static_cast<float>(g_theSysClock->GetDeltaSeconds());
	Vec3 position = m_worldCamera.GetPosition();
	EulerAngles orientation = m_worldCamera.GetOrientation();

	//orientation
	AnalogJoystick rightStick = controller.GetRightStick();
	if (rightStick.GetMagnitude() != 0.f) {
		Vec2 rightStickDelta = rightStick.GetPosition();
		orientation.m_yawDegees -= rightStickDelta.x * m_pixelToDegreeRatio;
		orientation.m_pitchDegees -= rightStickDelta.y * m_pixelToDegreeRatio;
		orientation.m_pitchDegees = GetClamped(orientation.m_pitchDegees, -85.f, 85.f);
	}
	/*
	float rollDelta = sysDs * m_rollDegPerSecond;
	
	if (controller.GetLeftTrigger() > 0.f) {
		orientation.m_rollDegees -= rollDelta;

	}
	if (controller.GetRightTrigger() > 0.f) {
		orientation.m_rollDegees += rollDelta;
	}
	orientation.m_rollDegees = GetClamped(orientation.m_rollDegees, -45.f, 45.f);
	*/

	//movement
	float movementdelta = sysDs * m_moveUnitsPerSecond;
	//increase speed
	if (controller.IsButtonDown(XBOX_BUTTON_A)) {
		movementdelta *= m_shiftSpeedMultiplier;
	}

	Vec3 fowardRelative = Vec3::MakeFromPolarDegreesAroundZ(orientation.m_yawDegees, movementdelta);
	Vec3 leftRelative = Vec3::MakeFromPolarDegreesAroundZ(orientation.m_yawDegees + 90.f, movementdelta);

	AnalogJoystick leftStrick = controller.GetLeftStick();
	Vec2 leftStickDelta = leftStrick.GetPosition();

	if (leftStickDelta.x != 0) {
		position -= leftRelative * leftStickDelta.x;
	}
	if (leftStickDelta.y != 0) {
		position += fowardRelative * leftStickDelta.y;
	}

	if (controller.IsButtonDown(XBOX_BUTTON_LSHOULDER)) {
		position.z -= movementdelta;
	}
	if (controller.IsButtonDown(XBOX_BUTTON_RSHOULDER)) {
		position.z += movementdelta;
	}


	//reset position and orientation
	if (controller.IsButtonDown(XBOX_BUTTON_START)) {
		SetPosAndOrientationForPlayerTurn();
	}

	m_worldCamera.SetPosition(position);
	m_worldCamera.SetOrientation(orientation);
}

void Game::ToggleCameraMode()
{
	if (m_worldCamMode == CameraMode::LOCKED)
	{
		m_worldCamMode = CameraMode::FREEFLY;
	}else if (m_worldCamMode == CameraMode::FREEFLY)
	{
		m_worldCamMode = CameraMode::LOCKED;
		SetPosAndOrientationForPlayerTurn();

	}
}


void Game::SetPosAndOrientationForPlayerTurn()
{
	if (m_currentState == GameState::ATTRACT)
	{
		if (Game::s_isPlayingRemotely)//if playing online, locked to my player index view
		{
			if (s_myPlayerIndex == 0)
			{
				m_worldCamera.SetPosition(m_defaultPlayer0CamPos);
				m_worldCamera.SetOrientation(m_defaultPlayer0CamOri);
			}
			else if (s_myPlayerIndex == 1)
			{
				m_worldCamera.SetPosition(m_defaultPlayer1CamPos);
				m_worldCamera.SetOrientation(m_defaultPlayer1CamOri);
			}
			return;
		}
		else {
			m_worldCamera.SetPosition(m_defaultPlayer0CamPos);
			m_worldCamera.SetOrientation(m_defaultPlayer0CamOri);
		}
		
		
	}
	else if (m_currentState == GameState::PLAYING)
	{
		if (Game::s_isPlayingRemotely && !Game::s_isSpectator)//if playing online, locked to my player index view
		{
			return;
		}
		if (m_currentPlayerIndex == 0)
		{
			m_worldCamera.SetPosition(m_defaultPlayer0CamPos);
			m_worldCamera.SetOrientation(m_defaultPlayer0CamOri);
		}
		else if (m_currentPlayerIndex == 1)
		{
			m_worldCamera.SetPosition(m_defaultPlayer1CamPos);
			m_worldCamera.SetOrientation(m_defaultPlayer1CamOri);
		}
	}else if (m_currentState == GameState::GAMEOVER)
	{
		m_worldCamera.SetPosition(m_defaultGameOverCamPos);
		int result = m_currentMatch->GetResult();
		if (result == 0)
		{
			m_defaultGameOverCamOri.m_yawDegees = 90.f;
		}else if (result == 1)
		{
			m_defaultGameOverCamOri.m_yawDegees = -90.f;
		}
		else
		{
			m_defaultGameOverCamOri.m_yawDegees = 0;
		}
		
		m_worldCamera.SetOrientation(m_defaultGameOverCamOri);
	}
}

void Game::UpdateEntities()
{
	float ds = static_cast<float>(m_gameClock->GetDeltaSeconds());
	int numEntities = (int)m_allEntities.size();
	for (int i = 0; i < numEntities;i++) {
		m_allEntities[i]->Update(ds);
	}

	m_currentMatch->Update(ds);
	if (m_currentMatch->IsMatchFinished())
	{
		EnterState(GameState::GAMEOVER);
		if (m_worldCamMode == CameraMode::LOCKED)
		{
			SetPosAndOrientationForPlayerTurn();
		}
	}


	m_boardModel.Update(ds);
}



void Game::UpdateCameras()
{
	UpdateCameraBasedOnInput();
}

void Game::MoveCamTo(Camera& cam, Vec2 position)
{
	Vec2 camOffset((cam.GetOrthographicTopRight().x - cam.GetOrthographicBottomLeft().x)*0.5f, (cam.GetOrthographicTopRight().y - cam.GetOrthographicBottomLeft().y)*0.5f);

	cam.SetOrthographicView(position-camOffset,position+camOffset);

}

Mat44 Game::MakeCameraToRenderTransform() const
{
	Vec3 iBasis(0, 0, 1.f);
	Vec3 jBasis(-1.f, 0, 0);
	Vec3 kBasis(0, 1.f, 0);
	Vec3 translation(0, 0, 0);
	Mat44 result(iBasis,jBasis,kBasis, translation);
	return result;
}

void Game::SetBGMVolume(float volume)
{
	m_musicVolume = volume;
	SetPlayingBGMToVolume();
}

void Game::SubscribeToLocalCommands()
{
	//local commands

	//ChessServerInfo [ip=<remoteIPAddress>] [port=<portNumber>]
	EventArgs argsChessInfo;
	argsChessInfo.SetValue("ip", "127.0.0.1");
	argsChessInfo.SetValue("port", "3100");
	g_theEventSystem->SubscribeEventCallbackFunction("ChessServerInfo", Event_ChessServerInfo, argsChessInfo);

	//ChessListen [port=<portNumber>]
	EventArgs argsChessListen;
	argsChessListen.SetValue("port", "3100");
	g_theEventSystem->SubscribeEventCallbackFunction("ChessListen", Event_ChessListen, argsChessListen);

	//ChessConnect [ip=<remoteAddress>] [port=<portNumber>] [isSpectator=false]
	EventArgs argsChessConnect;
	argsChessConnect.SetValue("ip", "127.0.0.1");
	argsChessConnect.SetValue("port", "3100");
	g_theEventSystem->SubscribeEventCallbackFunction("ChessConnect", Event_ChessConnect, argsChessConnect);

	//RemoteCmd cmd=<commandName> [<key1>=<value1>] [key2=<value2>]…
	g_theEventSystem->SubscribeEventCallbackFunction("RemoteCmd", Event_RemoteCmd);
}

void Game::UnsubscribeToLocalCommands()
{
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessServerInfo", Event_ChessServerInfo);
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessListen", Event_ChessListen);
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessConnect", Event_ChessConnect);
	g_theEventSystem->UnsubscribeEventCallbackFunction("RemoteCmd", Event_RemoteCmd);
}

void Game::SubscribeToRemoteCommands()
{
	//remote commands

	//[remote] ChessDisconnect [reason="<explanation>"]
	EventArgs argsChessDisconnect;
	argsChessDisconnect.SetValue("reason", "gotta eat dinner");
	g_theEventSystem->SubscribeEventCallbackFunction("ChessDisconnect", Event_ChessDisconnect, argsChessDisconnect);

	//[remote] ChessPlayerInfo [name=<myName>]
	EventArgs argsChessPlayerInfo;
	argsChessPlayerInfo.SetValue("name", "Miffy");
	g_theEventSystem->SubscribeEventCallbackFunction("ChessPlayerInfo", Event_ChessPlayerInfo, argsChessPlayerInfo);

	//[remote] ChessBegin [firstPlayer=<playerName>]
	EventArgs argsChessBegin;
	argsChessBegin.SetValue("firstPlayer", "FirstPlayer");
	g_theEventSystem->SubscribeEventCallbackFunction("ChessBegin", Event_ChessBegin, argsChessBegin);

	// [remote] ChessValidate 
	// [state = <gameState>] [player1 = <playerName>] [player2 = <playerName>]
	//	[move = <moveNumber>] [board = <64characters>] :
	EventArgs argsChessValidate;
	argsChessValidate.SetValue("state", "player1Moving");
	argsChessValidate.SetValue("player1", "FirstPlayer");
	argsChessValidate.SetValue("player2", "SecondPlayer");
	argsChessValidate.SetValue("move", "0");
	argsChessValidate.SetValue("board", "RNBKQBNRPPPPPPPP................................pppppppprnbkqbnr");
	g_theEventSystem->SubscribeEventCallbackFunction("ChessValidate", Event_ChessValidate, argsChessValidate);

	//[remote] ChessMove from=<square> to=<square> [promoteTo=<pieceType>] [teleport=true]
	EventArgs argsChessMove;
	argsChessMove.SetValue("from", "E2");
	argsChessMove.SetValue("to", "E4");
	argsChessMove.SetValue("promoteTo", "Queen");
	argsChessMove.SetValue("teleport", "true");
	g_theEventSystem->SubscribeEventCallbackFunction("ChessMove", Event_ChessMove, argsChessMove);

	//[remote] ChessResign
	g_theEventSystem->SubscribeEventCallbackFunction("ChessResign", Event_ChessResign);

	//[remote] ChessOfferDraw
	g_theEventSystem->SubscribeEventCallbackFunction("ChessOfferDraw", Event_ChessOfferDraw);

	//[remote] ChessAcceptDraw and ChessRejectDraw
	g_theEventSystem->SubscribeEventCallbackFunction("ChessAcceptDraw", Event_ChessAcceptDraw);
	g_theEventSystem->SubscribeEventCallbackFunction("ChessRejectDraw", Event_ChessRejectDraw);
}

void Game::UnsubscribeToRemoteCommands()
{

	
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessDisconnect", Event_ChessDisconnect);
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessPlayerInfo", Event_ChessPlayerInfo);
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessBegin", Event_ChessBegin);
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessValidate", Event_ChessValidate);
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessMove", Event_ChessMove);
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessResign", Event_ChessResign);
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessOfferDraw", Event_ChessOfferDraw);
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessAcceptDraw", Event_ChessAcceptDraw);
	g_theEventSystem->UnsubscribeEventCallbackFunction("ChessRejectDraw", Event_ChessRejectDraw);
}

/* [local] ChessServerInfo [ip=<remoteIPAddress>] [port=<portNumber>]
prints out all values (name, ip, port) and current connection status and game state 
regardless of which arguments are given (or none!).
a.	Example: “ChessInfo”
b.	Example: “ChessInfo ip=192.168.0.53”
c.	Example: “ChessInfo port=3100”
Note: If currently connected, any changes to these values should be rejected and warned.
*/
bool Game::Event_ChessServerInfo(EventArgs& args)
{
	std::string tempIP = args.GetValue("ip", "");
	std::string tempPort = args.GetValue("port", "");
	
	std::string prevIP = g_theNetwork->GetCurrentIPAddress();
	unsigned short prevPort = g_theNetwork->GetCurrentListeningPort();
	unsigned short portNum = tempPort == ""? prevPort : (unsigned short)std::stoi(tempPort);


	bool isConnected = g_theNetwork->IsClientConnectedToServer() || g_theNetwork->IsServerConnectedToAtLeastOneClient();
	bool hasChangedIPorPort = (tempIP != "" && tempIP != prevIP) || (portNum != prevPort);

	if (isConnected)
	{
		if (hasChangedIPorPort) {
			PrintWarningMsgToConsole("Warning: ChessServerInfo: you are connected to at least one server/client, cannot change port or IP now.");
		}
	}
	else {
		if (tempIP != "")
		{
			if (!g_theNetwork->IsServerIPAddressValid(tempIP))
			{
				PrintErrorMsgToConsole("Error: ChessServerInfo invalid IP Address. Example: 127.0.0.1");
				return false;
			}
			else {

				g_theNetwork->SwitchToIPAddress(tempIP);
			}
		}
		if (tempPort != "")
		{

			if (!g_theNetwork->IsServerListeningPortValid(portNum))
			{
				PrintErrorMsgToConsole("Error: ChessServerInfo invalid Port Number (1024 < port < 65535).");
				return false;
			}
			else {
				g_theNetwork->SwitchToListeningPort(portNum);
			}
		}
	}
	bool printSuccessful = PrintInfoMsgToConsole(g_theNetwork->GetConnectionStateString());

	return printSuccessful;
}

/* [local] ChessListen [port=<portNumber>]: Run StartServer on your NetworkSystem, 
then start a Listen socket at the specified port number 
(or use existing default or previously-overridden port number if omitted).  
Code default is port=3100.
a.	Example: “ChessListen” // use port from code or previous ChessInfo.
b.	Example: “ChessListen port=3101” // explicit port number override
*/
bool Game::Event_ChessListen(EventArgs& args)
{
	std::string tempPort = args.GetValue("port", "");
	unsigned short prevPort = g_theNetwork->GetCurrentListeningPort();
	unsigned short portNum = tempPort == "" ? prevPort : (unsigned short)std::stoi(tempPort);
	s_isSpectator = false;

	if (!g_theNetwork->IsServerListeningPortValid(portNum))
	{//if input port is not valid
		if (!g_theNetwork->IsServerListeningPortValid(prevPort))
		{//if neither the default port nor the input port is valid
			PrintErrorMsgToConsole("Error: ChessListen invalid or missing Port Number (1024 < port < 65535).");
			return false;
		}
		else {//if the default is valid
			std::string warningMsg = Stringf("Warning: ChessListen invalid or missing Port Number (1024 < port < 65535). Defaulting to %d.", prevPort);
			PrintWarningMsgToConsole(warningMsg);
			portNum = prevPort;
		}
	}
	else if (portNum != prevPort) {//if input port is valid but not equal to the default port
		std::string infoMsg = Stringf("ChessListen: Switching from port %d to port %d and start listening", prevPort, portNum);
		PrintInfoMsgToConsole(infoMsg);
	}
	else {//if input port is equal to the default port
		std::string infoMsg = Stringf("ChessListen: Start listening on port %d", portNum);
		PrintInfoMsgToConsole(infoMsg);
	}

	ConnectionState currentState = g_theNetwork->GetConnectionState();
	switch (currentState)
	{
	case ConnectionState::IDLE:
	{
		g_theNetwork->SetServerListeningPort(portNum);
		g_theNetwork->StartServer();
		break;
	}
	case ConnectionState::NS_SERVER_LISTENING:
	{
		g_theNetwork->SwitchToListeningPort(portNum);
		break;
	}
	case ConnectionState::NS_CLIENT_CONNECTING:
	case ConnectionState::NS_CLIENT_CONNECTED:
	{
		g_theNetwork->ClientDisconnect();
		g_theNetwork->SetServerListeningPort(portNum);
		g_theNetwork->StartServer();
		break;
	}
	case ConnectionState::UNINITIALIZED:
	case ConnectionState::COUNT:
	default:
	{
		PrintErrorMsgToConsole("Error: invalid network connection state: uninitialized");
		return false;
	}

	}
	s_myPlayerIndex = false;


	return true;
}

/* [local] ChessConnect [ip=<remoteAddress>] [port=<portNumber>]  [isSpectator=false]
Run StartClient on your NetworkSystem, then try to Connect to a server at this 
remote IP address and port number.  Add any omitted arguments using existing 
(default or previously-overridden) values.  
Code defaults are ip=127.0.0.1 and port=3100.
a.	Example: “ChessConnect” // use current IP and port from code, GameConfig.xml, 
	or previous ChessInfo command.
b.	Example: “ChessConnect ip=192.168.0.53” // explicit IP, current port
c.	Example: “ChessConnect ip=192.168.0.53 port=3100” // explicit IP and port
*/
bool Game::Event_ChessConnect(EventArgs& args)
{
	
	//port validation
	std::string tempPort = args.GetValue("port", "");
	unsigned short prevPort = g_theNetwork->GetCurrentListeningPort();
	unsigned short portNum = tempPort == "" ? prevPort : (unsigned short)std::stoi(tempPort);//if no param, defaulting to prev port
	if (!g_theNetwork->IsServerListeningPortValid(portNum))
	{//if input port is not valid
		if (!g_theNetwork->IsServerListeningPortValid(prevPort))
		{//if neither the default port nor the input port is valid
			PrintErrorMsgToConsole("Error: ChessConnect invalid or missing Port Number (1024 < port < 65535).");
			return false;
		}
		else {//if the default is valid
			std::string warningMsg = Stringf("Warning: ChessConnect invalid or missing Port Number (1024 < port < 65535). Defaulting to %d.", prevPort);
			PrintWarningMsgToConsole(warningMsg);
			portNum = prevPort;
		}
	}
	else if (portNum != prevPort) {//if input port is valid but not equal to the default port
		std::string infoMsg = Stringf("ChessConnect: Switching from port %d to port %d", prevPort, portNum);
		PrintInfoMsgToConsole(infoMsg);
	}

	//IP validation
	std::string tempIP = args.GetValue("ip", "");
	std::string prevIP = g_theNetwork->GetCurrentIPAddress();
	tempIP = tempIP == "" ? prevIP : tempIP; //if no param, defaulting to prev IP
	if (!g_theNetwork->IsServerIPAddressValid(tempIP))
	{
		if (!g_theNetwork->IsServerIPAddressValid(prevIP))
		{//if neither the default ip nor the input ip is valid
			PrintErrorMsgToConsole("Error: ChessConnect invalid or missing IP.");
			return false;
		}
		else {//if the default is valid
			std::string warningMsg = Stringf("Warning: ChessConnect invalid or missing IP Address. Defaulting to %s.", prevIP.c_str());
			PrintWarningMsgToConsole(warningMsg);
			tempIP = prevIP;
		}
	}
	else if (tempIP != prevIP) {//if input IP is valid but not equal to the default IP
		std::string infoMsg = Stringf("ChessConnect: Switching from IP %s to IP %s", prevIP.c_str(), tempIP.c_str());
		PrintInfoMsgToConsole(infoMsg);
	}

	//spectator validation
	std::string isSpectator = args.GetValue("isSpectator", "");
	int spectatorValidation = GetValidationOfBooleanArgs(isSpectator);
	if (spectatorValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessConnect invalid isSpectator command");
		return false;
	}

	s_isSpectator = spectatorValidation == 1;

	std::string infoMsg = Stringf("ChessConnect: Trying to connect to IP address %s at port %d", tempIP.c_str(), portNum);
	if (s_isSpectator)
	{
		infoMsg.append(" as spectator");
	}
	PrintInfoMsgToConsole(infoMsg);

	ConnectionState currentState = g_theNetwork->GetConnectionState();
	switch (currentState)
	{
	case ConnectionState::IDLE:
	{
		g_theNetwork->SetServerListeningPort(portNum);
		g_theNetwork->SetServerIPAddr(tempIP);
		g_theNetwork->StartClient();
		break;
	}
	case ConnectionState::NS_SERVER_LISTENING:
	{//server to client
		g_theNetwork->ServerDisconnect();
		g_theNetwork->SetServerListeningPort(portNum);
		g_theNetwork->SetServerIPAddr(tempIP);
		g_theNetwork->StartClient();
		break;
	}
	case ConnectionState::NS_CLIENT_CONNECTING:
	case ConnectionState::NS_CLIENT_CONNECTED:
	{
		g_theNetwork->SwitchToListeningPortAndIPAddress(portNum, tempIP);
		break;
	}
	case ConnectionState::UNINITIALIZED:
	case ConnectionState::COUNT:
	default:
	{
		PrintErrorMsgToConsole("Error: invalid network connection state: uninitialized");
		return false;
	}

	}
	s_myPlayerIndex = true;

	return true;
}

/* [local] RemoteCmd cmd=<commandName> [<key1>=<value1>] [key2=<value2>]… :
Builds a DevConsole command string to send to the remote computer for execution,
where <commandName> is the actual DevConsole command (and EventSystem event name)
to be executed remotely.
“RemoteCmd” is not sent as part of the command string, nor is “cmd=”.
When your Chess game receives a remote command string from the network,
you should
	(a) append the text “remote=true” onto the end of the string,and then
	(b) Execute that string in your dev console (hopefully firing an Event,
		if the command name and arguments are correct).
a.	Example: “RemoteCmd cmd=Echo text=Hello” would send “Echo text=Hello” over the network;
	the receiving computer would add “ remote=true” on the end of it, and then
	Execute the final command string “Echo text=Hello remote=true” in its DevConsole.
b.	Example: “RemoteCmd cmd=ChessMove from=e2 to=e4” would send the command string
	“ChessMove from=e2 to=e4” over the network, and the remote computer would append to it
	and then execute the string “ChessMove from=e2 to=e4 remote=true” in its DevConsole
	(as if it had been typed there).
*/
bool Game::Event_RemoteCmd(EventArgs& args)
{
	std::string commandName = args.GetValue("cmd", "");
	Strings fireableCommands = g_theEventSystem->GetEventNames();
	/*//dont assume the others will have the same cmd line args
	if (DoStringsContains(fireableCommands, commandName))
	{
		PrintErrorMsgToConsole("Error: RemoteCmd: invalid Remote command");
		return false;
	}*/

	std::string commandLine(commandName);
	
	std::map<std::string, std::string> allKeyValurPairs = args.GetAllValuePairs();
	std::string result = "";
	for (std::map<std::string, std::string>::const_iterator it = allKeyValurPairs.cbegin(); it != allKeyValurPairs.cend(); ++it) {
		if (it->first != "cmd")
		{
			result.append(Stringf(" %s=%s", it->first.c_str(), it->second.c_str()));
		}
	}
	commandLine.append(result);
	commandLine.append(" remote=true");

	//g_theEventSystem->FireEvent(commandName, args);
	SetOutgoingData(commandLine);


	return true;
}

/* [remote] ChessDisconnect [reason="<explanation>"]  [isSpectator=false]
Send this command remotely to the connected computer,
then disconnect yourself after sending.  
If the command includes a “remote=true” argument then it was sent to you from the remote host;
disconnect your connection but do not re-send it back.
a.	Example: “ChessDisconnect”
b.	Example: “ChessDisconnect reason=Goodbye!”
c.	Example: “ChessDisconnect reason="I have to go cook dinner."”
*/
bool Game::Event_ChessDisconnect(EventArgs& args)
{
	std::string reason = args.GetValue("reason", "");
	std::string isRemote = args.GetValue("remote", "");
	int remoteValidation = GetValidationOfBooleanArgs(isRemote);
	if (remoteValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessDisconnect invalid Remote command");
		return false;
	}

	//spectator validation
	std::string isSpectator = args.GetValue("isSpectator", "");
	int spectatorValidation = GetValidationOfBooleanArgs(isSpectator);
	if (spectatorValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessConnect invalid isSpectator command");
		return false;
	}
	bool isEventSenderSpectator = spectatorValidation == 1;


	bool isSentByMyself = remoteValidation == 0;
	std::string explanationMsg("ChessDisconnect: Disconnecting...");
	bool isDisconnecting = false;
	if (isSentByMyself)
	{
		std::string commandLine("ChessDisconnect remote=true");
		if (reason != "")
		{
			commandLine = Stringf("ChessDisconnect reason=%s remote=true", reason.c_str());
			explanationMsg = Stringf("ChessDisconnect: Disconnecting with reason: %s", reason.c_str());
		}
		if (isEventSenderSpectator || s_isSpectator)
		{//if you append isSpectator=true at the end of Disconnect, or you isSpectator=true at the end of Connect
			commandLine.append(" isSpectator=true");
		}
		PrintInfoMsgToConsole(explanationMsg);
		SendOutgoingDataImmediately(commandLine);
		g_theNetwork->Disconnect();//disconnect 
		isDisconnecting = true;
	}
	else {
		if (reason != "")
		{
			explanationMsg = Stringf("ChessDisconnect: Opponent disconnecting with reason:%s", reason.c_str());
		}
		else {
			explanationMsg = "ChessDisconnect: Opponent disconnecting...";
		}
		if (isEventSenderSpectator)
		{//if the event sender is a spectator
			explanationMsg = "ChessDisconnect: One spectator is disconnecting...";
		}else
		{//if the event sender is not a spectator
			g_theNetwork->Disconnect();//disconnect all
			isDisconnecting = true;
		}
		PrintInfoMsgToConsole(explanationMsg);
		

	}
	if (isDisconnecting && App::s_theGame->m_currentState != GameState::ATTRACT)
	{
		PrintInfoMsgToConsole("ChessDisconnect: Disconnected");
		App::s_theGame->EnterState(GameState::ATTRACT);
	}
	
	
	
	return true;
}

/* [remote] ChessPlayerInfo [name=<myName>]
Sets your local player name. 
If received from remote host with the “remote=true” argument, 
sets your opponent’s player name instead.
a.	Example: “ChessPlayerInfo name=Squirrel” // My name is Squirrel
b.	Example: “ChessPlayerInfo name=Matt remote=true” // Opponent says he is Matt
*/
bool Game::Event_ChessPlayerInfo(EventArgs& args)
{
	std::string playerName = args.GetValue("name", "");
	std::string isRemote = args.GetValue("remote", "");
	int remoteValidation = GetValidationOfBooleanArgs(isRemote);
	if (remoteValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessPlayerInfo invalid Remote command");
		return false;
	}
	bool isSentByMyself = remoteValidation == 0;

	std::string explanationMsg("ChessPlayerInfo");
	std::string myName = App::s_theGame->m_players[s_myPlayerIndex].m_playerName;
	std::string opponentName = App::s_theGame->m_players[!s_myPlayerIndex].m_playerName;

	std::string commandLine = Stringf("ChessPlayerInfo name=%s", playerName.c_str());

	if (isSentByMyself)//setting my name on opponent's machine
	{
		if (!SpectatorCheck("set player info", args.GetValue("fromServer", "")))return false;
		
		if (playerName != "")
		{
			if (playerName == opponentName)//you are trying to set name to be the same as opponent's name
			{
				std::string errorMsg = Stringf("Error: ChessPlayerInfo you are trying to set name to be the same as your opponent's name %s.", opponentName.c_str());
				PrintErrorMsgToConsole(errorMsg);
				return false;
			}
			App::s_theGame->m_players[s_myPlayerIndex].m_playerName = playerName;
			myName = playerName;
		}
		else {//player name not input, defaulting to default
			playerName = myName;
			std::string warningMsg = Stringf("Warning: ChessPlayerInfo missing name args. Defaulting to %s.", playerName.c_str());
			PrintWarningMsgToConsole(warningMsg);
		}
		commandLine = Stringf("ChessPlayerInfo name=%s", playerName.c_str());
		if (!s_isSpectator) {
			commandLine.append(" remote=true");
			SetOutgoingData(commandLine);
		}
		
	}
	else {//setting opponent's name on my machine

		//as a server, you need to relay opponent's command to spectators
		SetOutgoingDataToAllSpectators(commandLine);


		if (playerName != "")
		{
			if (playerName == myName)//opponent trying to set name to be the same as my name
			{
				std::string errorMsg = Stringf("Error: ChessPlayerInfo opponent trying to set their name to be the same as your name %s.", myName.c_str());
				PrintErrorMsgToConsole(errorMsg);
				return false;
			}
			App::s_theGame->m_players[!s_myPlayerIndex].m_playerName = playerName;
			opponentName = playerName;
		}
		else {//player name cannot be null
			std::string errorMsg = Stringf("Error: ChessPlayerInfo opponent missing name args.");
			PrintErrorMsgToConsole(errorMsg);
			return false;
		}
	}

	explanationMsg = Stringf("ChessPlayerInfo: my name is %s, opponent's name is %s", myName.c_str(), opponentName.c_str());
	PrintInfoMsgToConsole(explanationMsg);
	g_theAudio->StartSound(App::s_theGame->m_infoSFX, false, s_SFXVolume);

	return true;
}

/* [remote] ChessBegin [firstPlayer=<playerName>]: 
Start a new match with the named player as the starting player 
(playing the traditional “white” position), defaults to you if unspecified.
a.	Example: “ChessBegin” // start new game with me as first player 
	(adds firstPlayer=<myName> to remote command before sending)
b.	Example: “ChessBegin firstPlayer=Matt” // I’m starting a new game; Matt goes first
c.	Example: “ChessBegin firstPlayer=Matt remote=true” // Matt started a new game, and wants himself to go first
*/
bool Game::Event_ChessBegin(EventArgs& args)
{

	if (!App::s_theGame->m_areAllModelsLoaded)
	{
		std::string sorryMsg("ChessBegin: Sorry, the assets are still loading, please wait...");
		PrintErrorMsgToConsole(sorryMsg);
		DebugAddMessage(sorryMsg, 2, Rgba8::RED, Rgba8::RED);
		return false;
	}

	std::string firstPlayerName = args.GetValue("firstPlayer", "");
	std::string isRemote = args.GetValue("remote", "");
	int remoteValidation = GetValidationOfBooleanArgs(isRemote);
	if (remoteValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessBegin invalid Remote command");
		return false;
	}
	bool isSentByMyself = remoteValidation == 0;

	std::string myName = App::s_theGame->m_players[s_myPlayerIndex].m_playerName;
	std::string opponentName = App::s_theGame->m_players[!s_myPlayerIndex].m_playerName;
	//match player name with player index
	int startingPlayerIndex = -1;
	if (firstPlayerName == "" || firstPlayerName == myName)//no param, defaulting to default player name
	{
		startingPlayerIndex = s_myPlayerIndex;
		firstPlayerName = myName;
	}else if (firstPlayerName == opponentName)
	{
		startingPlayerIndex = !s_myPlayerIndex;
	}


	if (startingPlayerIndex == -1)
	{
		std::string errorMsg = Stringf("Error: ChessBegin invalid name args. name should be a player's name (%s/%s)", myName.c_str(), opponentName.c_str());
		PrintErrorMsgToConsole(errorMsg);
		return false;
	}
	std::string commandLine = Stringf("ChessBegin firstPlayer=%s", firstPlayerName.c_str());

	if (isSentByMyself)
	{
		if (!SpectatorCheck("start a game", args.GetValue("fromServer", "")))return false;
		if (!s_isSpectator)
		{
			commandLine.append(" remote=true");
			SetOutgoingData(commandLine);
		}
	}
	else {
		//as a server, you need to relay opponent's command to spectators
		SetOutgoingDataToAllSpectators(commandLine);
	}
	App::s_theGame->m_currentPlayerIndex = startingPlayerIndex;
	App::s_theGame->EnterState(GameState::PLAYING);

	return true;
}

/* [remote] ChessValidate 
[state=<gameState>] [player1=<playerName>] [player2=<playerName>] 
[move=<moveNumber>] [board=<64characters>]: 
Validate the current state of the game, names and seats of the players, 
current move number (starting with 0 before the opening move is made), 
and the state of the entire board.  
Standard piece character abbreviations are used (see above). 
If you receive this (remote=true) then validate each item against your own game state 
	and, if any discontinuity is found:
		i.	Print a detailed report to the DevConsole log, noting all discontinuities;
		ii.	Send ChessDisconnect reason="VALIDATION FAILED" to your opponent;
		iii.	Disconnect.
b.	Example: “ChessValidate” // I’m initiating a validation; all info will be sent to opponent…
c.	…and then “ChessValidate state=Player1Moving player1=Squirrel player2=Matt move=0 
	board=RNBKQBNRPPPPPPPP................................pppppppprnbkqbnr remote=true” 
	// …would be received by my opponent as a result
d.	Valid game state strings include: “Player1Moving”, “Player2Moving”, and “GameOver”
*/
bool Game::Event_ChessValidate(EventArgs& args)
{

	std::string gameState = args.GetValue("state", "");
	std::string player1Name = args.GetValue("player1", "");
	std::string player2Name = args.GetValue("player2", "");
	std::string moveNum = args.GetValue("move", "");
	std::string boardAsAtring = args.GetValue("board", "");

	std::string isRemote = args.GetValue("remote", "");

	int remoteValidation = GetValidationOfBooleanArgs(isRemote);
	if (remoteValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessValidate invalid Remote command");
		return false;
	}
	bool isSentByOpponent = remoteValidation == 1;

	

	const ChessMatch* match = App::s_theGame->m_currentMatch;

	std::string currentGameStateLocal = match == nullptr? "GameOver" : match->GetGameStateAsString();
	//if the game has not started
	if (match == nullptr)
	{
		if (isSentByOpponent) {
			if (gameState != currentGameStateLocal)
			{
				PrintInfoMsgToConsole("ChessValidate: opponent just validated your state and does not match");
				EventArgs argsWithReason;
				argsWithReason.SetValue("reason", "VALIDATION_FAILED");
				Game::Event_ChessDisconnect(argsWithReason);
			}
			else {
				
				PrintInfoMsgToConsole("ChessValidate: opponent just validated your state");
				g_theAudio->StartSound(App::s_theGame->m_infoSFX, false, s_SFXVolume);
			}
		}
		else {

			if (!SpectatorCheck("validate a game", args.GetValue("fromServer", "")))return false;
			if (s_isSpectator)return false;

			if (gameState != "Player1Moving" && gameState != "Player2Moving" && gameState != "GameOver")
			{
				std::string warningMsg = Stringf("Warning: ChessValidate invalid or missing gameState (Player1Moving/Player2Moving/GameOver). Defaulting to %s.", currentGameStateLocal.c_str());
				PrintWarningMsgToConsole(warningMsg);
				gameState = currentGameStateLocal;
			}
			PrintInfoMsgToConsole("ChessValidate: validating opponent's state...");

			std::string commandLine = Stringf("ChessValidate state=%s remote=true", gameState.c_str());
			SetOutgoingData(commandLine);
		}
		return true;
	}


	//if the game is started
	std::string player1NameLocal = App::s_theGame->m_players[0].m_playerName;
	std::string player2NameLocal = App::s_theGame->m_players[1].m_playerName;
	int moveNumLocal = match->GetTurnNum();
	std::string boardAsAtringLocal = match->GetBoardStateAsString();

	int inputMoveNum = moveNum == "" ? moveNumLocal : std::stoi(moveNum);

	if (isSentByOpponent)
	{//if sent by the opponent, validate each with my data
		if (gameState != currentGameStateLocal 
			|| player1Name != player1NameLocal 
			|| player2Name != player2NameLocal 
			|| inputMoveNum != moveNumLocal
			|| boardAsAtring != boardAsAtringLocal)
		{
			PrintInfoMsgToConsole("ChessValidate: opponent just validated your state and does not match");
			EventArgs argsWithReason;
			argsWithReason.SetValue("reason", "VALIDATION_FAILED");
			Game::Event_ChessDisconnect(argsWithReason);
		}
		else {
			PrintInfoMsgToConsole("ChessValidate: opponent just validated your state");
			g_theAudio->StartSound(App::s_theGame->m_infoSFX, false, s_SFXVolume);
		}

	}
	else {//if any param is missing, fill it out for the user

		if (!SpectatorCheck("validate a game", args.GetValue("fromServer", "")))return false;
		if (s_isSpectator)return false;
		if (gameState != "Player1Moving" && gameState != "Player2Moving" && gameState != "GameOver")
		{
			std::string warningMsg = Stringf("Warning: ChessValidate invalid or missing gameState (Player1Moving/Player2Moving/GameOver). Defaulting to %s.", currentGameStateLocal.c_str());
			PrintWarningMsgToConsole(warningMsg);
			gameState = currentGameStateLocal;
		}
		if (player1Name != player1NameLocal)
		{
			std::string warningMsg = Stringf("Warning: ChessValidate invalid or missing player1Name. Defaulting to %s.", player1NameLocal.c_str());
			PrintWarningMsgToConsole(warningMsg);
			player1Name = player1NameLocal;
		}
		if (player2Name != player2NameLocal)
		{
			std::string warningMsg = Stringf("Warning: ChessValidate invalid or missing player2Name. Defaulting to %s.", player2NameLocal.c_str());
			PrintWarningMsgToConsole(warningMsg);
			player2Name = player2NameLocal;
		}
		if (inputMoveNum != moveNumLocal)
		{
			std::string warningMsg = Stringf("Warning: ChessValidate invalid or missing moveNum. Defaulting to %d.", moveNumLocal);
			PrintWarningMsgToConsole(warningMsg);
			moveNum = std::to_string(moveNumLocal);
		}
		if (boardAsAtring != boardAsAtringLocal)
		{
			std::string warningMsg = Stringf("Warning: ChessValidate invalid or missing board. Defaulting to %s.", boardAsAtringLocal.c_str());
			PrintWarningMsgToConsole(warningMsg);
			boardAsAtring = boardAsAtringLocal;
		}

		PrintInfoMsgToConsole("ChessValidate: validating opponent's state...");

		std::string commandLine = Stringf("ChessValidate state=%s player1=%s player2=%s move=%s board=%s remote=true", 
			gameState.c_str(), player1Name.c_str(), player2Name.c_str(), moveNum.c_str(), boardAsAtring.c_str());

		SetOutgoingData(commandLine);

		
	}


	return true;
}

/* [remote] ChessMove from=<square> to=<square> [promoteTo=<pieceType>]: 
Move the current player’s piece from one square to another; the move must be legal; 
it is considered a network protocol error to send an illegal move.  
If this is a pawn moving into the final (furthest) rank (row), 
the promoteTo= argument is required; otherwise, it is forbidden.
a.	Example: “ChessMove from=e2 to=e4” // you are making a move
b.	Example: “ChessMove from=c7 to=c8 promoteTo=knight” // promoting a pawn
c.	Example: “ChessMove from=e2 to=e4 remote=true” // Opponent says he moved
d.	Example: “ChessMove from=c7 to=a1 teleport=true” // ignore move rules
*/
bool Game::Event_ChessMove(EventArgs& args)
{
	

	std::string fromBoardStr = args.GetValue("from", "");
	std::string toBoardStr = args.GetValue("to", "");

	if (!IsSquareCoordsValid(fromBoardStr) || !IsSquareCoordsValid(toBoardStr))
	{
		PrintErrorMsgToConsole("Error: ChessMove invalid from and to args. name should be a player's name");
		return false;
	}

	std::string isRemote = args.GetValue("remote", "");
	int remoteValidation = GetValidationOfBooleanArgs(isRemote);
	if (remoteValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessMove invalid Remote command");
		return false;
	}
	bool isSentByMyself = remoteValidation == 0;
	std::string commandLine("ChessMove");

	std::map<std::string, std::string> allKeyValurPairs = args.GetAllValuePairs();
	std::string result = "";
	for (std::map<std::string, std::string>::const_iterator it = allKeyValurPairs.cbegin(); it != allKeyValurPairs.cend(); ++it) {
		if (it->first != "cmd" && it->first != "remote")
		{
			result.append(Stringf(" %s=%s", it->first.c_str(), it->second.c_str()));
		}
	}
	commandLine.append(result);
	
	if (isSentByMyself)//if the event is fired by myself (already succeeded on my machine
	{// if succeeded, send it to the opponent
		if (!SpectatorCheck("move a piece", args.GetValue("fromServer", "")))return false;
		if (!s_isSpectator)
		{
			commandLine.append(" remote=true");
			SetOutgoingData(commandLine);
			//SendOutgoingDataImmediately(commandLine);
		}
		else {
			bool validMove = App::s_theGame->m_currentMatch->Event_ChessMove(args);
			if (!validMove)
			{
				PrintErrorMsgToConsole("Error: ChessMove opponent invalid move.");
			}
		}
		
	}
	else {
		//as a server, you need to relay opponent's command to spectators
		SetOutgoingDataToAllSpectators(commandLine);

		PrintInfoMsgToConsole("ChessMove: opponent moved piece");
		bool validMove = App::s_theGame->m_currentMatch->Event_ChessMove(args);
		if (!validMove)
		{
			PrintErrorMsgToConsole("Error: ChessMove opponent invalid move.");
		}
	}

	return true;
}

/* [remote] ChessResign: You resign (forfeit) the current game.  
Only valid while a game is in progress; can be used on either player’s turn.  
If remote=true then opponent resigned.
a.	Example: “ChessResign” // you resign
b.	Example: “ChessResign remote=true” // Opponent is telling you that he resigned
*/
bool Game::Event_ChessResign(EventArgs& args)
{

	std::string isRemote = args.GetValue("remote", "");
	int remoteValidation = GetValidationOfBooleanArgs(isRemote);
	if (remoteValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessResign invalid Remote command");
		return false;
	}
	bool isSentByMyself = remoteValidation == 0;

	std::string commandLine("ChessResign");

	if (isSentByMyself)
	{//set current match to be opponent winning
		if (!SpectatorCheck("resign", args.GetValue("fromServer", "")))return false;
		if (!s_isSpectator)
		{
			PrintInfoMsgToConsole("ChessResign: You resigned");
			commandLine.append(" remote=true");
			SetOutgoingData(commandLine);
		}
		App::s_theGame->m_currentMatch->SetMatchResultAndEndTheGame(!s_myPlayerIndex);
	}
	else {//if opponent resigned, set current match to be me winning

		//as a server, you need to relay opponent's command to spectators
		SetOutgoingDataToAllSpectators(commandLine);

		PrintInfoMsgToConsole("ChessResign: Your opponent resigned");
		App::s_theGame->m_currentMatch->SetMatchResultAndEndTheGame(s_myPlayerIndex);
		g_theAudio->StartSound(App::s_theGame->m_infoSFX, false, s_SFXVolume);
	}

	return true;
}

/* [remote] ChessOfferDraw: 
You offer your opponent a draw (mutually-agreed tie game).  
Only valid while a game is in progress; can be used on either player’s turn.  
You may wish to limit each player’s ability to send this message more than once per turn; 
this rejection should happen locally.
a.	Example: “ChessOfferDraw” // you are offering a draw (tie game)
b.	Example: “ChessOfferDraw remote=true” // Opponent is offering you a draw (tie)
*/
bool Game::Event_ChessOfferDraw(EventArgs& args)
{

	std::string isRemote = args.GetValue("remote", "");
	int remoteValidation = GetValidationOfBooleanArgs(isRemote);
	if (remoteValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessOfferDraw invalid Remote command");
		return false;
	}
	bool isSentByMyself = remoteValidation == 0;

	

	//if a game has not started yet
	if (App::s_theGame->m_currentMatch == nullptr)
	{
		PrintErrorMsgToConsole("Error: ChessOfferDraw there is no ongoing match");
		return false;
	}

	if (App::s_theGame->m_canAcceptOrRejectDraw)
	{//you previously received draw offer from opponent in the same turn
		PrintInfoMsgToConsole("You are offering draw to opponent who has also offered draw");
		EventArgs tempArgs;
		Event_ChessAcceptDraw(tempArgs);
		return false;
	}

	std::string commandLine("ChessOfferDraw");

	if (isSentByMyself)
	{
		if (!SpectatorCheck("offer draw", args.GetValue("fromServer", "")))return false;
		if (!s_isSpectator)
		{
			commandLine.append(" remote=true");
			SetOutgoingData(commandLine);
		}
	}
	else {

		//as a server, you need to relay opponent's command to spectators
		SetOutgoingDataToAllSpectators(commandLine);

		PrintInfoMsgToConsole("Your opponent offered draw.");
		App::s_theGame->m_canAcceptOrRejectDraw = true;
		g_theAudio->StartSound(App::s_theGame->m_infoSFX, false, s_SFXVolume);
	}

	return true;
}

/* [remote] ChessAcceptDraw and ChessRejectDraw: 
You accept (or reject) your opponent’s offer of a draw (mutually-agreed tie game).  
Only valid while 
	a game is in progress, and 
	after your opponent has just sent you a ChessOfferDraw message (with remote=true).
a.	Example: “ChessAcceptDraw” // I accept my opponent’s offer to draw (tie) the game
b.	Example: “ChessRejectDraw” // I reject their offer
c.	Example: “ChessAcceptDraw remote=true” // My opponent accepted my offer to draw
*/
bool Game::Event_ChessAcceptDraw(EventArgs& args)
{

	std::string isRemote = args.GetValue("remote", "");
	int remoteValidation = GetValidationOfBooleanArgs(isRemote);
	if (remoteValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessAcceptDraw invalid Remote command");
		return false;
	}
	bool isSentByMyself = remoteValidation == 0;

	std::string commandLine("ChessAcceptDraw");

	if (isSentByMyself)
	{// I accept my opponent’s offer to draw (tie) the game
		if (!SpectatorCheck("accept draw", args.GetValue("fromServer", "")))return false;
		
		if (!App::s_theGame->m_canAcceptOrRejectDraw)
		{
			PrintErrorMsgToConsole("Error: ChessAcceptDraw do not have permission to accept draw");
			return false;
		}
		if (!s_isSpectator)
		{
			commandLine.append(" remote=true");
			SetOutgoingData(commandLine);
		}
			PrintInfoMsgToConsole("You accepted draw, game ends.");
			App::s_theGame->m_canAcceptOrRejectDraw = false;
		
	}
	else {// My opponent accepted my offer to draw

		//as a server, you need to relay opponent's command to spectators
		SetOutgoingDataToAllSpectators(commandLine);

		PrintInfoMsgToConsole("Your opponent accepted draw, game ends.");
		g_theAudio->StartSound(App::s_theGame->m_infoSFX, false, s_SFXVolume);
	}
	
	App::s_theGame->m_currentMatch->SetMatchResultAndEndTheGame(2);

	return true;
}
bool Game::Event_ChessRejectDraw(EventArgs& args)
{
	std::string isRemote = args.GetValue("remote", "");
	int remoteValidation = GetValidationOfBooleanArgs(isRemote);
	if (remoteValidation == -1)
	{
		PrintErrorMsgToConsole("Error: ChessRejectDraw invalid Remote command");
		return false;
	}
	bool isSentByMyself = remoteValidation == 0;

	std::string commandLine("ChessRejectDraw");

	if (isSentByMyself)
	{// I reject my opponent’s offer to draw (tie) the game
		if (!SpectatorCheck("reject draw", args.GetValue("fromServer", "")))return false;

		if (!App::s_theGame->m_canAcceptOrRejectDraw)
		{
			PrintErrorMsgToConsole("Error: ChessRejectDraw do not have permission to reject draw");
			return false;
		}
		if (!s_isSpectator)
		{
			commandLine.append(" remote=true");
			SetOutgoingData(commandLine);
		}
		PrintInfoMsgToConsole("You rejected draw.");
		App::s_theGame->m_canAcceptOrRejectDraw = false;

	}
	else {// My opponent reject my offer to draw

		//as a server, you need to relay opponent's command to spectators
		SetOutgoingDataToAllSpectators(commandLine);

		PrintInfoMsgToConsole("Your opponent rejected draw.");
		g_theAudio->StartSound(App::s_theGame->m_infoSFX, false, s_SFXVolume);
	}

	return true;
}



bool Game::SetOutgoingData(std::string const& data)
{
	ConnectionState currentState = g_theNetwork->GetConnectionState();
	switch (currentState)
	{
	case ConnectionState::IDLE:
		PrintErrorMsgToConsole("Error: invalid network connection state: idle");
		return false;
	case ConnectionState::NS_CLIENT_CONNECTING:
		PrintErrorMsgToConsole("Error: invalid network connection state: connecting");
		return false;
	case ConnectionState::NS_SERVER_LISTENING:
	{//server to client
		g_theNetwork->SetOutgoingDataToAll(data);
		break;
	}
	case ConnectionState::NS_CLIENT_CONNECTED:
	{//client to server
		g_theNetwork->SetOutgoingDataToIndex(data);
		break;
	}
	case ConnectionState::UNINITIALIZED:
	case ConnectionState::COUNT:
	default:
	{
		PrintErrorMsgToConsole("Error: invalid network connection state: uninitialized");
		return false;
	}

	}
	return true;
}

bool Game::SetOutgoingDataToAllSpectators(std::string const& data)
{
	std::string dataModifiedByServer = data;
	dataModifiedByServer.append(" fromServer=true");
	ConnectionState currentState = g_theNetwork->GetConnectionState();
	switch (currentState)
	{
	case ConnectionState::IDLE:
		PrintErrorMsgToConsole("Error: invalid network connection state: idle");
		return false;
	case ConnectionState::NS_CLIENT_CONNECTING:
		PrintErrorMsgToConsole("Error: invalid network connection state: connecting");
		return false;
	case ConnectionState::NS_SERVER_LISTENING:
	{//server to client
		g_theNetwork->SetOutgoingDataToAllButIndex(dataModifiedByServer, 0);
		break;
	}
	case ConnectionState::NS_CLIENT_CONNECTED:
	{//client to server: do nothing, it is not client's responsibility to relay commands
		//g_theNetwork->SetOutgoingDataToIndex(data);
		break;
	}
	case ConnectionState::UNINITIALIZED:
	case ConnectionState::COUNT:
	default:
	{
		PrintErrorMsgToConsole("Error: invalid network connection state: uninitialized");
		return false;
	}

	}
	return true;
}

bool Game::SendOutgoingDataImmediately(std::string const& data, bool disconnectRightAfter)
{
	ConnectionState currentState = g_theNetwork->GetConnectionState();
	switch (currentState)
	{
	case ConnectionState::IDLE:
		PrintErrorMsgToConsole("Error: invalid network connection state: idle. No server/client to send the data.");
		return false;
	case ConnectionState::NS_SERVER_LISTENING:
	{//server to client
		g_theNetwork->SendDataToClientsOnceOnly(data);
		if (disconnectRightAfter)g_theNetwork->ServerDisconnect();
		break;
	}
	case ConnectionState::NS_CLIENT_CONNECTING: {
		PrintErrorMsgToConsole("Error: invalid network connection state: connecting. No server to send the data.");
		if (disconnectRightAfter)g_theNetwork->ClientDisconnect();
		break;
	}
	case ConnectionState::NS_CLIENT_CONNECTED:
	{//client to server
		g_theNetwork->SendDataToServerOnceOnly(data);
		if (disconnectRightAfter)g_theNetwork->ClientDisconnect();
		break;
	}
	case ConnectionState::UNINITIALIZED:
	case ConnectionState::COUNT:
	default:
	{
		PrintErrorMsgToConsole("Error: invalid network connection state: uninitialized. Data will not be sent.");
		return false;
	}

	}
	return true;
}

bool Game::SendOutgoingDataImmediatelyToAllSpectators(std::string const& data, bool disconnectRightAfter)
{
	ConnectionState currentState = g_theNetwork->GetConnectionState();
	switch (currentState)
	{
	case ConnectionState::IDLE:
		PrintErrorMsgToConsole("Error: invalid network connection state: idle. No server/client to send the data.");
		return false;
	case ConnectionState::NS_SERVER_LISTENING:
	{//server to client
		g_theNetwork->SendDataToClientsOnceOnlyExceptIndex(data,0);
		if (disconnectRightAfter)g_theNetwork->ServerDisconnect();
		break;
	}
	case ConnectionState::NS_CLIENT_CONNECTING: {
		//PrintErrorMsgToConsole("Error: invalid network connection state: connecting. No server to send the data.");
		if (disconnectRightAfter)g_theNetwork->ClientDisconnect();
		break;
	}
	case ConnectionState::NS_CLIENT_CONNECTED:
	{//client to server
		//g_theNetwork->SendDataToServerOnceOnly(data);
		if (disconnectRightAfter)g_theNetwork->ClientDisconnect();
		break;
	}
	case ConnectionState::UNINITIALIZED:
	case ConnectionState::COUNT:
	default:
	{
		PrintErrorMsgToConsole("Error: invalid network connection state: uninitialized. Data will not be sent.");
		return false;
	}

	}
	return true;
}

bool Game::SpectatorCheck(std::string const& actionStr, std::string const& fromServerStr)
{
	if (s_isSpectator && g_theNetwork->IsClientConnectedToServer())
	{//spectator are not allowed to play
		int isSentFromServer = GetValidationOfBooleanArgs(fromServerStr);
		if (isSentFromServer == 1)
		{//sending from server, print info message
			std::string infoMsg = Stringf("The Client is trying to %s in a game that you are spectating.", actionStr.c_str());
			PrintInfoMsgToConsole(infoMsg);
			return true;
		}
		else {
			std::string errorMsg = Stringf("Error: You are trying to %s in a game that you are spectating.", actionStr.c_str());
			PrintErrorMsgToConsole(errorMsg);
			g_theAudio->StartSound(App::s_theGame->m_errorSFX, false, s_SFXVolume);
			return false;
		}
	}
	return true;
}

std::string GetRenderModeStr(int debugRenderMode)
{
	switch (debugRenderMode)
	{
	case 0:
		return "Lit with normal";
	case 1:
		return "Vertex color";
	case 2:
		return "Model constant color";
	case 3:
		return "UV color";
	case 4:
		return "Model Tangent";
	case 5:
		return "Model Bitangent";
	case 6:
		return "Model Normal";
	case 7:
		return "Diffuse texture color";
	case 8:
		return "Normal texture color";
	case 9:
		return "Pixel Normal TBN Space";
	case 10:
		return "Pixel Normal World Space";
	case 11:
		return "Lit no normal map";
	case 12:
		return "Diffuse light intensity";
	case 13:
		return "Specular light intensity";
	case 14:
		return "World Tangent";
	case 15:
		return "World Bitangent";
	case 16:
		return "World Normal";
	case 17:
		return "Model to world I (forward) basis vector, in world spacce (I)";
	case 18:
		return "Model to world J (left) basis vector, in world spacce (J)";
	case 19:
		return "Model to world K (up) basis vector, in world spacce (K)";
	case 20:
		return "Emissive Color Only";
	default:
		return "??(Unused)";
	}
}
