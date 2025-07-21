#pragma once

#include "Game/GameCommon.hpp"


#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Audio/AudioSystem.hpp"

#include <vector>
#include <string>


enum class SFXType {
	INVALID = -1,
	GAMEOVER,
	GAMESTART,
	MOVE,
	INFOMSG,
	ERRORMSG,
	COUNT
};


void PlaySoundIfNotAlreadyPlaying(SoundID soundId, SoundPlaybackID& soundPlaybackId, float volume);

struct AudioDefinition
{
public:
	AudioDefinition();
	~AudioDefinition();

	static void InitializeDefinitions(const char* path);
	static void LoadAllBGMsFromThisPath(const char* path);
	static void ClearDefinitions();
	static AudioDefinition* PushBackNewDef(const XmlElement& element);

	static const SoundID GetRandomBGM();
	static const SoundID GetRandomSFXByType(SFXType type);
	static const AudioDefinition* GetAudioDefBySoundID(SoundID id);

	static std::vector<AudioDefinition*> s_allAudioDefinitions;
	static std::vector<AudioDefinition*> s_BGMAudioDefinitions;
	static std::vector<AudioDefinition*> s_SFXAudioDefinitionsByType[(int)SFXType::COUNT];


	bool LoadFromXmlElement(XmlElement const& element);

public:
	std::string m_fileName;
	std::string m_fileSuffix;
	SoundID m_soundID = MISSING_SOUND_ID;

	SFXType m_SFXType = SFXType::INVALID;

	bool m_isBGM = false;

	

};

