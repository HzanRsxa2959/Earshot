#include "plugin.h"
//>
#include <filesystem>
#include "CModelInfo.h"
#include "CCamera.h"
#include "CAEAudioHardware.h"
#include <map>
#include "CTimer.h"
#include "include/irrKlang-1.6.0/include/irrKlang.h"
#include "CMenuManager.h"
#define SUBHOOK_STATIC
#include "include/subhook-0.8.2/subhook.h"
#include "CAEPedAudioEntity.h"
//<

using namespace plugin;
//>
using namespace std;
namespace fs = filesystem;

auto modname = string("EarShot");
auto modMessage(string messagetext, UINT messageflags = MB_OK) {
	return MessageBox(GetActiveWindow(), messagetext.c_str(), modname.c_str(), messageflags);
}
auto modextension = string(".earshot");

auto caseLower(string casedstring) {
	auto caselessstring = string();
	for (auto casedcharacter : casedstring) {
		if (
				casedcharacter >= 'A'
			&&	casedcharacter <= 'Z'
			) {
			casedcharacter = tolower(casedcharacter);
		}
		caselessstring += casedcharacter;
	}
	return caselessstring;
}
auto caseUpper(string casedstring) {
	auto caselessstring = string();
	for (auto casedcharacter : casedstring) {
		if (
			casedcharacter >= 'a'
			&&	casedcharacter <= 'z'
			) {
			casedcharacter = toupper(casedcharacter);
		}
		caselessstring += casedcharacter;
	}
	return caselessstring;
}

auto audiosplaying = vector<pair<irrklang::ISound *, CPhysical *>>();

auto audioengine = (irrklang::ISoundEngine *)nullptr;

auto cameraposition = (CVector *)nullptr;
auto audioState(irrklang::ISound *audiostream, CPhysical *audioentity) {
	if (audioentity) {
		auto audioposition = &audioentity->GetPosition();
		audiostream->setPosition(irrklang::vec3df(audioposition->x, audioposition->y, audioposition->z));
	}

	auto audiovolume = AEAudioHardware.m_fEffectMasterScalingFactor;

	auto audiolocation = &audiostream->getPosition();
	auto audiodistance = CVector(
			cameraposition->x - audiolocation->X
		,	cameraposition->y - audiolocation->Y
		,	cameraposition->z - audiolocation->Z
	).Magnitude();
	audiodistance = clamp<float>(audiodistance, 0.0f, 150.0f);
	auto audiofactor = (150.0f - audiodistance) / 150.0f;
	audiofactor = pow(audiofactor, 6.0f);
	audiovolume *= audiofactor;

	audiostream->setVolume(audiovolume);

	audiostream->setPlaybackSpeed(CTimer::GetIsSlowMotionActive() ? 0.5f : 1.0f);
}

auto folderroot = fs::path(GAME_PATH(""));
auto foldermod = fs::path(PLUGIN_PATH("")) / fs::path(modname);
auto folderdata = folderroot / fs::path("data") / fs::path(modname);

auto rootlength = folderroot.string().length();
auto outputPath(fs::path *filepath) {
	return (filepath->string().erase(0, rootlength));
}

auto AudioPlay(fs::path *audiopath, CPhysical *audioentity) {
	auto audiostream = audioengine->play3D(audiopath->string().c_str(), irrklang::vec3df(0.0f, 0.0f, 0.0f), false, true);
	if (!audiostream) {
		modMessage("Could not play " + outputPath(audiopath));

		return;
	}

	audiostream->setMinDistance(150.0f);
	audioState(audiostream, audioentity);

	audiostream->setIsPaused(false);

	audiosplaying.push_back(make_pair(audiostream, audioentity));
}

#define MODELUNDEFINED eModelID(-1)

class AudioStream {
public:
	AudioStream() {}
	fs::path audiosfolder;
	AudioStream(fs::path weaponfolder) {
		audiosfolder = weaponfolder;
	}
	auto audioPath(string *filename, fs::path *filepath) {
		auto filelocation = audiosfolder / fs::path(*filename).replace_extension(".wav");
		if (fs::exists(filelocation)) *filepath = filelocation;
	}
	auto audioPlay(string *filename, CPhysical *audioentity) {
		auto filepath = fs::path();
		audioPath(filename, &filepath);

		if (
				fs::exists(filepath)
			&&	!filepath.empty()
			) {
			AudioPlay(&filepath, audioentity);

			return true;
		}

		return false;
	}
};
#define AUDIOPLAY(MODELID, FILESTEM, RETURNVALUE) if (findWeapon(&weaponType, eModelID(MODELID), FILESTEM, entity)) RETURNVALUE;

