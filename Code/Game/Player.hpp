#pragma once

#include "Engine/Core/Rgba8.hpp"

#include <string>

class Player
{
public:
	Player();
	Player(int playerIndex, std::string const& name, Rgba8 const& color);
	~Player();

public:

	int m_playerIndex = 0;
	std::string m_playerName;
	Rgba8 m_playerColor = Rgba8::WHITE;

};

