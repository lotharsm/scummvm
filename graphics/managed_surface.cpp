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

#include "graphics/managed_surface.h"
#include "graphics/blit.h"
#include "graphics/palette.h"
#include "graphics/transform_tools.h"
#include "common/algorithm.h"
#include "common/textconsole.h"
#include "common/endian.h"

namespace Graphics {

const int SCALE_THRESHOLD = 0x100;

ManagedSurface::ManagedSurface() :
		w(_innerSurface.w), h(_innerSurface.h), pitch(_innerSurface.pitch), format(_innerSurface.format),
		_disposeAfterUse(DisposeAfterUse::NO), _owner(nullptr),
		_transparentColor(0),_transparentColorSet(false), _palette(nullptr) {
}

ManagedSurface::ManagedSurface(const ManagedSurface &surf) :
		w(_innerSurface.w), h(_innerSurface.h), pitch(_innerSurface.pitch), format(_innerSurface.format),
		_disposeAfterUse(DisposeAfterUse::NO), _owner(nullptr),
		_transparentColor(0), _transparentColorSet(false), _palette(nullptr) {
	*this = surf;
}

ManagedSurface::ManagedSurface(ManagedSurface &&surf) :
		w(_innerSurface.w), h(_innerSurface.h), pitch(_innerSurface.pitch), format(_innerSurface.format),
		_disposeAfterUse(surf._disposeAfterUse), _owner(surf._owner), _offsetFromOwner(surf._offsetFromOwner),
		_transparentColor(surf._transparentColor), _transparentColorSet(surf._transparentColorSet),
		_palette(surf._palette) {

	_innerSurface.setPixels(surf.getPixels());
	_innerSurface.w = surf.w;
	_innerSurface.h = surf.h;
	_innerSurface.pitch = surf.pitch;
	_innerSurface.format = surf.format;

	// Reset the old surface
	surf._innerSurface.init(0, 0, 0, NULL, PixelFormat());
	surf._disposeAfterUse = DisposeAfterUse::NO;
	surf._owner = nullptr;
	surf._offsetFromOwner = Common::Point();
	surf._transparentColor = 0;
	surf._transparentColorSet = false;
	surf._palette = nullptr;
}

ManagedSurface::ManagedSurface(int width, int height) :
		w(_innerSurface.w), h(_innerSurface.h), pitch(_innerSurface.pitch), format(_innerSurface.format),
		_disposeAfterUse(DisposeAfterUse::NO), _owner(nullptr),
		_transparentColor(0), _transparentColorSet(false), _palette(nullptr) {
	create(width, height);
}

ManagedSurface::ManagedSurface(int width, int height, const Graphics::PixelFormat &pixelFormat) :
		w(_innerSurface.w), h(_innerSurface.h), pitch(_innerSurface.pitch), format(_innerSurface.format),
		_disposeAfterUse(DisposeAfterUse::NO), _owner(nullptr),
		_transparentColor(0), _transparentColorSet(false), _palette(nullptr) {
	create(width, height, pixelFormat);
}

ManagedSurface::ManagedSurface(ManagedSurface &surf, const Common::Rect &bounds) :
		w(_innerSurface.w), h(_innerSurface.h), pitch(_innerSurface.pitch), format(_innerSurface.format),
		_disposeAfterUse(DisposeAfterUse::NO), _owner(nullptr),
		_transparentColor(0), _transparentColorSet(false), _palette(nullptr) {
	create(surf, bounds);
}

ManagedSurface::~ManagedSurface() {
	free();
}

ManagedSurface &ManagedSurface::operator=(const ManagedSurface &surf) {
	// Free any current surface
	free();

	if (surf._disposeAfterUse == DisposeAfterUse::YES) {
		// Create a new surface and copy the pixels from the source surface
		create(surf.w, surf.h, surf.format);
		Common::copy((const byte *)surf.getPixels(), (const byte *)surf.getPixels() +
			surf.w * surf.h * surf.format.bytesPerPixel, (byte *)this->getPixels());
	} else {
		// Source isn't managed, so simply copy its fields
		_owner = surf._owner;
		_offsetFromOwner = surf._offsetFromOwner;
		void *srcPixels = const_cast<void *>(surf._innerSurface.getPixels());
		_innerSurface.setPixels(srcPixels);
		_innerSurface.w = surf.w;
		_innerSurface.h = surf.h;
		_innerSurface.pitch = surf.pitch;
		this->format = surf.format;
	}

	// Copy miscellaneous properties
	_transparentColorSet = surf._transparentColorSet;
	_transparentColor = surf._transparentColor;
	_palette = surf._palette ? new Palette(*surf._palette) : nullptr;

	return *this;
}

ManagedSurface &ManagedSurface::operator=(ManagedSurface &&surf) {
	// Free any current surface
	free();

	_disposeAfterUse = surf._disposeAfterUse;
	_owner = surf._owner;
	_offsetFromOwner = surf._offsetFromOwner;

	_innerSurface.setPixels(surf.getPixels());
	_innerSurface.w = surf.w;
	_innerSurface.h = surf.h;
	_innerSurface.pitch = surf.pitch;
	_innerSurface.format = surf.format;

	// Copy miscellaneous properties
	_transparentColorSet = surf._transparentColorSet;
	_transparentColor = surf._transparentColor;
	_palette = surf._palette;

	// Reset the old surface
	surf._innerSurface.init(0, 0, 0, NULL, PixelFormat());
	surf._disposeAfterUse = DisposeAfterUse::NO;
	surf._owner = nullptr;
	surf._offsetFromOwner = Common::Point();
	surf._transparentColor = 0;
	surf._transparentColorSet = false;
	surf._palette = nullptr;

	return *this;
}

void ManagedSurface::setPixels(void *newPixels) {
	free();
	_innerSurface.setPixels(newPixels);
}

void ManagedSurface::create(int16 width, int16 height) {
	create(width, height, PixelFormat::createFormatCLUT8());
}

void ManagedSurface::create(int16 width, int16 height, const PixelFormat &pixelFormat) {
	free();
	_innerSurface.create(width, height, pixelFormat);

	// For pixel formats with an alpha channel, we need to do a clear
	// so that all the pixels will have full alpha (0xff)
	if (pixelFormat.aBits() != 0)
		clear(0);

	_disposeAfterUse = DisposeAfterUse::YES;
	markAllDirty();
}

void ManagedSurface::create(ManagedSurface &surf, const Common::Rect &bounds) {
	free();

	_offsetFromOwner = Common::Point(bounds.left, bounds.top);
	_innerSurface.setPixels(surf.getBasePtr(bounds.left, bounds.top));
	_innerSurface.pitch = surf.pitch;
	_innerSurface.format = surf.format;
	_innerSurface.w = bounds.width();
	_innerSurface.h = bounds.height();
	_owner = &surf;
	_disposeAfterUse = DisposeAfterUse::NO;

	// Copy miscellaneous properties
	_transparentColorSet = surf._transparentColorSet;
	_transparentColor = surf._transparentColor;
	_palette = surf._palette ? new Palette(*surf._palette) : nullptr;
}

void ManagedSurface::free() {
	if (_disposeAfterUse == DisposeAfterUse::YES) {
		_innerSurface.free();
	} else {
		_innerSurface.setPixels(nullptr);
	}

	_disposeAfterUse = DisposeAfterUse::NO;
	_owner = nullptr;
	_offsetFromOwner = Common::Point(0, 0);
	_transparentColorSet = false;
	if (_palette) {
		delete _palette;
		_palette = nullptr;
	}
}

void ManagedSurface::copyFrom(const ManagedSurface &surf) {
	// Surface::copyFrom frees pixel pointer so let's free up ManagedSurface to be coherent
	free();

	// Copy the surface
	_innerSurface.copyFrom(surf._innerSurface);
	markAllDirty();

	// Pixels data is now owned by us
	_disposeAfterUse = DisposeAfterUse::YES;

	// Copy miscellaneous properties
	_transparentColorSet = surf._transparentColorSet;
	_transparentColor = surf._transparentColor;
	_palette = surf._palette ? new Palette(*surf._palette) : nullptr;
}

void ManagedSurface::copyFrom(const Surface &surf) {
	// Surface::copyFrom frees pixel pointer so let's free up ManagedSurface to be coherent
	free();

	// Copy the surface
	_innerSurface.copyFrom(surf);
	markAllDirty();

	// Pixels data is now owned by us
	_disposeAfterUse = DisposeAfterUse::YES;

	// Set miscellaneous properties to sane values
	_transparentColorSet = false;
	_transparentColor = 0;
	if (_palette) {
		delete _palette;
		_palette = nullptr;
	}
}

void ManagedSurface::convertFrom(const ManagedSurface &surf, const PixelFormat &fmt) {
	// Surface::copyFrom frees pixel pointer so let's free up ManagedSurface to be coherent
	free();

	// Copy the surface
	_innerSurface.convertFrom(surf._innerSurface, fmt);
	markAllDirty();

	// Pixels data is now owned by us
	_disposeAfterUse = DisposeAfterUse::YES;

	// Copy miscellaneous properties
	_transparentColorSet = surf._transparentColorSet;
	_transparentColor = surf._transparentColor;
	_palette = (fmt.isCLUT8() && surf._palette) ? new Palette(*surf._palette) : nullptr;
}

void ManagedSurface::convertFrom(const Surface &surf, const PixelFormat &fmt) {
	// Surface::copyFrom frees pixel pointer so let's free up ManagedSurface to be coherent
	free();

	// Copy the surface
	_innerSurface.convertFrom(surf, fmt);
	markAllDirty();

	// Pixels data is now owned by us
	_disposeAfterUse = DisposeAfterUse::YES;

	// Set miscellaneous properties to sane values
	_transparentColorSet = false;
	_transparentColor = 0;
	if (_palette) {
		delete _palette;
		_palette = nullptr;
	}
}

Graphics::ManagedSurface *ManagedSurface::scale(int16 newWidth, int16 newHeight, bool filtering) const {
	Graphics::ManagedSurface *target = new Graphics::ManagedSurface();

	target->create(newWidth, newHeight, format);

	if (filtering) {
		scaleBlitBilinear((byte *)target->getPixels(), (const byte *)getPixels(), target->pitch, pitch, target->w, target->h, w, h, format);
	} else {
		scaleBlit((byte *)target->getPixels(), (const byte *)getPixels(), target->pitch, pitch, target->w, target->h, w, h, format);
	}

	// Copy miscellaneous properties
	if (hasTransparentColor())
		target->setTransparentColor(getTransparentColor());
	if (hasPalette())
		target->setPalette(_palette->data(), 0, _palette->size());

	return target;
}

Graphics::ManagedSurface *ManagedSurface::rotoscale(const TransformStruct &transform, bool filtering) const {

	Common::Point newHotspot;
	Common::Rect rect = TransformTools::newRect(Common::Rect((int16)w, (int16)h), transform, &newHotspot);

	Graphics::ManagedSurface *target = new Graphics::ManagedSurface();

	target->create((uint16)rect.right - rect.left, (uint16)rect.bottom - rect.top, this->format);

	if (filtering) {
		rotoscaleBlitBilinear((byte *)target->getPixels(), (const byte *)getPixels(), target->pitch, pitch, target->w, target->h, w, h, format, transform, newHotspot);
	} else {
		rotoscaleBlit((byte *)target->getPixels(), (const byte *)getPixels(), target->pitch, pitch, target->w, target->h, w, h, format, transform, newHotspot);
	}

	// Copy miscellaneous properties
	if (hasTransparentColor())
		target->setTransparentColor(getTransparentColor());
	if (hasPalette())
		target->setPalette(_palette->data(), 0, _palette->size());

	return target;
}

void ManagedSurface::simpleBlitFrom(const Surface &src, const Palette *srcPalette) {
	simpleBlitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Point(0, 0), srcPalette);
}

