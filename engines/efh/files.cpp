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

#include "common/config-manager.h"
#include "efh/efh.h"

namespace Efh {

int32 EfhEngine::readFileToBuffer(const Common::Path &filename, uint8 *destBuffer) {
	debugC(1, kDebugUtils, "readFileToBuffer %s", filename.toString().c_str());
	Common::File f;
	if (!f.open(filename))
		error("Unable to find file %s", filename.toString().c_str());

	int size = f.size();

	return f.read(destBuffer, size);
}

void EfhEngine::readAnimInfo() {
	debugC(6, kDebugEngine, "readAnimInfo");

	Common::Path fileName("animinfo");
	Common::File f;
	if (!f.open(fileName))
		error("Unable to find file %s", fileName.toString().c_str());

	for (int i = 0; i < 100; ++i) {
		for (int id = 0; id < 15; ++id) {
			Common::String txtBuffer = "->";
			for (int frameId = 0; frameId < 4; ++frameId) {
				_animInfo[i]._frameList[id]._subFileId[frameId] = f.readByte();
				txtBuffer += Common::String::format(" %d", _animInfo[i]._frameList[id]._subFileId[frameId]);
			}

			debugC(6, kDebugEngine, "%s", txtBuffer.c_str());
		}

		Common::String debugStr = "";
		for (int id = 0; id < 10; ++id) {
			_animInfo[i]._posY[id] = f.readByte();
			debugStr += Common::String::format("%d ", _animInfo[i]._posY[id]);
		}
		debugC(6, kDebugEngine, "%s", debugStr.c_str());

		debugStr = "";
		for (int id = 0; id < 10; ++id) {
			_animInfo[i]._posX[id] = f.readUint16LE();
			debugStr += Common::String::format("%d ", _animInfo[i]._posX[id]);
		}
		debugC(6, kDebugEngine, "%s", debugStr.c_str());
		debugC(6, kDebugEngine, "---------");
	}
}

void EfhEngine::findMapFile(int16 mapId) {
	debugC(7, kDebugEngine, "findMapFile %d", mapId);

	if (!_introDoneFl)
		return;

	Common::Path fileName(Common::String::format("map.%d", mapId));
	Common::File f;
	// The original was checking for the file and eventually asking to change floppies
	if (!f.open(fileName))
		error("File not found: %s", fileName.toString().c_str());

	f.close();
}

void EfhEngine::rImageFile(const Common::Path &filename, uint8 *targetBuffer, uint8 **subFilesArray, uint8 *packedBuffer) {
	debugC(1, kDebugUtils, "rImageFile %s", filename.toString().c_str());
	readFileToBuffer(filename, packedBuffer);

	uint32 size = uncompressBuffer(packedBuffer, targetBuffer);
	if (ConfMan.getBool("dump_scripts")) {
		// dump a decompressed image file
		Common::DumpFile dump;
		dump.open(filename.append(".dump"));
		dump.write(targetBuffer, size);
		dump.flush();
		dump.close();
		// End of dump
	}

	// TODO: Refactoring: once uncompressed, the container contains for each image its width, its height, and raw data (4 Bpp)
	// => Write a class to handle that more properly
	uint8 *ptr = targetBuffer;
	uint16 counter = 0;
	while (READ_LE_INT16(ptr) != 0 && !shouldQuit()) {
		subFilesArray[counter] = ptr;
		++counter;
		int16 imageWidth = READ_LE_INT16(ptr);
		ptr += 2;
		int16 imageHeight = READ_LE_INT16(ptr);
		ptr += 2;
		ptr += (imageWidth * imageHeight);
	}
}

void EfhEngine::readImpFile(int16 id, bool techMapFl) {
	debugC(6, kDebugEngine, "readImpFile %d %s", id, techMapFl ? "True" : "False");

	Common::Path fileName(Common::String::format("imp.%d", id));

	if (techMapFl)
		readFileToBuffer(fileName, _imp1);
	else
		readFileToBuffer(fileName, _imp2);

	decryptImpFile(techMapFl);
}

void EfhEngine::readItems() {
	debugC(7, kDebugEngine, "readItems");

	Common::Path fileName("items");
	Common::File f;
	if (!f.open(fileName))
		error("Unable to find file %s", fileName.toString().c_str());

	for (int i = 0; i < 300; ++i) {
		for (uint idx = 0; idx < 15; ++idx)
			_items[i]._name[idx] = f.readByte();

		_items[i]._damage = f.readByte();
		_items[i]._defense = f.readByte();
		_items[i]._attacks = f.readByte();
		_items[i]._uses = f.readByte();
		_items[i]._agilityModifier = f.readByte();
		_items[i]._range = f.readByte();
		_items[i]._attackType = f.readByte();
		_items[i]._specialEffect = f.readByte();
		_items[i]._defenseType = f.readByte();
		_items[i]._exclusiveType = f.readByte();
		_items[i]._field19_mapPosX_or_maxDeltaPoints = f.readByte();
		_items[i]._mapPosY = f.readByte();

		debugC(7, kDebugEngine, "%s\t%x\t%x\t%x\t%x\t%x\t%x\t%x\t%x\t%x\t%x\t%x\t%x", _items[i]._name, _items[i]._damage, _items[i]._defense, _items[i]._attacks, _items[i]._uses, _items[i]._agilityModifier, _items[i]._range, _items[i]._attackType, _items[i]._specialEffect, _items[i]._defenseType, _items[i]._exclusiveType, _items[i]._field19_mapPosX_or_maxDeltaPoints, _items[i]._mapPosY);
	}
}

void EfhEngine::loadNewPortrait() {
	debugC(7, kDebugEngine, "loadNewPortrait");

	static int16 const unkConstRelatedToAnimImageSetId[19] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2};
	_unkRelatedToAnimImageSetId = unkConstRelatedToAnimImageSetId[_techId];