#define AUDIOSHOOT(MODELID, RETURNVALUE) AUDIOPLAY(MODELID, "shoot", RETURNVALUE)
#define AUDIORELOAD(MODELID, RETURNVALUE) AUDIOPLAY(MODELID, "reload", RETURNVALUE)
#define AUDIOHIT(MODELID, RETURNVALUE) AUDIOPLAY(MODELID, "hit", RETURNVALUE)
#define AUDIOSWING(MODELID, RETURNVALUE) AUDIOPLAY(MODELID, "swing", RETURNVALUE)

#define AUDIOCALL(AUDIOMACRO, RETURNVALUE) if (entity->m_nType == eEntityType::ENTITY_TYPE_PED) { AUDIOMACRO(MODELUNDEFINED, RETURNVALUE) } AUDIOMACRO(entity->m_nModelIndex, RETURNVALUE)

auto initializationstatus = int(-2);

auto logfile = fstream();
#define LOGLINE logfile << "\t" << 
#define LOGSEPARATOR << " : " <<
#define LOGNEWLINE << '\n'

auto nameType(string *weaponname, eWeaponType *weapontype) {
	char weaponchar[255]; sprintf(weaponchar, "%s", caseUpper(*weaponname).c_str());
	*weapontype = CWeaponInfo::FindWeaponType(weaponchar);

	if (*weapontype > eWeaponType::WEAPON_UNARMED) return true;

	return false;
}

auto registeredweapons = map<pair<eWeaponType, eModelID>, AudioStream>();
auto registerWeapon(fs::path *filepath) {
	auto modelid = MODELUNDEFINED;
	auto *weaponname = &string(); *weaponname = filepath->stem().string();

	auto fileseparator = weaponname->find_first_of(' ');
	if (fileseparator != string::npos) {
		char modelname[255]; sprintf(modelname, "%s", weaponname->substr(fileseparator + 1).c_str());
		auto modelfound = eModelID();
		CModelInfo::GetModelInfo(modelname, (int *)&modelfound);
		if (modelfound > MODELUNDEFINED) modelid = modelfound;
		*weaponname = weaponname->substr(0, fileseparator);
	}

	auto weapontype = eWeaponType();
	if (nameType(weaponname, &weapontype)) {
		registeredweapons[make_pair(weapontype, modelid)] = AudioStream(filepath->parent_path());

		LOGLINE weapontype LOGSEPARATOR modelid LOGSEPARATOR outputPath(filepath).c_str() LOGNEWLINE;
	}
}

auto findWeapon(eWeaponType *weapontype, eModelID modelid, string filename, CPhysical *audioentity) {
	auto audiofind = registeredweapons.find(make_pair(*weapontype, modelid));
	if (audiofind != registeredweapons.end()) return audiofind->second.audioPlay(&filename, audioentity);
	return false;
}

typedef void(__thiscall *originalCAEWeaponAudioEntity__WeaponFire)(
	eWeaponType weaponType, CPhysical *entity, int audioEventId
	);
typedef void(__thiscall *originalCAEWeaponAudioEntity__WeaponReload)(
	eWeaponType weaponType, CPhysical *entity, int audioEventId
	);
typedef void(__thiscall *originalCAEPedAudioEntity__HandlePedHit)(
	int a2, CPhysical *a3, unsigned __int8 a4, float a5, unsigned int a6
	);
typedef char(__thiscall *originalCAEPedAudioEntity__HandlePedSwing)(
	int a2, int a3, int a4
	);

auto subhookCAEWeaponAudioEntity__WeaponFire = subhook_t();
auto subhookCAEWeaponAudioEntity__WeaponReload = subhook_t();
auto subhookCAEPedAudioEntity__HandlePedHit = subhook_t();
auto subhookCAEPedAudioEntity__HandlePedSwing = subhook_t();