void ManagedSurface::simpleBlitFrom(const Surface &src, const Common::Point &destPos, const Palette *srcPalette) {
	simpleBlitFrom(src, Common::Rect(0, 0, src.w, src.h), destPos, srcPalette);
}

void ManagedSurface::simpleBlitFrom(const Surface &src, const Common::Rect &srcRect,
		const Common::Point &destPos, const Palette *srcPalette) {
	simpleBlitFromInner(src, srcRect, destPos, srcPalette, false, 0);
}

void ManagedSurface::simpleBlitFrom(const ManagedSurface &src) {
	simpleBlitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Point(0, 0));
}

void ManagedSurface::simpleBlitFrom(const ManagedSurface &src, const Common::Point &destPos) {
	simpleBlitFrom(src, Common::Rect(0, 0, src.w, src.h), destPos);
}

void ManagedSurface::simpleBlitFrom(const ManagedSurface &src, const Common::Rect &srcRect,
		const Common::Point &destPos) {
	simpleBlitFromInner(src._innerSurface, srcRect, destPos, src._palette,
		src._transparentColorSet, src._transparentColor);
}

void ManagedSurface::simpleBlitFromInner(const Surface &src, const Common::Rect &srcRect,
		const Common::Point &destPos, const Palette *srcPalette,
		bool transparentColorSet, uint transparentColor) {

	Common::Rect srcRectC = srcRect;
	Common::Rect dstRectC = srcRect;

	dstRectC.moveTo(destPos.x, destPos.y);
	clip(srcRectC, dstRectC);

	const byte *srcPtr = (const byte *)src.getBasePtr(srcRectC.left, srcRectC.top);
	byte *dstPtr = (byte *)getBasePtr(dstRectC.left, dstRectC.top);

	if (format == src.format) {
		if (transparentColorSet) {
			keyBlit(dstPtr, srcPtr, pitch, src.pitch, srcRectC.width(), srcRectC.height(),
				format.bytesPerPixel, transparentColor);
		} else {
			copyBlit(dstPtr, srcPtr, pitch, src.pitch, srcRectC.width(), srcRectC.height(),
				format.bytesPerPixel);
		}
	} else if (src.format.isCLUT8()) {
		assert(srcPalette);
		assert(!format.isCLUT8());

		uint32 map[256];
		convertPaletteToMap(map, srcPalette->data(), srcPalette->size(), format);

		if (transparentColorSet) {
			crossKeyBlitMap(dstPtr, srcPtr, pitch, src.pitch, srcRectC.width(), srcRectC.height(),
				format.bytesPerPixel, map, transparentColor);
		} else {
			crossBlitMap(dstPtr, srcPtr, pitch, src.pitch, srcRectC.width(), srcRectC.height(),
				format.bytesPerPixel, map);
		}
	} else {
		if (transparentColorSet) {
			crossKeyBlit(dstPtr, srcPtr, pitch, src.pitch, srcRectC.width(), srcRectC.height(),
				format, src.format, transparentColor);
		} else {
			crossBlit(dstPtr, srcPtr, pitch, src.pitch, srcRectC.width(), srcRectC.height(),
				format, src.format);
		}
	}

	addDirtyRect(dstRectC);
}

