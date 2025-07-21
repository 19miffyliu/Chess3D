#include "Game/Player.hpp"


Player::Player()
{
}

Player::Player(int playerIndex, std::string const& name, Rgba8 const& color)
:m_playerIndex(playerIndex),m_playerName(name),m_playerColor(color){
}

Player::~Player()
{
}




