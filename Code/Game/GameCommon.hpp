#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

#include <string>
#include <atomic>
#include <thread>

//extern variables
class Renderer;
extern Renderer* g_theRenderer;

class App;
extern App* g_theApp;

class AudioSystem;
extern AudioSystem* g_theAudio;

class NetworkSystem;
extern NetworkSystem* g_theNetwork;

class RandomNumberGenerator;
extern RandomNumberGenerator g_randGen;

//atomic variables
extern std::atomic<bool> g_isAssetLoadingCompleted;
extern std::atomic<int> g_numAssetsLoaded;

//screen camera
constexpr float SCREEN_SIZE_X = 1600.f;
constexpr float SCREEN_SIZE_Y = 800.f;

//debug drawing
constexpr float DEBUG_THICKNESS = 0.05f;
constexpr int DEBUG_SIDES = 36;
constexpr float TEXT_HEIGHT = 20.f;
constexpr float TEXT_HEIGHT_LARGE = 40.f;

//drawing dot variables
constexpr float sqrt3by2 = 0.86602540378f;
constexpr float sqrt2by2 = 0.707106781186f;

void DebugDrawRing(Vec2 const& center, float radius, float thickness, Rgba8 const& color);
void DebugDrawLine(Vec2 const& start, Vec2 const& end, Rgba8 color, float thickness);
void DebugDrawDot(Vec2 const& center, float radius, Rgba8 color);//draw a triangle
void DebugDrawRectangle(Vec2 const& bottomLeft, Vec2 const& topRight, Rgba8 color);
void DebugDrawBox(Vec2 const& bottomLeft, Vec2 const& topRight, Rgba8 color, float thickness);

//game specific
constexpr float BOARD_HEIGHT = 0.33f;

bool IsSquareCoordsValid(std::string const& squareCoords);
bool IsBoardCoordsValid(IntVec2 const& boardCoords);
IntVec2 GetBoardCoordsForUserString(std::string const& squareCoords);
int GetBoardStateIndexForUserString(std::string const& squareCoords);
std::string GetSquareCoordsForBoardCoords(IntVec2 const& boardCoords);

IntVec2 GetBoardCoordsForBoardStateIndex(int index);
int GetBoardStateIndexForBoardCoords(IntVec2 const& boardCoords);
Vec3 GetWorldCoordForBoardCoords(IntVec2 const& boardCoords);

bool PrintErrorMsgToConsole(std::string const& errorMsg);
bool PrintWarningMsgToConsole(std::string const& warningMsg);
bool PrintInfoMsgToConsole(std::string const& infoMsg);