void ManagedSurface::maskBlitFrom(const Surface &src, const Surface &mask, const Palette *srcPalette) {
	maskBlitFrom(src, mask, Common::Rect(0, 0, src.w, src.h), Common::Point(0, 0), srcPalette);
}

void ManagedSurface::maskBlitFrom(const Surface &src, const Surface &mask, const Common::Point &destPos, const Palette *srcPalette) {
	maskBlitFrom(src, mask, Common::Rect(0, 0, src.w, src.h), destPos, srcPalette);
}

void ManagedSurface::maskBlitFrom(const Surface &src, const Surface &mask, const Common::Rect &srcRect,
		const Common::Point &destPos, const Palette *srcPalette) {
	maskBlitFromInner(src, mask, srcRect, destPos, srcPalette);
}

void ManagedSurface::maskBlitFrom(const ManagedSurface &src, const ManagedSurface &mask) {
	maskBlitFrom(src, mask, Common::Rect(0, 0, src.w, src.h), Common::Point(0, 0));
}

void ManagedSurface::maskBlitFrom(const ManagedSurface &src, const ManagedSurface &mask, const Common::Point &destPos) {
	maskBlitFrom(src, mask, Common::Rect(0, 0, src.w, src.h), destPos);
}

void ManagedSurface::maskBlitFrom(const ManagedSurface &src, const ManagedSurface &mask,
		const Common::Rect &srcRect, const Common::Point &destPos) {
	maskBlitFromInner(src._innerSurface, mask._innerSurface, srcRect, destPos, src._palette);
}

void ManagedSurface::maskBlitFromInner(const Surface &src, const Surface &mask,
		const Common::Rect &srcRect, const Common::Point &destPos,
		const Palette *srcPalette) {

	if (mask.w != src.w || mask.h != src.h)
		error("Surface::maskBlitFrom: mask dimensions do not match src");

	Common::Rect srcRectC = srcRect;
	Common::Rect dstRectC = srcRect;

	dstRectC.moveTo(destPos.x, destPos.y);
	clip(srcRectC, dstRectC);

	const byte *srcPtr = (const byte *)src.getBasePtr(srcRectC.left, srcRectC.top);
	const byte *maskPtr = (const byte *)mask.getBasePtr(srcRectC.left, srcRectC.top);
	byte *dstPtr = (byte *)getBasePtr(dstRectC.left, dstRectC.top);

	if (format == src.format) {
		maskBlit(dstPtr, srcPtr, maskPtr, pitch, src.pitch, mask.pitch, srcRectC.width(), srcRectC.height(),
			format.bytesPerPixel);
	} else if (src.format.isCLUT8()) {
		assert(srcPalette);
		assert(!format.isCLUT8());

		uint32 map[256];
		convertPaletteToMap(map, srcPalette->data(), srcPalette->size(), format);
		crossMaskBlitMap(dstPtr, srcPtr, maskPtr, pitch, src.pitch, mask.pitch, srcRectC.width(), srcRectC.height(),
			format.bytesPerPixel, map);
	} else {
		crossMaskBlit(dstPtr, srcPtr, maskPtr, pitch, src.pitch, mask.pitch, srcRectC.width(), srcRectC.height(),
			format, src.format);
	}

	addDirtyRect(dstRectC);
}

void ManagedSurface::blitFrom(const Surface &src, const Palette *srcPalette) {
	blitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Point(0, 0), srcPalette);
}

void ManagedSurface::blitFrom(const Surface &src, const Common::Point &destPos, const Palette *srcPalette) {
	blitFrom(src, Common::Rect(0, 0, src.w, src.h), destPos, srcPalette);
}

void ManagedSurface::blitFrom(const Surface &src, const Common::Rect &srcRect,
		const Common::Point &destPos, const Palette *srcPalette) {
	blitFromInner(src, srcRect, Common::Rect(destPos.x, destPos.y, destPos.x + srcRect.width(),
		destPos.y + srcRect.height()), srcPalette);
}

void ManagedSurface::blitFrom(const Surface &src, const Common::Rect &srcRect,
		const Common::Rect &destRect, const Palette *srcPalette) {
	blitFromInner(src, srcRect, destRect, srcPalette);
}

