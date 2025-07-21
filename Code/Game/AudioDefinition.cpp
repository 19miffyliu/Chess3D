#include "Game/AudioDefinition.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"

#include <filesystem>

std::vector<AudioDefinition*> AudioDefinition::s_allAudioDefinitions;
std::vector<AudioDefinition*> AudioDefinition::s_BGMAudioDefinitions;
std::vector<AudioDefinition*> AudioDefinition::s_SFXAudioDefinitionsByType[(int)SFXType::COUNT];

AudioDefinition::AudioDefinition()
{
}

AudioDefinition::~AudioDefinition()
{
}

void AudioDefinition::InitializeDefinitions(const char* path)
{
	XmlDocument document;
	XmlError result = document.LoadFile(path);
	if (result != 0)
	{
		ERROR_AND_DIE("AudioDefinition::InitializeDefinitions: Error loading file");
	}
	XmlElement* rootElement = document.RootElement();

	std::string BGMFolderPath = ParseXmlAttribute(*rootElement, "BGMPath", "?");
	if (BGMFolderPath != "")
	{
		LoadAllBGMsFromThisPath(BGMFolderPath.c_str());
	}

	std::string SFXFolderPath = ParseXmlAttribute(*rootElement, "SFXPath", "?");
	XmlElement* audioDefElement = rootElement->FirstChildElement();
	


	while (audioDefElement != nullptr)
	{
		AudioDefinition* tempDef = nullptr;
		bool isGrouped = ParseXmlAttribute(*audioDefElement, "isGrouped", false);
		if (isGrouped)
		{
			int startIndex = ParseXmlAttribute(*audioDefElement, "groupStartIndex", -1);
			int endIndex = ParseXmlAttribute(*audioDefElement, "groupEndIndex", -1);
			if (startIndex != -1 && startIndex != endIndex)
			{
				for (int i = startIndex; i <= endIndex; i++)
				{
					tempDef = PushBackNewDef(*audioDefElement);
					if (tempDef != nullptr)
					{
						tempDef->m_fileName.append(GetNumberWithDigits(i, 2));

						std::string folderPath = tempDef->m_isBGM ? BGMFolderPath : SFXFolderPath;
						std::string filePath = folderPath + tempDef->m_fileName + tempDef->m_fileSuffix;

						tempDef->m_soundID = g_theAudio->CreateOrGetSound(filePath);
					}
				}
			}


		}
		else {
			tempDef = PushBackNewDef(*audioDefElement);
			
			if (tempDef != nullptr)
			{
				std::string folderPath = tempDef->m_isBGM ? BGMFolderPath : SFXFolderPath;
				std::string filePath = folderPath + tempDef->m_fileName + tempDef->m_fileSuffix;

				tempDef->m_soundID = g_theAudio->CreateOrGetSound(filePath);
			}
		}

		
		

		audioDefElement = audioDefElement->NextSiblingElement();
	}
}

void AudioDefinition::LoadAllBGMsFromThisPath(const char* path)
{
	std::filesystem::path folderPath = path;
	if (std::filesystem::exists(folderPath) && std::filesystem::is_directory(folderPath))
	{
		for (const std::filesystem::directory_entry& entry :
			std::filesystem::directory_iterator(folderPath)) {
			AudioDefinition* audioDef = new AudioDefinition();
			audioDef->m_isBGM = true;
			std::string BGMFilePath = entry.path().string();
			std::size_t foundLastName = BGMFilePath.find_last_of("/\\");
			std::size_t foundSuffix = BGMFilePath.find_last_of(".");
			audioDef->m_fileName = BGMFilePath.substr(foundLastName + 1, foundSuffix - foundLastName - 1);
			audioDef->m_fileSuffix = BGMFilePath.substr(foundSuffix);
			audioDef->m_soundID = g_theAudio->CreateOrGetSound(BGMFilePath);
			s_allAudioDefinitions.push_back(audioDef);
			s_BGMAudioDefinitions.push_back(audioDef);
			g_numAssetsLoaded++;

		}
	}




	

	
}