auto __fastcall HookedCAEWeaponAudioEntity__WeaponFire(CAEWeaponAudioEntity *thispointer, void *unusedpointer,
	eWeaponType weaponType, CPhysical *entity, int audioEventId
) {
	if (
			thispointer
		&&	entity
		) {
		AUDIOCALL(AUDIOSHOOT, return)
	}

	subhook_remove(subhookCAEWeaponAudioEntity__WeaponFire);
	thispointer->WeaponFire(weaponType, entity, audioEventId);
	subhook_install(subhookCAEWeaponAudioEntity__WeaponFire);
}
auto __fastcall HookedCAEWeaponAudioEntity__WeaponReload(CAEWeaponAudioEntity *thispointer, void *unusedpointer,
	eWeaponType weaponType, CPhysical *entity, int audioEventId
) {
	if (
		thispointer
		&&	entity
		) {
		AUDIOCALL(AUDIORELOAD, return)
	}

	subhook_remove(subhookCAEWeaponAudioEntity__WeaponReload);
	thispointer->WeaponReload(weaponType, entity, audioEventId);
	subhook_install(subhookCAEWeaponAudioEntity__WeaponReload);
}
auto __fastcall HookedCAEPedAudioEntity__HandlePedHit(CAEPedAudioEntity *thispointer, void *unusedpointer,
	int a2, CPhysical *a3, unsigned __int8 a4, float a5, unsigned int a6
) {
	if (
			thispointer
		&&	a3
		) {
		auto entity = a3;
		auto weaponType = thispointer->m_pPed->m_aWeapons[thispointer->m_pPed->m_nActiveWeaponSlot].m_nType;
		AUDIOCALL(AUDIOHIT, return)
	}

	subhook_remove(subhookCAEPedAudioEntity__HandlePedHit);
	plugin::CallMethod<0x4E1CC0, CAEPedAudioEntity *, int, CPhysical *, unsigned __int8, float, unsigned int>(thispointer, a2, a3, a4, a5, a6);
	subhook_install(subhookCAEPedAudioEntity__HandlePedHit);
}
auto __fastcall HookedCAEPedAudioEntity__HandlePedSwing(CAEPedAudioEntity *thispointer, void *unusedpointer,
	int a2, int a3, int a4
) {
	if (
			thispointer
		//&&	
		) {
		auto ped = thispointer->m_pPed;
		auto entity = (CPhysical *)ped;
		auto weaponType = (&ped->m_aWeapons[ped->m_nActiveWeaponSlot])->m_nType;
		AUDIOCALL(AUDIOSWING, return char(-1))
	}

	subhook_remove(subhookCAEPedAudioEntity__HandlePedSwing);
	auto returnvalue = plugin::CallMethodAndReturn<char, 0x4E1A40, CAEPedAudioEntity *, int, int, int>(thispointer, a2, a3, a4);
	subhook_install(subhookCAEPedAudioEntity__HandlePedSwing);

	return returnvalue;
}
//<