	if (_currentAnimImageSetId == 200 + _unkRelatedToAnimImageSetId)
		return;

	findMapFile(_techId);
	_currentAnimImageSetId = 200 + _unkRelatedToAnimImageSetId;
	int imageSetId = _unkRelatedToAnimImageSetId + 13;
	loadImageSet(imageSetId, _portraitBuf, _portraitSubFilesArray, _decompBuf);
}

void EfhEngine::loadAnimImageSet() {
	debugC(3, kDebugEngine, "loadAnimImageSet");

	if (_currentAnimImageSetId == _animImageSetId || _animImageSetId == 0xFF)
		return;

	findMapFile(_techId);

	_unkAnimRelatedIndex = 0;
	_currentAnimImageSetId = _animImageSetId;

	int16 animSetId = _animImageSetId + 17;
	loadImageSet(animSetId, _portraitBuf, _portraitSubFilesArray, _decompBuf);
}

void EfhEngine::loadHistory() {
	debugC(2, kDebugEngine, "loadHistory");

	Common::Path fileName("history");
	readFileToBuffer(fileName, _history);
}

void EfhEngine::loadTechMapImp(int16 fileId) {
	debugC(3, kDebugEngine, "loadTechMapImp %d", fileId);

	if (fileId == 0xFF)
		return;

	_techId = fileId;
	findMapFile(_techId);

	// The original was loading the specific tech.%d and map.%d files.
	// This is gone in our implementation as we pre-load all the files to save them inside the savegames

	loadImageSetToTileBank(0, _mapBitmapRefArr[_techId]._setId1);
	loadImageSetToTileBank(1, _mapBitmapRefArr[_techId]._setId2);

	initMapMonsters();
	readImpFile(_techId, true);
	displayAnimFrames(0xFE, false);
}

void EfhEngine::loadPlacesFile(uint16 fullPlaceId, bool forceReloadFl) {
	debugC(2, kDebugEngine, "loadPlacesFile %d %s", fullPlaceId, forceReloadFl ? "True" : "False");

	if (fullPlaceId == 0xFF)
		return;

	findMapFile(_techId);
	_fullPlaceId = fullPlaceId;
	uint16 minPlace = _lastMainPlaceId * 20;
	uint16 maxPlace = minPlace + 19;

	if (_fullPlaceId < minPlace || _fullPlaceId > maxPlace || forceReloadFl) {
		_lastMainPlaceId = _fullPlaceId / 20;
		Common::Path fileName(Common::String::format("places.%d", _lastMainPlaceId));
		readFileToBuffer(fileName, _decompBuf);
		uncompressBuffer(_decompBuf, _places);
	}
	copyCurrentPlaceToBuffer(_fullPlaceId % 20);
}