void AudioDefinition::ClearDefinitions()
{

	s_BGMAudioDefinitions.clear();

	int numSFXTypes = (int)SFXType::COUNT;
	for (int i = 0; i < numSFXTypes; i++)
	{
		s_SFXAudioDefinitionsByType[i].clear();
	}

	int numAllAudio = (int)s_allAudioDefinitions.size();
	for (int i = 0; i < numAllAudio; i++)
	{
		delete s_allAudioDefinitions[i];
		s_allAudioDefinitions[i] = nullptr;
	}

	s_allAudioDefinitions.clear();
}

AudioDefinition* AudioDefinition::PushBackNewDef(const XmlElement& element)
{
	AudioDefinition* audioDef = new AudioDefinition();
	audioDef->LoadFromXmlElement(element);

	s_allAudioDefinitions.push_back(audioDef);

	if (audioDef->m_isBGM)
	{
		s_BGMAudioDefinitions.push_back(audioDef);
	}
	else {
		s_SFXAudioDefinitionsByType[(int)audioDef->m_SFXType].push_back(audioDef);
	}

	g_numAssetsLoaded++;
	return audioDef;
}

const SoundID AudioDefinition::GetRandomBGM()
{
	int numBGMInThisPhase = (int)s_BGMAudioDefinitions.size();

	if (numBGMInThisPhase < 1)
	{
		return MISSING_SOUND_ID;
	}
	
	int randomIndex = g_randGen.RollRandomIntInRange(0, numBGMInThisPhase - 1);
	return s_BGMAudioDefinitions[randomIndex]->m_soundID;
}

const SoundID AudioDefinition::GetRandomSFXByType(SFXType type)
{
	int sfxType = (int)type;
	int numSFXOfThisType = (int)s_SFXAudioDefinitionsByType[sfxType].size();

	if (numSFXOfThisType < 1)
	{
		return MISSING_SOUND_ID;
	}

	int randomIndex = 0;

	//get random index that fit criteria, ignore if not human hurt
	randomIndex = g_randGen.RollRandomIntInRange(0, numSFXOfThisType - 1);
	
	return s_SFXAudioDefinitionsByType[sfxType][randomIndex]->m_soundID;
}

const AudioDefinition* AudioDefinition::GetAudioDefBySoundID(SoundID id)
{
	int numAllSound = (int)s_allAudioDefinitions.size();
	for (int i = 0; i < numAllSound; i++)
	{
		if (s_allAudioDefinitions[i]->m_soundID == id)
		{
			return s_allAudioDefinitions[i];
		}
	}
	return nullptr;
}

bool AudioDefinition::LoadFromXmlElement(XmlElement const& element)
{

	m_isBGM = ParseXmlAttribute(element,"isBGM", false);

	m_fileName = ParseXmlAttribute(element, "fileName", "?");
	GUARANTEE_OR_DIE(m_fileName != "?", "Cannot find file name for audio");

	m_fileSuffix = ParseXmlAttribute(element, "fileSuffix", "?");

	std::string sfxType = ParseXmlAttribute(element, "SFXType", "?");
	if (sfxType == "GameOver") {
		m_SFXType = SFXType::GAMEOVER;
	}
	else if (sfxType == "GameStart") {
		m_SFXType = SFXType::GAMESTART;
	}
	else if (sfxType == "Move") {
		m_SFXType = SFXType::MOVE;
	}
	else if (sfxType == "InfoMessage") {
		m_SFXType = SFXType::INFOMSG;
	}
	else if (sfxType == "ErrorMessage") {
		m_SFXType = SFXType::ERRORMSG;
	}

	return false;
}

void PlaySoundIfNotAlreadyPlaying(SoundID soundId, SoundPlaybackID& soundPlaybackId, float volume)
{
	if (soundPlaybackId == MISSING_SOUND_ID || !g_theAudio->IsPlaying(soundPlaybackId))
	{
		soundPlaybackId = g_theAudio->StartSound(soundId, false, volume);
	}
}