void ManagedSurface::blitFrom(const ManagedSurface &src) {
	blitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Point(0, 0));
}

void ManagedSurface::blitFrom(const ManagedSurface &src, const Common::Point &destPos) {
	blitFrom(src, Common::Rect(0, 0, src.w, src.h), destPos);
}

void ManagedSurface::blitFrom(const ManagedSurface &src, const Common::Rect &srcRect,
		const Common::Point &destPos) {
	if (src._transparentColorSet)
		transBlitFrom(src, srcRect, destPos);
	else
		blitFromInner(src._innerSurface, srcRect, Common::Rect(destPos.x, destPos.y, destPos.x + srcRect.width(),
			destPos.y + srcRect.height()), src._palette);
}

void ManagedSurface::blitFrom(const ManagedSurface &src, const Common::Rect &srcRect,
		const Common::Rect &destRect) {
	if (src._transparentColorSet)
		transBlitFrom(src, srcRect, destRect, (uint32)-1);
	else
		blitFromInner(src._innerSurface, srcRect, destRect, src._palette);
}

void ManagedSurface::blitFromInner(const Surface &src, const Common::Rect &srcRect,
		const Common::Rect &destRect, const Palette *srcPalette) {

	if (destRect.isEmpty())
		return;

	const int scaleX = SCALE_THRESHOLD * srcRect.width() / destRect.width();
	const int scaleY = SCALE_THRESHOLD * srcRect.height() / destRect.height();

	if (!srcRect.isValidRect())
		return;

	// Copy format so compiler can optimize better.
	// This should allow it to do some loop optimizations and condition hoisting as it can tell nothing
	// inside of the loop will clobber the format.
	Graphics::PixelFormat destFormat = format;
	Graphics::PixelFormat srcFormat = src.format;

	bool isSameFormat = (destFormat == srcFormat);
	if (!isSameFormat) {
		assert(destFormat.bytesPerPixel == 1 || destFormat.bytesPerPixel == 2 || destFormat.bytesPerPixel == 3 || destFormat.bytesPerPixel == 4);
		assert(srcFormat.bytesPerPixel == 1 || srcFormat.bytesPerPixel == 2 || srcFormat.bytesPerPixel == 3 || srcFormat.bytesPerPixel == 4);
		if (srcFormat.bytesPerPixel == 1) {
			// When the pixel format differs, the destination must be non-paletted
			assert(!destFormat.isCLUT8() && srcPalette && srcPalette->size() > 0);
		}
	}


	uint32 alphaMask = 0;
	if (srcFormat.aBits() > 0)
		alphaMask = (((static_cast<uint32>(1) << (srcFormat.aBits() - 1)) - 1) * 2 + 1) << srcFormat.aShift;

	const bool noScale = scaleX == SCALE_THRESHOLD && scaleY == SCALE_THRESHOLD;
	for (int destY = destRect.top, scaleYCtr = 0; destY < destRect.bottom; ++destY, scaleYCtr += scaleY) {
		if (destY < 0 || destY >= h)
			continue;
		const byte *srcP = (const byte *)src.getBasePtr(srcRect.left, scaleYCtr / SCALE_THRESHOLD + srcRect.top);
		byte *destP = (byte *)getBasePtr(destRect.left, destY);

		// For paletted format, assume the palette is the same and there is no transparency.
		// We can thus do a straight copy of the pixels.
		if (destFormat.isCLUT8() && noScale) {
			int width = srcRect.width();
			if (destRect.left + width > w)
				width = w - destRect.left;
			if (destRect.left < 0) {
				srcP -= destRect.left;
				destP -= destRect.left;
				width += destRect.left;
			}
			if (width > 0)
				Common::copy(srcP, srcP + width, destP);
			continue;
		}

		// Loop through drawing the pixels of the row
		for (int destX = destRect.left, xCtr = 0, scaleXCtr = 0; destX < destRect.right; ++destX, ++xCtr, scaleXCtr += scaleX) {
			if (destX < 0 || destX >= w)
				continue;

			const byte *srcVal = &srcP[scaleXCtr / SCALE_THRESHOLD * srcFormat.bytesPerPixel];
			byte *destVal = &destP[xCtr * destFormat.bytesPerPixel];
			if (destFormat.isCLUT8()) {
				*destVal = *srcVal;
				continue;
			}

			uint32 col = 0;
			// Use the src's pixel format to split up the source pixel
			if (srcFormat.bytesPerPixel == 1)
				col = *reinterpret_cast<const uint8 *>(srcVal);
			else if (srcFormat.bytesPerPixel == 2)
				col = *reinterpret_cast<const uint16 *>(srcVal);
			else if (srcFormat.bytesPerPixel == 4)
				col = *reinterpret_cast<const uint32 *>(srcVal);
			else
				col = READ_UINT24(srcVal);

			const bool isOpaque = srcFormat.isCLUT8() ? true : ((col & alphaMask) == alphaMask);
			const bool isTransparent = srcFormat.isCLUT8() ? false : ((col & alphaMask) == 0);

			uint32 destPixel = 0;

			// Need to check isOpaque in case alpha mask is 0
			if (!isOpaque && isTransparent) {
				// Completely transparent, so skip
				continue;
			} else if (isOpaque && isSameFormat) {
				// Completely opaque, same format, copy the entire value
				destPixel = col;
			} else {
				byte rSrc, gSrc, bSrc, aSrc;
				byte aDest = 0, rDest = 0, gDest = 0, bDest = 0;

				// Different format or partially transparent
				if (srcFormat.isCLUT8()) {
					srcPalette->get(col, rSrc, gSrc, bSrc);
					aSrc = 0xff;
				} else {
					srcFormat.colorToARGB(col, aSrc, rSrc, gSrc, bSrc);
				}

				if (isOpaque) {
					aDest = aSrc;
					rDest = rSrc;
					gDest = gSrc;
					bDest = bSrc;
				} else {
					// Partially transparent, so calculate new pixel colors
					uint32 destColor;
					if (destFormat.bytesPerPixel == 1)
						destColor = *reinterpret_cast<uint8 *>(destVal);
					else if (destFormat.bytesPerPixel == 2)
						destColor = *reinterpret_cast<uint16 *>(destVal);
					else if (destFormat.bytesPerPixel == 4)
						destColor = *reinterpret_cast<uint32 *>(destVal);
					else
						destColor = READ_UINT24(destVal);

					destFormat.colorToARGB(destColor, aDest, rDest, gDest, bDest);

					if (aDest == 0xff) {
						// Opaque target
						rDest = static_cast<uint8>((((rDest * (255U - aSrc) + rSrc * aSrc) * (257U * 257U)) >> 24) & 0xff);
						gDest = static_cast<uint8>((((gDest * (255U - aSrc) + gSrc * aSrc) * (257U * 257U)) >> 24) & 0xff);
						bDest = static_cast<uint8>((((bDest * (255U - aSrc) + bSrc * aSrc) * (257U * 257U)) >> 24) & 0xff);
					} else {
						// Translucent target
						double sAlpha = (double)aSrc / 255.0;
						double dAlpha = (double)aDest / 255.0;
						dAlpha *= (1.0 - sAlpha);
						rDest = static_cast<uint8>((rSrc * sAlpha + rDest * dAlpha) / (sAlpha + dAlpha));
						gDest = static_cast<uint8>((gSrc * sAlpha + gDest * dAlpha) / (sAlpha + dAlpha));
						bDest = static_cast<uint8>((bSrc * sAlpha + bDest * dAlpha) / (sAlpha + dAlpha));
						aDest = static_cast<uint8>(255. * (sAlpha + dAlpha));
					}
				}

				destPixel = destFormat.ARGBToColor(aDest, rDest, gDest, bDest);
			}

			if (destFormat.bytesPerPixel == 1)
				*(uint8 *)destVal = destPixel;
			else if (destFormat.bytesPerPixel == 2)
				*(uint16 *)destVal = destPixel;
			else if (destFormat.bytesPerPixel == 4)
				*(uint32 *)destVal = destPixel;
			else
				WRITE_UINT24(destVal, destPixel);
		}
	}

	addDirtyRect(destRect);
}

