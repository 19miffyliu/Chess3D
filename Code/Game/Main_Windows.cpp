#include "Game/GameCommon.hpp"
#include "Game/App.hpp"
#include "Engine/Core/EngineCommon.hpp"


#define WIN32_LEAN_AND_MEAN		// Always #define this before #including <windows.h>
#include <windows.h>			// #include this (massive, platform-specific) header in VERY few places (and .CPPs only)
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Core/StringUtils.hpp"

NamedStrings g_gameconfigBlackBoard;
App* g_theApp = nullptr;

//-----------------------------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE applicationInstanceHandle, HINSTANCE, LPSTR commandLineString, int )
{
	//insert command line args into game config blackboard
	Strings cmdLines = SplitStringOnDelimiter(std::string(commandLineString), ' ', false);
	int numCommands = (int)cmdLines.size();
	if (numCommands > 0 && cmdLines[0].find('=') != std::string::npos)
	{
		for (int i = 0; i < numCommands; i++)
		{
			Strings cmdPairs = SplitStringOnDelimiter(cmdLines[i], '=', false);
			g_gameconfigBlackBoard.SetValue(cmdPairs[0], cmdPairs[1]);
		}
	}

	UNUSED(applicationInstanceHandle);
	//UNUSED(commandLineString);

	g_theApp = new App();
	g_theApp->Startup();


	// Program main loop; keep running frames until it's time to quit
	g_theApp->RunMainLoop();

	
	g_theApp->Shutdown();
	delete g_theApp;
	g_theApp = nullptr;

	return 0;
}