void EfhEngine::readTileFact() {
	debugC(7, kDebugEngine, "readTileFact");

	Common::Path fileName("tilefact");
	Common::File f;
	if (!f.open(fileName))
		error("Unable to find file %s", fileName.toString().c_str());

	for (int i = 0; i < 432; ++i) {
		_tileFact[i]._status = f.readByte();
		_tileFact[i]._tileId = f.readByte();
	}
}

void EfhEngine::loadNPCS() {
	debugC(7, kDebugEngine, "loadNPCS");

	Common::Path fileName("npcs");
	Common::File f;
	if (!f.open(fileName))
		error("Unable to find file %s", fileName.toString().c_str());

	for (int i = 0; i < 99; ++i) {
		for (int idx = 0; idx < 11; ++idx)
			_npcBuf[i]._name[idx] = f.readByte();
		_npcBuf[i].fieldB_textId = f.readByte();
		_npcBuf[i].field_C = f.readByte();
		_npcBuf[i].field_D = f.readByte();
		_npcBuf[i].fieldE_textId = f.readByte();
		_npcBuf[i].field_F = f.readByte();
		_npcBuf[i].field_10 = f.readByte();
		_npcBuf[i].field11_NpcId = f.readByte();
		_npcBuf[i].field12_textId = f.readUint16LE();
		_npcBuf[i].field14_textId = f.readUint16LE();
		_npcBuf[i]._xp = f.readUint32LE();
		for (int idx = 0; idx < 15; ++idx) {
			_npcBuf[i]._activeScore[idx] = f.readByte();
		}
		for (int idx = 0; idx < 11; ++idx) {
			_npcBuf[i]._passiveScore[idx] = f.readByte();
		}
		for (int idx = 0; idx < 11; ++idx) {
			_npcBuf[i]._infoScore[idx] = f.readByte();
		}
		_npcBuf[i].field_3F = f.readByte();
		_npcBuf[i].field_40 = f.readByte();
		for (int idx = 0; idx < 10; ++idx) {
			_npcBuf[i]._inventory[idx]._ref = f.readSint16LE();
			_npcBuf[i]._inventory[idx]._stat1 = f.readByte();
			_npcBuf[i]._inventory[idx]._curHitPoints = f.readByte();
		}
		_npcBuf[i]._possessivePronounSHL6 = f.readByte();
		_npcBuf[i]._speed = f.readByte();
		_npcBuf[i].field_6B = f.readByte();
		_npcBuf[i].field_6C = f.readByte();
		_npcBuf[i].field_6D = f.readByte();
		_npcBuf[i]._defaultDefenseItemId = f.readByte();
		_npcBuf[i].field_6F = f.readByte();
		_npcBuf[i].field_70 = f.readByte();
		_npcBuf[i].field_71 = f.readByte();
		_npcBuf[i].field_72 = f.readByte();
		_npcBuf[i].field_73 = f.readByte();
		_npcBuf[i]._hitPoints = f.readSint16LE();
		_npcBuf[i]._maxHP = f.readSint16LE();
		_npcBuf[i].field_78 = f.readByte();
		_npcBuf[i].field_79 = f.readUint16LE();
		_npcBuf[i].field_7B = f.readUint16LE();
		_npcBuf[i].field_7D = f.readByte();
		_npcBuf[i].field_7E = f.readByte();
		_npcBuf[i].field_7F = f.readByte();
		_npcBuf[i].field_80 = f.readByte();
		_npcBuf[i].field_81 = f.readByte();
		_npcBuf[i].field_82 = f.readByte();
		_npcBuf[i].field_83 = f.readByte();
		_npcBuf[i].field_84 = f.readByte();
		_npcBuf[i].field_85 = f.readByte();
	}
}

/**
 * Pre-Loads MAP and TECH files.
 * This is required in order to implement a clean savegame feature
 */
