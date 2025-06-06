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

#ifndef ENGINES_UTIL_H
#define ENGINES_UTIL_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/list.h"
#include "graphics/pixelformat.h"
#include "graphics/mode.h"

/**
 * @defgroup engines_util Util
 * @ingroup engines
 *
 * @brief Various utility functions related to engines.
 *
 * @{
 */

/**
 * Set up the graphics mode of the backend.
 *
 * @param is3D: set to true if the game to be launched uses 3D graphics
 */
void initCommonGFX(bool is3D);

/**
 * Send a list of graphics modes to the backend so it can make a decision
 * about the best way to set up the display hardware.
 *
 * Engines that switch between different virtual screen sizes during the game
 * should call this function prior to any call to initGraphics. Engines that use
 * only a single screen size do not need to call this function.
 */
void initGraphicsModes(const Graphics::ModeList &modes);

/**
 * Set up the screen size and graphics mode of the backend.
 *
 * Shows various warnings on certain backend graphics
 * transaction failures (aspect switch, fullscreen switch, etc.).
 *
 * An error dialog will be generated when the backend is not able to switch
 * to the specified mode.
 *
 * Defaults to CLUT8 (256 color palette) if only width and height provided.
 * If graphics format is explicitly set to nullptr, uses the preferred format of
 * the backend.
 * Finds the best compatible format if a list of graphics formats is provided.
 */
void initGraphics(int width, int height);
/**
 * @overload
 */
void initGraphics(int width, int height, const Graphics::PixelFormat *format);
/**
 * @overload
 */
void initGraphics(int width, int height, const Common::List<Graphics::PixelFormat> &formatList);
/**
 * @overload
 */
void initGraphics3d(int width, int height);

/**
 * Inits any of the modes in "modes". "modes" is in the order of preference.
 * Return value is index in modes of resulting mode.
 */
int initGraphicsAny(const Graphics::ModeWithFormatList &modes, int start = 0);
/** @} */
#endif
