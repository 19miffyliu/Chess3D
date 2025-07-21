#include "Game/Entity.hpp"
#include "Engine/Core/StringUtils.hpp"

Entity::Entity()
{
}

Entity::Entity(Game* owner)
:m_game(owner){
}

Entity::~Entity()
{
	m_game = nullptr;
}

Mat44 Entity::GetModelToWorldTransform() const
{
	Mat44 mat;
	mat.SetTranslation3D(m_position);
	mat.Append(m_orientation.GetAsMatrix_IFwd_JLeft_KUp());

	return mat;
}

Mat44 Entity::GetEntityTransform() const
{
	return Mat44::MakeTranslationThenOrientation(m_position, m_orientation);
}

std::string Entity::GetPositionAsString() const
{
	std::string pos = Stringf("%.02f, %.02f, %.02f", m_position.x, m_position.y, m_position.z);
	return pos;
}

std::string Entity::GetOrientationAsString() const
{
	std::string orientation = Stringf("%.02f, %.02f, %.02f",
		m_orientation.m_yawDegees,
		m_orientation.m_pitchDegees,
		m_orientation.m_rollDegees);

	return orientation;
}
