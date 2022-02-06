#include "plugin.h"
//>
#include <filesystem>
#include "CModelInfo.h"
#include "CCamera.h"
#include "extensions/ScriptCommands.h"
#include "CAEAudioHardware.h"
#include <map>
#include "CTimer.h"
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

auto audiosplaying = vector<pair<int *, CVector>>();

auto cameraposition = (CVector *)nullptr;
auto audioVolume(int *audiostream, CVector *audioposition) {
	Command<0x10ABC>(*audiostream, (1.0f / (CVector(cameraposition->x - audioposition->x, cameraposition->y - audioposition->y, cameraposition->z - audioposition->z).Magnitude())) * AEAudioHardware.m_fEffectMasterScalingFactor); //SET_AUDIO_STREAM_VOLUME
}

auto AudioPlay(fs::path *audiopath, CVector *audioposition) {
	auto audiohandle = int();
	if (
		Command<0x10AAC>(audiopath->string().c_str(), &audiohandle) //LOAD_AUDIO_STREAM
		&& audiohandle
		) {
		auto audiostream = new int(audiohandle);

		audioVolume(audiostream, audioposition);
		Command<0x10AAD>(*audiostream, 1); //SET_AUDIO_STREAM_STATE

		audiosplaying.push_back(make_pair(audiostream, *audioposition));
	}
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
	auto audioPlay(string *filename, CVector *audioposition) {
		auto filepath = fs::path();
		audioPath(filename, &filepath);

		if (CTimer::ms_fTimeScale < 1.0f) audioPath(&(*filename + "_slow"), &filepath);

		if (
				fs::exists(filepath)
			&&	!filepath.empty()
			) {
			AudioPlay(&filepath, audioposition);

			return true;
		}

		return false;
	}
};
#define AUDIOPLAY(MODELID, FILESTEM) if (findWeapon(&weaponType, eModelID(MODELID), FILESTEM, &entity->GetPosition())) return;
#define AUDIOSHOOT(MODELID) AUDIOPLAY(MODELID, "shoot")
#define AUDIOCALL(AUDIOMACRO) if (entity->m_nType == eEntityType::ENTITY_TYPE_PED) { AUDIOMACRO(MODELUNDEFINED) } AUDIOMACRO(entity->m_nModelIndex)

auto foldermod = fs::path(PLUGIN_PATH("")) / fs::path(modname);
auto folderroot = fs::path(GAME_PATH(""));

auto initializationstatus = int(0);

auto logfile = fstream();
#define LOGLINE logfile << "\t" << 
#define LOGSEPARATOR << " : " <<
#define LOGNEWLINE << '\n'

auto rootlength = folderroot.string().length();
auto outputPath(fs::path *filepath) {
	return (filepath->string().erase(0, rootlength)).c_str();
}

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

		LOGLINE weapontype LOGSEPARATOR modelid LOGSEPARATOR outputPath(filepath) LOGNEWLINE;
	}
}

typedef int(_cdecl *flaparent)(int);
flaparent FLAParent;
auto FLAType(eWeaponType *weapontype) {
	if (FLAParent) {
		auto weaponparent = FLAParent(*weapontype);
		if (weaponparent != -1) *weapontype = eWeaponType(weaponparent);
	}
}

auto findWeapon(eWeaponType *weapontype, eModelID modelid, string filename, CVector *audioposition) {
	auto audiofind = registeredweapons.find(make_pair(*weapontype, modelid));
	if (audiofind != registeredweapons.end()) return audiofind->second.audioPlay(&filename, audioposition);
	return false;
}