void ManagedSurface::transBlitFrom(const Surface &src, uint32 transColor, bool flipped,
		uint32 srcAlpha, const Palette *srcPalette) {
	transBlitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Rect(0, 0, this->w, this->h),
		transColor, flipped, srcAlpha, srcPalette);
}

void ManagedSurface::transBlitFrom(const Surface &src, const Common::Point &destPos,
		uint32 transColor, bool flipped, uint32 srcAlpha, const Palette *srcPalette) {
	transBlitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Rect(destPos.x, destPos.y,
		destPos.x + src.w, destPos.y + src.h), transColor, flipped, srcAlpha, srcPalette);
}

void ManagedSurface::transBlitFrom(const Surface &src, const Common::Rect &srcRect,
		const Common::Point &destPos, uint32 transColor, bool flipped, uint32 srcAlpha, const Palette *srcPalette) {
	transBlitFrom(src, srcRect, Common::Rect(destPos.x, destPos.y,
		destPos.x + srcRect.width(), destPos.y + srcRect.height()), transColor, flipped, srcAlpha, srcPalette);
}

void ManagedSurface::transBlitFrom(const Surface &src, const Common::Rect &srcRect, const Common::Rect &destRect, const Palette *srcPalette) {
	transBlitFrom(src, srcRect, destRect, 0, false, 0xff, srcPalette);
}

void ManagedSurface::transBlitFrom(const Surface &src, const Common::Rect &srcRect,
		const Common::Rect &destRect, uint32 transColor, bool flipped, uint32 srcAlpha,
		const Palette *srcPalette) {
	transBlitFromInner(src, srcRect, destRect, transColor, flipped, srcAlpha,
		srcPalette, nullptr);
}

void ManagedSurface::transBlitFrom(const ManagedSurface &src, uint32 transColor, bool flipped,
		uint32 srcAlpha) {
	transBlitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Rect(0, 0, this->w, this->h),
		transColor, flipped, srcAlpha);
}

void ManagedSurface::transBlitFrom(const ManagedSurface &src, const Common::Point &destPos,
		uint32 transColor, bool flipped, uint32 srcAlpha) {
	transBlitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Rect(destPos.x, destPos.y,
		destPos.x + src.w, destPos.y + src.h), transColor, flipped, srcAlpha);
}

void ManagedSurface::transBlitFrom(const ManagedSurface &src, const Common::Rect &srcRect,
		const Common::Point &destPos, uint32 transColor, bool flipped, uint32 srcAlpha) {
	uint32 tColor = !transColor && src._transparentColorSet ? src._transparentColor : transColor;

	transBlitFrom(src, srcRect, Common::Rect(destPos.x, destPos.y, destPos.x + srcRect.width(),
		destPos.y + srcRect.height()), tColor, flipped, srcAlpha);
}

void ManagedSurface::transBlitFrom(const ManagedSurface &src, const Common::Rect &srcRect,
		const Common::Rect &destRect, uint32 transColor, bool flipped, uint32 srcAlpha) {
	if (transColor == (uint32)-1 && src._transparentColorSet)
		transColor = src._transparentColor;
	const Palette *srcPalette = src._palette;
	const Palette *dstPalette = _palette;

	transBlitFromInner(src._innerSurface, srcRect, destRect, transColor, flipped,
		srcAlpha, srcPalette, dstPalette);
}

static byte *createPaletteLookup(const Palette *srcPalette, const Palette *dstPalette) {
	if (srcPalette->size() == 0 || dstPalette->size() == 0)
		return nullptr;

	byte *lookup = new byte[srcPalette->size()];
	byte rSrc, gSrc, bSrc;
	byte rDst, gDst, bDst;

	for (uint i = 0; i < srcPalette->size(); i++) {
		srcPalette->get(i, rSrc, gSrc, bSrc);
		if (i < dstPalette->size()) {
			dstPalette->get(i, rDst, gDst, bDst);
			if (rSrc == rDst && gSrc == gDst && bSrc == bDst) {
				lookup[i] = i;
				continue;
			}
		}

		lookup[i] = dstPalette->findBestColor(rSrc, gSrc, bSrc);
	}

	return lookup;
}