class EarShot {
public:
    EarShot() {
        // Initialise your plugin here
        
//>
		if (fs::exists(folderdata)) foldermod = folderdata;
		else fs::create_directories(foldermod);

		logfile.open(foldermod / fs::path(modname).replace_extension(".log"), fstream::out);

		Events::initRwEvent += [] {
			audioengine = irrklang::createIrrKlangDevice();

			audioengine->setRolloffFactor(0.0f);
		};

		Events::processScriptsEvent += [] {
			if (initializationstatus != -2) return;

			initializationstatus = 0;

			Events::gameProcessEvent += [] {
				if (initializationstatus == 0) {
					if (!audioengine) {
						modMessage("Could not start up irrKlang audio engine.");
						initializationstatus = -1;
					}
				}

				if (initializationstatus != -1) {
					if (initializationstatus == 0) {
						logfile << "File(s) (Overwritten by Mod Loader):" LOGNEWLINE;
						for (auto directoryentry : fs::recursive_directory_iterator(foldermod)) {
							auto entrypath = directoryentry.path();
							if (!fs::is_directory(entrypath)) {
								auto fileextension = caseLower(entrypath.extension().string());
								if (fileextension == modextension) registerWeapon(&entrypath);
							}
						}

						logfile LOGNEWLINE;
						auto autoid3000mlmodule = GetModuleHandle("AutoID3000ML.dll");
						if (autoid3000mlmodule) {
							auto getfiles = GetProcAddress(autoid3000mlmodule, "getFiles");
							if (getfiles) {
								logfile << "File(s) (Mod Loader):" LOGNEWLINE;
								typedef const char **(_cdecl *autoid3000mlfiles)(const char *, int *);
								auto AutoID3000MLFiles = (autoid3000mlfiles)getfiles;

								auto mlamount = int(); auto mlfiles = AutoID3000MLFiles(modextension.c_str(), &mlamount); --mlamount;
								for (auto mlindex = int(0); mlindex < mlamount; ++mlindex) registerWeapon(&(folderroot / fs::path(mlfiles[mlindex])));
							}
						}
						else logfile << "Mod Loader support requires AutoID3000.";

						auto registeredtotal = registeredweapons.size();

						if (registeredtotal == 0) {
							initializationstatus = -1;
							logfile LOGNEWLINE << "No file(s) found. Stopping work for this session." LOGNEWLINE;
							logfile.close();
							return;
						}

						logfile LOGNEWLINE << "Total: " << registeredtotal;

						subhookCAEWeaponAudioEntity__WeaponFire = subhook_new((void *)(originalCAEWeaponAudioEntity__WeaponFire)0x504F80, HookedCAEWeaponAudioEntity__WeaponFire, subhook_flags_t(0)); //CAEWeaponAudioEntity::WeaponFire
						subhookCAEWeaponAudioEntity__WeaponReload = subhook_new((void *)(originalCAEWeaponAudioEntity__WeaponReload)0x503690, HookedCAEWeaponAudioEntity__WeaponReload, subhook_flags_t(0)); //CAEWeaponAudioEntity::WeaponReload
						subhookCAEPedAudioEntity__HandlePedHit = subhook_new((void *)(originalCAEPedAudioEntity__HandlePedHit)0x4E1CC0, HookedCAEPedAudioEntity__HandlePedHit, subhook_flags_t(0)); //CAEPedAudioEntity::HandlePedHit
						subhookCAEPedAudioEntity__HandlePedSwing = subhook_new((void *)(originalCAEPedAudioEntity__HandlePedSwing)0x4E1A40, HookedCAEPedAudioEntity__HandlePedSwing, subhook_flags_t(0)); //CAEPedAudioEntity::HandlePedSwing
						
						subhook_install(subhookCAEWeaponAudioEntity__WeaponFire);
						subhook_install(subhookCAEWeaponAudioEntity__WeaponReload);
						subhook_install(subhookCAEPedAudioEntity__HandlePedHit);
						subhook_install(subhookCAEPedAudioEntity__HandlePedSwing);

						Events::onPauseAllSounds += [] { audioengine->setAllSoundsPaused(); };
						Events::onResumeAllSounds += [] { if (FrontEndMenuManager.m_bMenuActive) return; audioengine->setAllSoundsPaused(false); };
						Events::gameProcessEvent += [] { audioengine->setAllSoundsPaused(FrontEndMenuManager.m_bMenuActive); };

						logfile.close();
					}

					cameraposition = &TheCamera.GetPosition();

					audioengine->setListenerPosition(
							irrklang::vec3df(cameraposition->x, cameraposition->y, cameraposition->z)
						,	irrklang::vec3df(0.0f, 0.0f, 0.0f)
						,	irrklang::vec3df(0.0f, 0.0f, 0.0f)
						,	irrklang::vec3df(0.0f, 0.0f, 1.0f)
					);

					audiosplaying.erase(remove_if(audiosplaying.begin(), audiosplaying.end(), [](pair<irrklang::ISound *, CPhysical *> audioplaying) {
						auto audiostream = audioplaying.first;
						auto audioentity = audioplaying.second;
						if (audiostream->isFinished()) {

							return true;
						}

						audioState(audiostream, audioentity);

						return false;
					}), audiosplaying.end());
				}

				if (initializationstatus == 0) initializationstatus = 1;
			};
		};
//<
    }
//>
//<
} earShot;

//>
//<