auto OriginalCAEWeaponAudioEntity__WeaponFire(CAEWeaponAudioEntity *thispointer, eWeaponType weaponType, CPhysical *entity, int audioEventId) {
	if (entity) {
		switch (weaponType) {
			case eWeaponType::WEAPON_PISTOL:
				thispointer->PlayGunSounds(entity, 52, 53, 6, 7, 8, audioEventId, 0.0, 1.41421, 1.0);
			break;

			case eWeaponType::WEAPON_PISTOL_SILENCED:
				thispointer->PlayGunSounds(entity, 76, 77, 24, 24, -1, audioEventId, -7.0, 1.0, 1.0);
			break;

			case eWeaponType::WEAPON_DESERT_EAGLE:
				thispointer->PlayGunSounds(entity, 52, 53, 6, 7, 8, audioEventId, 0.0, 0.94387001, 1.0);
			break;

			case eWeaponType::WEAPON_SHOTGUN:
			case eWeaponType::WEAPON_SPAS12:
				thispointer->PlayGunSounds(entity, 73, 74, 21, 22, 23, audioEventId, 0.0, 1.0, 1.0);
			break;

			case eWeaponType::WEAPON_SAWNOFF:
				thispointer->PlayGunSounds(entity, 73, 74, 21, 22, 23, audioEventId, 0.0, 0.79369998, 0.93000001);
			break;

			case eWeaponType::WEAPON_MICRO_UZI:
				thispointer->PlayGunSounds(entity, 29, 30, 0, 1, 2, audioEventId, 0.0, 1.0, 1.0);
			break;

			case eWeaponType::WEAPON_MP5:
				thispointer->PlayGunSounds(entity, 29, 30, 17, 18, 2, audioEventId, 0.0, 1.0, 1.0);
			break;

			case eWeaponType::WEAPON_AK47:
			case eWeaponType::WEAPON_M4:
				thispointer->PlayGunSounds(entity, 33, 53, 3, 4, 5, audioEventId, 0.0, 1.0, 1.0);
			break;

			case eWeaponType::WEAPON_TEC9:
				thispointer->PlayGunSounds(entity, 29, 30, 0, 1, 2, audioEventId, 0.0, 1.25992, 1.0);
			break;

			case eWeaponType::WEAPON_COUNTRYRIFLE:
				thispointer->PlayGunSounds(entity, 52, 53, 26, 27, 23, audioEventId, 0.0, 0.88999999, 1.0);
			break;

			case eWeaponType::WEAPON_SNIPERRIFLE:
				thispointer->PlayGunSounds(entity, 52, 53, 26, 27, 23, audioEventId, 0.0, 1.0, 1.0);
			break;

			case eWeaponType::WEAPON_FTHROWER:
				if (!thispointer->m_dwFlameThrowerLastPlayedTime) thispointer->PlayFlameThrowerSounds(entity, 83, 26, audioEventId, -14.0, 1.0);
				thispointer->m_dwFlameThrowerLastPlayedTime = CTimer::m_snTimeInMilliseconds;
			break;

			case eWeaponType::WEAPON_MINIGUN:
				thispointer->PlayMiniGunFireSounds(entity, audioEventId);
			break;

			case eWeaponType::WEAPON_DETONATOR:
				thispointer->PlayGunSounds(entity, 49, -1, -1, -1, -1, audioEventId, -14.0, 1.0, 1.0);
			break;

			case eWeaponType::WEAPON_SPRAYCAN:
				if (!thispointer->m_dwSpraycanLastPlayedTime) thispointer->PlayWeaponLoopSound(entity, 28, audioEventId, -20.0, 1.0, 3);
				thispointer->m_dwSpraycanLastPlayedTime = CTimer::m_snTimeInMilliseconds;
			break;

			case eWeaponType::WEAPON_EXTINGUISHER:
				if (!thispointer->m_dwExtinguisherLastPlayedTime) thispointer->PlayWeaponLoopSound(entity, 9, audioEventId, -20.0, 0.79369998, 4);
				thispointer->m_dwExtinguisherLastPlayedTime = CTimer::m_snTimeInMilliseconds;
			break;

			case eWeaponType::WEAPON_CAMERA:
				thispointer->PlayCameraSound(entity, audioEventId, -14.0);
			break;

			case eWeaponType::WEAPON_NIGHTVISION:
			case eWeaponType::WEAPON_INFRARED:
				thispointer->PlayGoggleSound(64, audioEventId);
			break;
		}
	}
}

auto __fastcall HookedCAEWeaponAudioEntity__WeaponFire(CAEWeaponAudioEntity *thispointer, void *unusedpointer, eWeaponType weaponType, CPhysical *entity, int audioEventId) {
	if (
			thispointer
		&&	entity
		) {
		AUDIOCALL(AUDIOSHOOT)

		FLAType(&weaponType);
		OriginalCAEWeaponAudioEntity__WeaponFire(thispointer, weaponType, entity, audioEventId);
	}
}
//<

class EarShot {
public:
    EarShot() {
        // Initialise your plugin here
        
//>
		fs::create_directories(foldermod);
		logfile.open(foldermod / fs::path(modname).replace_extension(".log"), fstream::out);

		Events::processScriptsEvent += [] {
			if (initializationstatus == 0) {
				auto cleoerror = bool(true);
				auto cleomodule = GetModuleHandle("CLEO.asi");
				if (cleomodule) {
					auto getversion = GetProcAddress(cleomodule, "_CLEO_GetVersion@0");
					if (getversion) {
						typedef int(__cdecl *cleoversion)();
						auto CLEOVersion = (cleoversion)getversion;
						auto versionnumber = CLEOVersion();
						if (versionnumber >= 0x04000000) {
							cleoerror = false;
						}
					}
				}
				if (cleoerror) {
					modMessage("CLEO 4 or later required.");
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

					auto flamodule = GetModuleHandle("$fastman92limitAdjuster.asi");
					if (flamodule) {
						auto getparent = GetProcAddress(flamodule, "GetWeaponHighestParentType");
						if (getparent) {
							FLAParent = (flaparent)getparent;
						}
					}

					patch::ReplaceFunction(0x504F80, HookedCAEWeaponAudioEntity__WeaponFire); //CAEWeaponAudioEntity::WeaponFire

					logfile.close();
				}

				cameraposition = &TheCamera.GetPosition();

				auto audiostate = int();
				audiosplaying.erase(remove_if(audiosplaying.begin(), audiosplaying.end(), [
						&audiostate
				](pair<int *, CVector> audioplaying) {
					Command<0x10AB9>(*audioplaying.first, &audiostate); //GET_AUDIO_STREAM_STATE
					if (audiostate == -1) {
						Command<0x10AAE>(*audioplaying.first); //REMOVE_AUDIO_STREAM
						delete(audioplaying.first);

						return true;
					}

					audioVolume(audioplaying.first, &audioplaying.second);

					return false;
				}), audiosplaying.end());
			}

			if (initializationstatus == 0) initializationstatus = 1;
		};
//<
    }
//>
//<
} earShot;

//>
//<