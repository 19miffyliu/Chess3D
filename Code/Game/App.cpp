#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"

#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/NetworkSystem/NetworkSystem.hpp"


Renderer* g_theRenderer = nullptr;
InputSystem* g_theInput = nullptr;
AudioSystem* g_theAudio = nullptr;
Window* g_theWindow = nullptr;
BitmapFont* g_theFont = nullptr;
Clock* g_theSysClock = nullptr;
NetworkSystem* g_theNetwork = nullptr;

RandomNumberGenerator g_randGen;

Game* App::s_theGame;

App::App()
{
}

App::~App()
{
}

void App::Startup()
{
	EventSystemConfig eventConfig;
	g_theEventSystem = new EventSystem(eventConfig);

	InputConfig inputconfig;
	g_theInput = new InputSystem(inputconfig);
	

	
	//bool isServer = g_gameconfigBlackBoard.GetValue("isServer", true);
	std::string programName("Chess3D");
	//if (isServer)programName = "Chess3D_Server";
	//else programName = "Chess3D_Client";


	WindowConfig windowConfig;
	windowConfig.m_aspectRatio = 2.f;
	windowConfig.m_inputSystem = g_theInput;
	windowConfig.m_windowtitle = programName;
	g_theWindow = new Window(windowConfig);

	RendererConfig rendererConfig;
	rendererConfig.m_window = g_theWindow;
	g_theRenderer = new Renderer(rendererConfig);

	g_theSysClock = new Clock();

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_theRenderer;
	debugRenderConfig.m_fontName = "Data/Fonts/SquirrelFixedFont";

	
	DevConsoleConfig devConsoleConfig;
	devConsoleConfig.m_fontName = "Data/Fonts/SquirrelFixedFont";
	devConsoleConfig.m_renderer = g_theRenderer;
	g_theDevConsole = new DevConsole(devConsoleConfig);

	NetworkConfig networkConfig;
	networkConfig.m_serverIPAddress = g_gameconfigBlackBoard.GetValue("serverIPAddr", "127.0.0.1");
	networkConfig.m_serverListeningPort = (unsigned short)g_gameconfigBlackBoard.GetValue("serverListeningPort", 3100);
	g_theNetwork = new NetworkSystem(networkConfig);
	

	AudioConfig audioConfig;
	g_theAudio = new AudioSystem(audioConfig);

	//start up functions
	g_theEventSystem->Startup();
	g_theWindow->Startup();
	g_theRenderer->Startup();
	g_theDevConsole->Startup();
	g_theInput->Startup();
	g_theAudio->Startup();

	//network start up
	g_theNetwork->Startup();
	
	


	IntVec2 windowsDim = g_theWindow->GetClientDimensions();
	g_gameconfigBlackBoard.SetValue("screenSizeX", std::to_string(windowsDim.x));
	g_gameconfigBlackBoard.SetValue("screenSizeY", std::to_string(windowsDim.y));

	g_theEventSystem->SubscribeEventCallbackFunction("quit", Event_Quit);
	DebugRenderSystemStartup(debugRenderConfig);


	s_theGame = new Game();
	s_theGame->Startup();

}



void App::Shutdown()
{
	s_theGame->Shutdown();
	delete s_theGame;
	s_theGame = nullptr;


	DebugRenderSystemShutdown();

	//shut down the engine
	g_theNetwork->Shutdown();
	g_theAudio->Shutdown();
	g_theDevConsole->Shutdown();
	g_theRenderer->Shutdown();
	g_theWindow->Shutdown();
	g_theInput->Shutdown();
	g_theEventSystem->Shutdown();
	


	//delete engine
	delete g_theNetwork;
	g_theNetwork = nullptr;
	delete g_theAudio;
	g_theAudio = nullptr;
	delete g_theDevConsole;
	g_theDevConsole = nullptr;
	delete g_theRenderer;
	g_theRenderer = nullptr;
	delete g_theWindow;
	g_theWindow = nullptr;
	delete g_theInput;
	g_theInput = nullptr;
	delete g_theEventSystem;
	g_theEventSystem = nullptr;

	
	delete g_theSysClock;
	g_theSysClock = nullptr;

	
	
}

void App::RunFrame()
{
	BeginFrame();
	Update();
	Render();		
	EndFrame();		
}

void App::RunMainLoop()
{
	while (!IsQuitting())
	{
		RunFrame(); 
	}
}

bool App::IsQuitting() const
{
	return m_isQuitting;
}

bool App::HandleQuitRequested()
{
	m_isQuitting = true;
	return true;
}

void App::BeginFrame()
{
	g_theSysClock->TickSystemClock();

	g_theEventSystem->BeginFrame();
	g_theInput->BeginFrame();
	g_theWindow->BeginFrame();
	g_theRenderer->BeginFrame();
	g_theDevConsole->BeginFrame();
	g_theAudio->BeginFrame();
	g_theNetwork->BeginFrame();

	DebugRenderBeginFrame();
	
}

void App::Update()
{
	UpdateMouse();

	s_theGame->Update();

	
}

void App::Render() const
{

	//clear screen
	Rgba8 clearColor(0, 127, 127, 255);
	g_theRenderer->ClearScreen(clearColor);


	//render game
	s_theGame->Render();


}

void App::EndFrame()
{	
	DebugRenderEndFrame();

	g_theNetwork->EndFrame();
	g_theAudio->EndFrame();
	g_theDevConsole->EndFrame();
	g_theRenderer->EndFrame();
	g_theWindow->EndFrame();
	g_theInput->EndFrame();
	g_theEventSystem->EndFrame();
}

void App::UpdateMouse()
{
	if (!g_theWindow->IsInFocus()
		||g_theDevConsole->GetMode() == DevConsoleMode::OPEN_FULL
		||s_theGame->IsInAttractMode()) {
		g_theInput->SetCursorMode(CursorMode::POINTER);
	}
	else {
		g_theInput->SetCursorMode(CursorMode::FPS);
	}
}

void App::RestartGame()
{
	delete s_theGame;
	s_theGame = nullptr;

	s_theGame = new Game();
	s_theGame->Startup();
}

bool App::Event_Quit(EventArgs& args)
{
	UNUSED(args);
	return g_theApp->HandleQuitRequested();
}