template<typename TSRC, typename TDEST>
void transBlitPixel(TSRC srcVal, TDEST &destVal, const Graphics::PixelFormat &srcFormat, const Graphics::PixelFormat &destFormat,
		uint32 srcAlpha, const Palette *srcPalette, const byte *lookup) {
	// Decode and re-encode each pixel
	byte aSrc, rSrc, gSrc, bSrc;
	if (srcFormat.isCLUT8()) {
		assert(srcPalette != nullptr && srcPalette->size() > 0);	// Catch the cases when palette is missing

		// Get the palette color
		srcPalette->get(srcVal, rSrc, gSrc, bSrc);
		aSrc = 0xff;
	} else {
		srcFormat.colorToARGB(srcVal, aSrc, rSrc, gSrc, bSrc);
	}

	if (srcAlpha != 0xff) {
		aSrc = aSrc * srcAlpha / 255;
	}

	byte aDest, rDest, gDest, bDest;
	if (aSrc == 0) {
		// Completely transparent, so skip
		return;
	} else if (aSrc == 0xff) {
		// Completely opaque, so copy RGB values over
		rDest = rSrc;
		gDest = gSrc;
		bDest = bSrc;
		aDest = 0xff;
	} else {
		// Partially transparent, so calculate new pixel colors
		destFormat.colorToARGB(destVal, aDest, rDest, gDest, bDest);
		double sAlpha = (double)aSrc / 255.0;
		double dAlpha = (double)aDest / 255.0;
		dAlpha *= (1.0 - sAlpha);
		rDest = static_cast<uint8>((rSrc * sAlpha + rDest * dAlpha) / (sAlpha + dAlpha));
		gDest = static_cast<uint8>((gSrc * sAlpha + gDest * dAlpha) / (sAlpha + dAlpha));
		bDest = static_cast<uint8>((bSrc * sAlpha + bDest * dAlpha) / (sAlpha + dAlpha));
		aDest = static_cast<uint8>(255. * (sAlpha + dAlpha));
	}

	destVal = destFormat.ARGBToColor(aDest, rDest, gDest, bDest);
}

template<>
void transBlitPixel<byte, byte>(byte srcVal, byte &destVal, const Graphics::PixelFormat &srcFormat, const Graphics::PixelFormat &destFormat,
		uint32 srcAlpha, const Palette *srcPalette, const byte *lookup) {
	if (srcAlpha == 0) {
		// Completely transparent, so skip
		return;
	}

	destVal = srcVal;

	if (lookup)
		destVal = lookup[destVal];
}

template<typename TSRC, typename TDEST>
void transBlit(const Surface &src, const Common::Rect &srcRect, ManagedSurface &dest, const Common::Rect &destRect,
		TSRC transColor, bool flipped, uint32 srcAlpha, const Palette *srcPalette,
		const Palette *dstPalette) {
	int scaleX = SCALE_THRESHOLD * srcRect.width() / destRect.width();
	int scaleY = SCALE_THRESHOLD * srcRect.height() / destRect.height();
	byte rst = 0, gst = 0, bst = 0, rdt = 0, gdt = 0, bdt = 0;
	byte r = 0, g = 0, b = 0;

	byte *lookup = nullptr;
	if (srcPalette && dstPalette)
		lookup = createPaletteLookup(srcPalette, dstPalette);

	// If we're dealing with a 32-bit source surface, we need to split up the RGB,
	// since we'll want to find matching RGB pixels irrespective of the alpha
	bool isSrcTrans32 = src.format.aBits() != 0 && transColor != (uint32)-1 && transColor > 0;
	if (isSrcTrans32) {
		src.format.colorToRGB(transColor, rst, gst, bst);
	}
	bool isDestTrans32 = dest.format.aBits() != 0 && dest.hasTransparentColor();
	if (isDestTrans32) {
		dest.format.colorToRGB(dest.getTransparentColor(), rdt, gdt, bdt);
	}

	// Loop through drawing output lines
	for (int destY = destRect.top, scaleYCtr = 0; destY < destRect.bottom; ++destY, scaleYCtr += scaleY) {
		if (destY < 0 || destY >= dest.h)
			continue;
		const TSRC *srcLine = (const TSRC *)src.getBasePtr(srcRect.left, scaleYCtr / SCALE_THRESHOLD + srcRect.top);
		TDEST *destLine = (TDEST *)dest.getBasePtr(destRect.left, destY);

		// Loop through drawing the pixels of the row
		for (int destX = destRect.left, xCtr = 0, scaleXCtr = 0; destX < destRect.right; ++destX, ++xCtr, scaleXCtr += scaleX) {
			if (destX < 0 || destX >= dest.w)
				continue;

			TSRC srcVal = srcLine[flipped ? src.w - scaleXCtr / SCALE_THRESHOLD - 1 : scaleXCtr / SCALE_THRESHOLD];
			TDEST &destVal = destLine[xCtr];

			dest.format.colorToRGB(destVal, r, g, b);

			// Check if dest pixel is transparent
			bool isDestPixelTrans = false;
			if (isDestTrans32) {
				dest.format.colorToRGB(destVal, r, g, b);
				if (rdt == r && gdt == g && bdt == b)
					isDestPixelTrans = true;
			} else if (dest.hasTransparentColor()) {
				isDestPixelTrans = destVal == dest.getTransparentColor();
			}

			if (isSrcTrans32) {
				src.format.colorToRGB(srcVal, r, g, b);
				if (rst == r && gst == g && bst == b)
					continue;

			} else if (srcVal == transColor)
				continue;

			if (isDestPixelTrans)
				// Remove transparent color on dest so it isn't alpha blended
				destVal = 0;

			transBlitPixel<TSRC, TDEST>(srcVal, destVal, src.format, dest.format, srcAlpha, srcPalette, lookup);
		}
	}

	delete[] lookup;
}

#define HANDLE_BLIT(SRC_BYTES, DEST_BYTES, SRC_TYPE, DEST_TYPE) \
	if (src.format.bytesPerPixel == SRC_BYTES && format.bytesPerPixel == DEST_BYTES) \
		transBlit<SRC_TYPE, DEST_TYPE>(src, srcRect, *this, destRect, transColor, flipped, srcAlpha, srcPalette, dstPalette); \
	else

void ManagedSurface::transBlitFromInner(const Surface &src, const Common::Rect &srcRect,
		const Common::Rect &destRect, uint32 transColor, bool flipped,
		uint32 srcAlpha, const Palette *srcPalette, const Palette *dstPalette) {
	if (src.w == 0 || src.h == 0 || destRect.width() == 0 || destRect.height() == 0)
		return;

	HANDLE_BLIT(1, 1, uint8,  uint8)
	HANDLE_BLIT(1, 2, uint8,  uint16)
	HANDLE_BLIT(1, 4, uint8,  uint32)
	HANDLE_BLIT(2, 1, uint16, uint8)
	HANDLE_BLIT(2, 2, uint16, uint16)
	HANDLE_BLIT(2, 4, uint16, uint32)
	HANDLE_BLIT(4, 1, uint32, uint8)
	HANDLE_BLIT(4, 2, uint32, uint16)
	HANDLE_BLIT(4, 4, uint32, uint32)
	error("Surface::transBlitFrom: bytesPerPixel must be 1, 2, or 4");

	// Mark the affected area
	addDirtyRect(destRect);
}