void EfhEngine::preLoadMaps() {
	Common::DumpFile dump;
	if (ConfMan.getBool("dump_scripts"))
		dump.open("efhMaps.dump");

	for (int idx = 0; idx < 19; ++idx) {
		Common::Path fileName(Common::String::format("tech.%d", idx));
		readFileToBuffer(fileName, _decompBuf);
		uncompressBuffer(_decompBuf, _techDataArr[idx]);

		fileName = Common::Path(Common::String::format("map.%d", idx));
		readFileToBuffer(fileName, _decompBuf);
		uncompressBuffer(_decompBuf, _mapArr[idx]);

		_mapBitmapRefArr[idx]._setId1 = _mapArr[idx][0];
		_mapBitmapRefArr[idx]._setId2 = _mapArr[idx][1];

		uint8 *mapSpecialTilePtr = &_mapArr[idx][2];

		for (int i = 0; i < 100; ++i) {
			_mapSpecialTiles[idx][i]._placeId = mapSpecialTilePtr[9 * i];
			_mapSpecialTiles[idx][i]._posX = mapSpecialTilePtr[9 * i + 1];
			_mapSpecialTiles[idx][i]._posY = mapSpecialTilePtr[9 * i + 2];
			_mapSpecialTiles[idx][i]._triggerType = mapSpecialTilePtr[9 * i + 3];
			_mapSpecialTiles[idx][i]._triggerValue = mapSpecialTilePtr[9 * i + 4];
			_mapSpecialTiles[idx][i]._field5_textId = READ_LE_UINT16(&mapSpecialTilePtr[9 * i + 5]);
			_mapSpecialTiles[idx][i]._field7_textId = READ_LE_UINT16(&mapSpecialTilePtr[9 * i + 7]);

			if (ConfMan.getBool("dump_scripts") && _mapSpecialTiles[idx][i]._placeId != 0xFF) {
				// dump a decoded version of the maps
				Common::String buffer = Common::String::format("[%d][%d] _ placeId: 0x%02X _pos: %d, %d _triggerType: 0x%02X (%d), triggerId: %d, _field5/7: %d %d\n"
					, idx, i, _mapSpecialTiles[idx][i]._placeId, _mapSpecialTiles[idx][i]._posX, _mapSpecialTiles[idx][i]._posX, _mapSpecialTiles[idx][i]._triggerType
					, _mapSpecialTiles[idx][i]._triggerType, _mapSpecialTiles[idx][i]._triggerValue, _mapSpecialTiles[idx][i]._field5_textId, _mapSpecialTiles[idx][i]._field7_textId);
				dump.write(buffer.c_str(), buffer.size());
			}
		}

		uint8 *mapMonstersPtr = &_mapArr[idx][902];
		for (int i = 0; i < 64; ++i) {
			_mapMonsters[idx][i]._possessivePronounSHL6 = mapMonstersPtr[29 * i];
			_mapMonsters[idx][i]._npcId = mapMonstersPtr[29 * i + 1];
			_mapMonsters[idx][i]._fullPlaceId = mapMonstersPtr[29 * i + 2];
			_mapMonsters[idx][i]._posX = mapMonstersPtr[29 * i + 3];
			_mapMonsters[idx][i]._posY = mapMonstersPtr[29 * i + 4];
			_mapMonsters[idx][i]._weaponItemId = mapMonstersPtr[29 * i + 5];
			_mapMonsters[idx][i]._maxDamageAbsorption = mapMonstersPtr[29 * i + 6];
			_mapMonsters[idx][i]._monsterRef = mapMonstersPtr[29 * i + 7];
			_mapMonsters[idx][i]._additionalInfo = mapMonstersPtr[29 * i + 8];
			_mapMonsters[idx][i]._talkTextId = mapMonstersPtr[29 * i + 9];
			_mapMonsters[idx][i]._groupSize = mapMonstersPtr[29 * i + 10];
			for (int j = 0; j < 9; ++j)
				_mapMonsters[idx][i]._hitPoints[j] = READ_LE_INT16(&mapMonstersPtr[29 * i + 11 + j * 2]);
		}

		uint8 *mapPtr = &_mapArr[idx][2758];
		for (int i = 0; i < 64; ++i) {
			for (int j = 0; j < 64; ++j)
				_mapGameMaps[idx][i][j] = *mapPtr++;
		}
	}

	if (ConfMan.getBool("dump_scripts")) {
		dump.flush();
		dump.close();
	}
}

} // End of namespace Efh
