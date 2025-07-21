#pragma once
#include "Game/Widget.hpp"

#include "Engine/Core/Rgba8.hpp"

#include <string>


enum class ButtonType {
	INVALID = -1,
	CONNECT_CLIENT,
	CONNECT_SPECTATOR,
	LISTEN_SERVER,
	DISCONNECT,
	SWITCH_BGM,
	CHESS_BEGIN,
	QUIT,
	COUNT
};

struct Button : public Widget
{
public:
	Button();
	Button(AABB2 const& widgetWindow, Rgba8 const& color, std::string const& text, Rgba8 const& textColor, ButtonType type);
	~Button();

	void Startup()override;
	void Update(float deltaSeconds)override;
	void Render()const override;

	void InitLocalVerts()override;
	void CreateBuffers()override;
	void SetBGColor(Rgba8 const& color)override;


	int GetUniqueWidgetType()const override;

public:
	ButtonType m_buttonType = ButtonType::INVALID;
	Rgba8 m_buttonColor = Rgba8::WHITE;
	std::string m_buttonText;
	Rgba8 m_buttonTextColor = Rgba8::BLACK;



};

