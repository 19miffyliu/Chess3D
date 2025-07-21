#pragma once

#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Mat44.hpp"

#include <vector>
#include <string>
class Game;


class Entity
{
public:
	Entity();
	Entity(Game* owner);
	virtual ~Entity();

	virtual void Update(float deltaSeconds) = 0;
	virtual void Render()const = 0;

	virtual Mat44 GetModelToWorldTransform()const;

	//get functions
	Mat44 GetEntityTransform()const;
	std::string GetPositionAsString()const;
	std::string GetOrientationAsString()const;


public:
	Game* m_game = nullptr;
	Vec3 m_position = Vec3();
	Vec3 m_velocity = Vec3();
	EulerAngles m_orientation = EulerAngles();
	EulerAngles m_angularVelocity = EulerAngles();
	Rgba8 m_color = Rgba8::WHITE;
};

typedef std::vector<Entity*> EntityList;