#undef HANDLE_BLIT

void ManagedSurface::blendBlitFrom(const Surface &src, const int flipping, const uint colorMod,
		const TSpriteBlendMode blend, const AlphaType alphaType) {
	blendBlitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Rect(0, 0, this->w, this->h),
		flipping, colorMod, blend, alphaType);
}

void ManagedSurface::blendBlitFrom(const Surface &src, const Common::Point &destPos,
		const int flipping, const uint colorMod, const TSpriteBlendMode blend, const
		AlphaType alphaType) {
	blendBlitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Rect(destPos.x, destPos.y,
		destPos.x + src.w, destPos.y + src.h), flipping, colorMod, blend, alphaType);
}

void ManagedSurface::blendBlitFrom(const Surface &src, const Common::Rect &srcRect,
		const Common::Point &destPos, const int flipping, const uint colorMod,
		const TSpriteBlendMode blend, const AlphaType alphaType) {
	blendBlitFrom(src, srcRect, Common::Rect(destPos.x, destPos.y,
		destPos.x + srcRect.width(), destPos.y + srcRect.height()), flipping, colorMod, blend, alphaType);
}

void ManagedSurface::blendBlitFrom(const Surface &src, const Common::Rect &srcRect,
		const Common::Rect &destRect, const int flipping, const uint colorMod,
		const TSpriteBlendMode blend, const AlphaType alphaType) {
	blendBlitFromInner(src, srcRect, destRect, flipping, colorMod, blend, alphaType);
}

void ManagedSurface::blendBlitFrom(const ManagedSurface &src, const int flipping, const uint colorMod,
		const TSpriteBlendMode blend, const AlphaType alphaType) {
	blendBlitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Rect(0, 0, this->w, this->h),
		flipping, colorMod, blend, alphaType);
}

void ManagedSurface::blendBlitFrom(const ManagedSurface &src, const Common::Point &destPos,
		const int flipping, const uint colorMod, const TSpriteBlendMode blend, const
		AlphaType alphaType) {
	blendBlitFrom(src, Common::Rect(0, 0, src.w, src.h), Common::Rect(destPos.x, destPos.y,
		destPos.x + src.w, destPos.y + src.h), flipping, colorMod, blend, alphaType);
}

void ManagedSurface::blendBlitFrom(const ManagedSurface &src, const Common::Rect &srcRect,
		const Common::Point &destPos, const int flipping, const uint colorMod,
		const TSpriteBlendMode blend, const AlphaType alphaType) {
	blendBlitFrom(src, srcRect, Common::Rect(destPos.x, destPos.y,
		destPos.x + srcRect.width(), destPos.y + srcRect.height()), flipping, colorMod, blend, alphaType);
}

void ManagedSurface::blendBlitFrom(const ManagedSurface &src, const Common::Rect &srcRect,
		const Common::Rect &destRect, const int flipping, const uint colorMod,
		const TSpriteBlendMode blend, const AlphaType alphaType) {
	blendBlitFromInner(src._innerSurface, srcRect, destRect, flipping, colorMod, blend, alphaType);
}

void ManagedSurface::blendBlitFromInner(const Surface &src, const Common::Rect &srcRect,
		const Common::Rect &destRect, const int flipping, const uint colorMod,
		const TSpriteBlendMode blend, const AlphaType alphaType) {
	Common::Rect srcRectC = srcRect;
	Common::Rect destRectC = destRect;

	if (!isBlendBlitPixelFormatSupported(src.format, format)) {
		warning("ManagedSurface::blendBlitFrom only accepts RGBA32!");
		return;
	}

	// Alpha is zero
	if ((colorMod & MS_ARGB(255, 0, 0, 0)) == 0) return;

	const int scaleX = BlendBlit::getScaleFactor(srcRectC.width(), destRectC.width());
	const int scaleY = BlendBlit::getScaleFactor(srcRectC.height(), destRectC.height());
	int scaleXoff = 0, scaleYoff = 0;

	if (destRectC.left < 0) {
		scaleXoff = (-destRectC.left * scaleX) % BlendBlit::SCALE_THRESHOLD;
		srcRectC.left += -destRectC.left * scaleX / BlendBlit::SCALE_THRESHOLD;
		destRectC.left = 0;
	}

	if (destRectC.top < 0) {
		scaleYoff = (-destRectC.top * scaleY) % BlendBlit::SCALE_THRESHOLD;
		srcRectC.top += -destRectC.top * scaleY / BlendBlit::SCALE_THRESHOLD;
		destRectC.top = 0;
	}

	if (destRectC.right > w) {
		srcRectC.right -= (destRectC.right - src.w) * scaleX / BlendBlit::SCALE_THRESHOLD;
		destRectC.right = w;
	}

	if (destRectC.bottom > h) {
		srcRectC.bottom -= (destRectC.bottom - src.h) * scaleY / BlendBlit::SCALE_THRESHOLD;
		destRectC.bottom = h;
	}

	if (flipping & FLIP_H) {
		int tmp_w = srcRectC.width();
		srcRectC.left = src.w - srcRectC.right;
		srcRectC.right = srcRectC.left + tmp_w;
		scaleXoff = (BlendBlit::SCALE_THRESHOLD - (scaleXoff + destRectC.width() * scaleX)) % BlendBlit::SCALE_THRESHOLD;
	}

	if (flipping & FLIP_V) {
		int tmp_h = srcRectC.height();
		srcRectC.top = src.h - srcRectC.bottom;
		srcRectC.bottom = srcRectC.top + tmp_h;
		scaleYoff = (BlendBlit::SCALE_THRESHOLD - (scaleYoff + destRectC.height() * scaleY)) % BlendBlit::SCALE_THRESHOLD;
	}

	if (!destRectC.isEmpty() && !srcRectC.isEmpty()) {
		BlendBlit::blit(
			(byte *)getBasePtr(0, 0),
			(const byte *)src.getBasePtr(srcRectC.left, srcRectC.top),
			pitch, src.pitch,
			destRectC.left, destRectC.top,
			destRectC.width(), destRectC.height(),
			scaleX, scaleY,
			scaleXoff, scaleYoff,
			colorMod, flipping,
			blend, alphaType);

		// Mark the affected area
		addDirtyRect(destRectC);
	}
}

