/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/file.h"
#include "common/debug-channels.h"
#include "common/textconsole.h"
#include "common/system.h"

#include "engines/util.h"

#include "graphics/cursorman.h"

#include "cruise/cruise.h"
#include "cruise/font.h"
#include "cruise/gfxModule.h"
#include "cruise/staticres.h"

namespace Cruise {

//SoundDriver *g_soundDriver;
//SfxPlayer *g_sfxPlayer;

CruiseEngine *_vm;

CruiseEngine::CruiseEngine(OSystem * syst, const CRUISEGameDescription *gameDesc)
	: Engine(syst), _gameDescription(gameDesc), _rnd("cruise") {

	_vm = this;
	setDebugger(new Debugger());
	_sound = new PCSound(_mixer, this);

	PCFadeFlag = false;
	_preLoad = false;
	_savedCursor = CURSOR_NOMOUSE;
	_lastTick = 0;
	_gameSpeed = GAME_FRAME_DELAY_1;
	_speedFlag = false;
	_polyStructs = nullptr;
	_polyStruct = nullptr;

	_mouseButtonDown = false;
	_menuJustOpened = false;

	// Setup mixer
	syncSoundSettings();
}

extern void listMemory();

CruiseEngine::~CruiseEngine() {
	delete _sound;

	freeSystem();

	if (gDebugLevel > 0)
		MemoryList();
}

bool CruiseEngine::hasFeature(EngineFeature f) const {
	return
		(f == kSupportsReturnToLauncher) ||
		(f == kSupportsLoadingDuringRuntime) ||
		(f == kSupportsSavingDuringRuntime);
}

Common::Error CruiseEngine::run() {
	// Initialize backend
	initGraphics(320, 200);

	if (!loadLanguageStrings()) {
		error("Could not setup language data for your version");
		return Common::kUnknownError;	// for compilers that don't support NORETURN
	}

	Common::TextToSpeechManager *ttsMan = g_system->getTextToSpeechManager();
	if (ttsMan != nullptr) {
		ttsMan->enable(ConfMan.getBool("tts_enabled"));
		ttsMan->setLanguage(ConfMan.get("language"));

		if (getLanguage() == Common::FR_FRA || getLanguage() == Common::IT_ITA) {
			_ttsTextEncoding = Common::CodePage::kWindows1252;
		} else if (getLanguage() == Common::RU_RUS) {
			_ttsTextEncoding = Common::CodePage::kDos866;
		} else {
			_ttsTextEncoding = Common::CodePage::kDos850;
		}
	}

	initialize();

	Cruise::changeCursor(Cruise::CURSOR_NORMAL);
	CursorMan.showMouse(true);

	mainLoop();

	deinitialize();

	return Common::kNoError;
}

void CruiseEngine::initialize() {
	// video init stuff
	initSystem();
	gfxModuleData_Init();

	// another bit of video init
	readVolCnf();
}

void CruiseEngine::deinitialize() {
	_vm->_polyStructNorm.clear();
	_vm->_polyStructExp.clear();

	// Clear any backgrounds
	for (int i = 0; i < 8; ++i) {
		if (backgroundScreens[i]) {
			MemFree(backgroundScreens[i]);
			backgroundScreens[i] = nullptr;
		}
	}
}

bool CruiseEngine::loadLanguageStrings() {
	Common::File f;

	// Give preference to a language file
	if (f.open("DELPHINE.LNG")) {
		char *data = (char *)MemAlloc(f.size());
		f.read(data, f.size());
		char *ptr = data;

		for (int i = 0; i < MAX_LANGUAGE_STRINGS; ++i) {
			// Get the start of the next string
			while (*ptr != '"') ++ptr;
			const char *v = ++ptr;

			// Find the end of the string, and replace the end '"' with a NULL
			while (*ptr != '"') ++ptr;
			*ptr++ = '\0';

			// Add the string to the list
			_langStrings.push_back(v);
		}

		f.close();
		MemFree(data);

	} else {
		// Try and use one of the pre-defined language lists
		const char **p = nullptr;
		switch (getLanguage()) {
		case Common::EN_ANY:
			p = englishLanguageStrings;
			break;
		case Common::FR_FRA:
			p = frenchLanguageStrings;
			break;
		case Common::DE_DEU:
			p = germanLanguageStrings;
			break;
		case Common::IT_ITA:
			p = italianLanguageStrings;
			break;
		case Common::ES_ESP:
			p = spanishLanguageStrings;
			break;
		case Common::RU_RUS:
			p = russianLanguageStrings;
			break;
		default:
			return false;
		}

		// Load in the located language set
		for (int i = 0; i < 13; ++i, ++p)
			_langStrings.push_back(*p);
	}

	return true;
}

void CruiseEngine::pauseEngine(bool pause) {
	if (pause) {
		_gamePauseToken = Engine::pauseEngine();
		// Draw the 'Paused' message
		drawSolidBox(64, 100, 256, 117, 0);
		drawString(10, 100, langString(ID_PAUSED), gfxModuleData.pPage00, itemColor, 300);
		gfxModuleData_flipScreen();

		_savedCursor = currentCursor;
		changeCursor(CURSOR_NOMOUSE);
	} else {
		_gamePauseToken.clear();
		processAnimation();
		flipScreen();
		changeCursor(_savedCursor);

		_vm->stopTextToSpeech();
	}

	gfxModuleData_addDirtyRect(Common::Rect(64, 100, 256, 117));
}

void CruiseEngine::sayText(const Common::String &text, Common::TextToSpeechManager::Action action) {
	if (text.empty() && action == Common::TextToSpeechManager::QUEUE) {
		return;
	}

	Common::TextToSpeechManager *ttsMan = g_system->getTextToSpeechManager();
	// _previousSaid is used to prevent the TTS from looping when sayText is called inside a loop,
	// for example when the cursor stays on a menu item. Without it when the text ends it would speak
	// the same text again.
	// _previousSaid is cleared when appropriate to allow for repeat requests
	if (ttsMan != nullptr && ConfMan.getBool("tts_enabled") && _previousSaid != text) {
		ttsMan->say(text, action, _ttsTextEncoding);
		_previousSaid = text;
	}
}

void CruiseEngine::sayQueuedText(Common::TextToSpeechManager::Action action) {
	sayText(_toSpeak, action);
	_toSpeak.clear();
}

void CruiseEngine::stopTextToSpeech() {
	Common::TextToSpeechManager *ttsMan = g_system->getTextToSpeechManager();
	if (ttsMan != nullptr && ConfMan.getBool("tts_enabled") && ttsMan->isSpeaking()) {
		ttsMan->stop();
		_previousSaid.clear();
	}
}

Common::Error CruiseEngine::loadGameState(int slot) {
	return loadSavegameData(slot);
}

bool CruiseEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return playerMenuEnabled != 0;
}

Common::Error CruiseEngine::saveGameState(int slot, const Common::String &desc, bool isAutosave) {
	return saveSavegameData(slot, desc);
}

bool CruiseEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	return (playerMenuEnabled != 0) && (userEnabled != 0);
}

const char *CruiseEngine::getSavegameFile(int saveGameIdx) {
	static char buffer[20];
	Common::sprintf_s(buffer, "cruise.s%02d", saveGameIdx);
	return buffer;
}

void CruiseEngine::syncSoundSettings() {
	Engine::syncSoundSettings();

	_sound->syncSounds();
}

} // End of namespace Cruise