Common::Rect ManagedSurface::blendBlitTo(ManagedSurface &target,
										 const int posX, const int posY,
										 const int flipping,
										 const Common::Rect *srcRect,
										 const uint colorMod,
										 const int width, const int height,
										 const TSpriteBlendMode blend,
										 const AlphaType alphaType) {
	return blendBlitTo(*target.surfacePtr(), posX, posY, flipping, srcRect, colorMod, width, height, blend, alphaType);
}
Common::Rect ManagedSurface::blendBlitTo(Surface &target,
										 const int posX, const int posY,
										 const int flipping,
										 const Common::Rect *srcRect,
										 const uint colorMod,
										 const int width, const int height,
										 const TSpriteBlendMode blend,
										 const AlphaType alphaType) {
	Common::Rect srcArea = srcRect ? *srcRect : Common::Rect(0, 0, w, h);
	Common::Rect dstArea(posX, posY, posX + (width == -1 ? srcArea.width() : width), posY + (height == -1 ? srcArea.height() : height));
	
	if (!isBlendBlitPixelFormatSupported(format, target.format)) {
		warning("ManagedSurface::blendBlitTo only accepts RGBA32!");
		return Common::Rect(0, 0, 0, 0);
	}

	// Alpha is zero
	if ((colorMod & MS_ARGB(255, 0, 0, 0)) == 0) return Common::Rect(0, 0, 0, 0);

	const int scaleX = BlendBlit::getScaleFactor(srcArea.width(), dstArea.width());
	const int scaleY = BlendBlit::getScaleFactor(srcArea.height(), dstArea.height());
	int scaleXoff = 0, scaleYoff = 0;

	if (dstArea.left < 0) {
		scaleXoff = (-dstArea.left * scaleX) % BlendBlit::SCALE_THRESHOLD;
		srcArea.left += -dstArea.left * scaleX / BlendBlit::SCALE_THRESHOLD;
		dstArea.left = 0;
	}

	if (dstArea.top < 0) {
		scaleYoff = (-dstArea.top * scaleY) % BlendBlit::SCALE_THRESHOLD;
		srcArea.top += -dstArea.top * scaleY / BlendBlit::SCALE_THRESHOLD;
		dstArea.top = 0;
	}

	if (dstArea.right > target.w) {
		srcArea.right -= (dstArea.right - target.w) * scaleX / BlendBlit::SCALE_THRESHOLD;
		dstArea.right = target.w;
	}

	if (dstArea.bottom > target.h) {
		srcArea.bottom -= (dstArea.bottom - target.h) * scaleY / BlendBlit::SCALE_THRESHOLD;
		dstArea.bottom = target.h;
	}

	if (flipping & FLIP_H) {
		int tmp_w = srcArea.width();
		srcArea.left = w - srcArea.right;
		srcArea.right = srcArea.left + tmp_w;
		scaleXoff = (BlendBlit::SCALE_THRESHOLD - (scaleXoff + dstArea.width() * scaleX)) % BlendBlit::SCALE_THRESHOLD;
	}

	if (flipping & FLIP_V) {
		int tmp_h = srcArea.height();
		srcArea.top = h - srcArea.bottom;
		srcArea.bottom = srcArea.top + tmp_h;
		scaleYoff = (BlendBlit::SCALE_THRESHOLD - (scaleYoff + dstArea.height() * scaleY)) % BlendBlit::SCALE_THRESHOLD;
	}

	if (!dstArea.isEmpty() && !srcArea.isEmpty()) {
		BlendBlit::blit(
			(byte *)target.getBasePtr(0, 0),
			(const byte *)getBasePtr(srcArea.left, srcArea.top),
			target.pitch, pitch,
			dstArea.left, dstArea.top,
			dstArea.width(), dstArea.height(),
			scaleX, scaleY,
			scaleXoff, scaleYoff,
			colorMod, flipping,
			blend, alphaType);
	}

	if (dstArea.isEmpty()) return Common::Rect(0, 0, 0, 0);
	else return Common::Rect(0, 0, dstArea.width(), dstArea.height());
}

void ManagedSurface::blendFillRect(Common::Rect r,
		const uint colorMod, const TSpriteBlendMode blend) {
	if (!isBlendBlitPixelFormatSupported(format, format)) {
		warning("ManagedSurface::blendFillRect only accepts RGBA32!");
		return;
	}

	// Alpha is zero
	if ((colorMod & MS_ARGB(255, 0, 0, 0)) == 0) return;

	// Use faster memory fills where possible
	if (blend == BLEND_NORMAL &&
	    (colorMod & MS_ARGB(255, 0, 0, 0)) == MS_ARGB(255, 0, 0, 0)) {
		fillRect(r, colorMod);
		return;
	}

	r.clip(w, h);

	if (!r.isValidRect())
		return;

	BlendBlit::fill(
		(byte *)getBasePtr(0, 0), pitch,
		r.width(), r.height(),
		colorMod, blend);

	// Mark the affected area
	addDirtyRect(r);
}

void ManagedSurface::markAllDirty() {
	addDirtyRect(Common::Rect(0, 0, this->w, this->h));
}

void ManagedSurface::addDirtyRect(const Common::Rect &r) {
	if (_owner) {
		Common::Rect bounds = r;
		bounds.clip(Common::Rect(0, 0, this->w, this->h));
		bounds.translate(_offsetFromOwner.x, _offsetFromOwner.y);
		_owner->addDirtyRect(bounds);
	}
}

void ManagedSurface::clear(uint32 color) {
	if (!empty())
		fillRect(getBounds(), color);
}

void ManagedSurface::clearPalette() {
	if (_palette) {
		delete _palette;
		_palette = nullptr;
	}
}

bool ManagedSurface::hasPalette() const {
	return _palette && _palette->size() > 0;
}

void ManagedSurface::grabPalette(byte *colors, uint start, uint num) const {
	if (_palette)
		_palette->grab(colors, start, num);
}

void ManagedSurface::setPalette(const byte *colors, uint start, uint num) {
	if (!_palette)
		_palette = new Palette(256);
	_palette->set(colors, start, num);

	if (_owner)
		_owner->setPalette(colors, start, num);
}

} // End of namespace Graphics
