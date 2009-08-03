// $Id$

/*
TODO:
- Verify model for 5th sprite number calculation.
  For example, does it have the right value in text mode?
- Further investigate sprite collision registers:
   - If there is NO collision, the value of these registers constantly changes.
     Could this be some kind of indication for the scanline XY coords???
   - Bit 9 of the Y coord (odd/even page??) is not yet implemented.
*/

#include "SpriteChecker.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "serialize.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

SpriteChecker::SpriteChecker(VDP& vdp_, RenderSettings& renderSettings,
                             EmuTime::param time)
	: vdp(vdp_), vram(vdp.getVRAM())
	, limitSpritesSetting(renderSettings.getLimitSprites())
	, frameStartTime(time)
{
	vram.spriteAttribTable.setObserver(this);
	vram.spritePatternTable.setObserver(this);
}

void SpriteChecker::reset(EmuTime::param time)
{
	vdp.setSpriteStatus(0); // TODO 0x00 or 0x1F  (blueMSX has 0x1F)
	collisionX = 0;
	collisionY = 0;

	frameStart(time);

	updateSpritesMethod = &SpriteChecker::updateSprites1;
	mode0 = false;
}

static inline SpriteChecker::SpritePattern doublePattern(SpriteChecker::SpritePattern a)
{
	// bit-pattern "abcd...." gets expanded to "aabbccdd"
	// upper 16 bits (of a 32 bit number) contain the pattern
	// lower 16 bits must be zero
	//                               // abcdefghijklmnop0000000000000000
	a = (a | (a >> 8)) & 0xFF00FF00; // abcdefgh00000000ijklmnop00000000
	a = (a | (a >> 4)) & 0xF0F0F0F0; // abcd0000efgh0000ijkl0000mnop0000
	a = (a | (a >> 2)) & 0xCCCCCCCC; // ab00cd00ef00gh00ij00kl00mn00op00
	a = (a | (a >> 1)) & 0xAAAAAAAA; // a0b0c0d0e0f0g0h0i0j0k0l0m0n0o0p0
	return a | (a >> 1);             // aabbccddeeffgghhiijjkkllmmnnoopp
}

inline SpriteChecker::SpritePattern SpriteChecker::calculatePatternNP(
	unsigned patternNr, unsigned y)
{
	const byte* patternPtr = vram.spritePatternTable.getReadArea(0, 256 * 8);
	unsigned index = patternNr * 8 + y;
	SpritePattern pattern = patternPtr[index] << 24;
	if (vdp.getSpriteSize() == 16) {
		pattern |= patternPtr[index + 16] << 16;
	}
	return !vdp.isSpriteMag() ? pattern : doublePattern(pattern);
}
inline SpriteChecker::SpritePattern SpriteChecker::calculatePatternPlanar(
	unsigned patternNr, unsigned y)
{
	const byte* ptr0;
	const byte* ptr1;
	vram.spritePatternTable.getReadAreaPlanar(0, 256 * 8, ptr0, ptr1);
	unsigned index = patternNr * 8 + y;
	const byte* patternPtr = (index & 1) ? ptr1 : ptr0;
	index /= 2;
	SpritePattern pattern = patternPtr[index] << 24;
	if (vdp.getSpriteSize() == 16) {
		pattern |= patternPtr[index + (16 / 2)] << 16;
	}
	return !vdp.isSpriteMag() ? pattern : doublePattern(pattern);
}

void SpriteChecker::updateSprites1(int limit)
{
	if (vdp.spritesEnabledFast()) {
		if (vdp.isDisplayEnabled()) {
			// in display area
			checkSprites1(currentLine, limit);
		} else {
			// in border, only check last line of top border
			int l0 = vdp.getLineZero() - 1;
			if ((currentLine <= l0) && (l0 < limit)) {
				checkSprites1(l0, l0 + 1);
			}
		}
	}
	currentLine = limit;
}

inline void SpriteChecker::checkSprites1(int minLine, int maxLine)
{
	// Calculate display line.
	// This is the line sprites are checked at; the line they are displayed
	// at is one lower.
	int displayDelta = vdp.getVerticalScroll() - vdp.getLineZero();

	// Get sprites for this line and detect 5th sprite if any.
	bool limitSprites = limitSpritesSetting.getValue();
	int size = vdp.getSpriteSize();
	bool mag = vdp.isSpriteMag();
	int magSize = (mag + 1) * size;
	const byte* attributePtr = vram.spriteAttribTable.getReadArea(0, 32 * 4);
	byte patternIndexMask = size == 16 ? 0xFC : 0xFF;
	int sprite = 0;
	for (/**/; sprite < 32; sprite++, attributePtr += 4) {
		int y = attributePtr[0];
		if (y == 208) break;
		for (int line = minLine; line < maxLine; ++line) {
			// Calculate line number within the sprite.
			int displayLine = line + displayDelta;
			int spriteLine = (displayLine - y) & 0xFF;
			if (spriteLine >= magSize) {
				// skip ahead till sprite becomes visible
				line += 256 - spriteLine - 1; // -1 because of for-loop
				continue;
			}
			int visibleIndex = spriteCount[line];
			if (visibleIndex == 4) {
				// Five sprites on a line.
				// According to TMS9918.pdf 5th sprite detection is only
				// active when F flag is zero.
				byte status = vdp.getStatusReg0();
				if ((status & 0xC0) == 0) {
					vdp.setSpriteStatus(
					     0x40 | (status & 0x20) | sprite);
				}
				if (limitSprites) continue;
			}
			++spriteCount[line];
			SpriteInfo& sip = spriteBuffer[line][visibleIndex];
			int patternIndex = attributePtr[2] & patternIndexMask;
			if (mag) spriteLine /= 2;
			sip.pattern = calculatePatternNP(patternIndex, spriteLine);
			sip.x = attributePtr[1];
			if (attributePtr[3] & 0x80) sip.x -= 32;
			sip.colorAttrib = attributePtr[3];
		}
	}
	byte status = vdp.getStatusReg0();
	if (~status & 0x40) {
		// No 5th sprite detected, store number of latest sprite processed.
		vdp.setSpriteStatus((status & 0x60) | (std::min(sprite, 31)));
	}

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The spriteBuffer array is filled now, so we can bail out.
	if (vdp.getStatusReg0() & 0x20) return;

	/*
	Model for sprite collision: (or "coincidence" in TMS9918 data sheet)
	Reset when status reg is read.
	Set when sprite patterns overlap.
	Color doesn't matter: sprites of color 0 can collide.
	Sprites with off-screen position can collide.

	Implemented by checking every pair for collisions.
	For large numbers of sprites that would be slow,
	but there are max 4 sprites and therefore max 6 pairs.
	If any collision is found, method returns at once.
	*/
	for (int line = minLine; line < maxLine; ++line) {
		int minXCollision = 999;
		for (int i = std::min(4, spriteCount[line]); --i >= 1; /**/) {
			int x_i = spriteBuffer[line][i].x;
			SpritePattern pattern_i = spriteBuffer[line][i].pattern;
			for (int j = i; --j >= 0; ) {
				// Do sprite i and sprite j collide?
				int x_j = spriteBuffer[line][j].x;
				int dist = x_j - x_i;
				if ((-magSize < dist) && (dist < magSize)) {
					SpritePattern pattern_j = spriteBuffer[line][j].pattern;
					if (dist < 0) {
						pattern_j <<= -dist;
					} else {
						pattern_j >>= dist;
					}
					SpritePattern colPat = pattern_i & pattern_j;
					if (colPat) {
						minXCollision = std::min<int>(minXCollision,
						    x_i + Math::countLeadingZeros(colPat));
					}
				}
			}
		}
		if (minXCollision != 999) {
			vdp.setSpriteStatus(vdp.getStatusReg0() | 0x20);
			// verified: collision coords are also filled
			//           in for sprite mode 1
			// x-coord should be increased by 12
			// y-coord                         8
			collisionX = minXCollision + 12;
			collisionY = line - vdp.getLineZero() + 8;
			return; // don't check lines with higher Y-coord
		}
	}
}

void SpriteChecker::updateSprites2(int limit)
{
	// TODO merge this with updateSprites1()?
	if (vdp.spritesEnabledFast()) {
		if (vdp.isDisplayEnabled()) {
			// in display area
			checkSprites2(currentLine, limit);
		} else {
			// in border, only check last line of top border
			int l0 = vdp.getLineZero() - 1;
			if ((currentLine <= l0) && (l0 < limit)) {
				checkSprites2(l0, l0 + 1);
			}
		}
	}
	currentLine = limit;
}

inline void SpriteChecker::checkSprites2(int minLine, int maxLine)
{
	// Calculate display line.
	// This is the line sprites are checked at; the line they are displayed
	// at is one lower.
	int displayDelta = vdp.getVerticalScroll() - vdp.getLineZero();

	// Get sprites for this line and detect 5th sprite if any.
	bool limitSprites = limitSpritesSetting.getValue();
	int size = vdp.getSpriteSize();
	bool mag = vdp.isSpriteMag();
	int magSize = (mag + 1) * size;
	int patternIndexMask = (size == 16) ? 0xFC : 0xFF;

	// because it gave a measurable performance boost, we duplicated the
	// code for planar and non-planar modes
	int sprite = 0;
	if (planar) {
		const byte* attributePtr0;
		const byte* attributePtr1;
		vram.spriteAttribTable.getReadAreaPlanar(
			512, 32 * 4, attributePtr0, attributePtr1);
		// TODO: Verify CC implementation.
		for (/**/; sprite < 32; ++sprite) {
			int y = attributePtr0[2 * sprite + 0];
			if (y == 216) break;
			for (int line = minLine; line < maxLine; ++line) {
				// Calculate line number within the sprite.
				int displayLine = line + displayDelta;
				int spriteLine = (displayLine - y) & 0xFF;
				if (spriteLine >= magSize) {
					// skip ahead till sprite is visible
					line += 256 - spriteLine - 1;
					continue;
				}
				int visibleIndex = spriteCount[line];
				if (visibleIndex == 8) {
					// Nine sprites on a line.
					// According to TMS9918.pdf 5th sprite detection is only
					// active when F flag is zero. Stuck to this for V9938.
					// Dragon Quest 2 needs this
					byte status = vdp.getStatusReg0();
					if ((status & 0xC0) == 0) {
						vdp.setSpriteStatus(
						     0x40 | (status & 0x20) | sprite);
					}
					if (limitSprites) continue;
				}
				if (mag) spriteLine /= 2;
				int colorIndex = (-1 << 10) | (sprite * 16 + spriteLine);
				byte colorAttrib =
					vram.spriteAttribTable.readPlanar(colorIndex);
				// Sprites with CC=1 are only visible if preceded by
				// a sprite with CC=0.
				if ((colorAttrib & 0x40) && visibleIndex == 0) continue;
				spriteCount[line] = visibleIndex + 1;
				SpriteInfo& sip = spriteBuffer[line][visibleIndex];
				int patternIndex = attributePtr0[2 * sprite + 1] & patternIndexMask;
				sip.pattern = calculatePatternPlanar(patternIndex, spriteLine);
				sip.x = attributePtr1[2 * sprite + 0];
				if (colorAttrib & 0x80) sip.x -= 32;
				sip.colorAttrib = colorAttrib;
				// set sentinel (see below)
				spriteBuffer[line][visibleIndex + 1].colorAttrib = 0;
			}
		}
	} else {
		const byte* attributePtr0 =
			vram.spriteAttribTable.getReadArea(512, 32 * 4);
		// TODO: Verify CC implementation.
		for (/**/; sprite < 32; ++sprite) {
			int y = attributePtr0[4 * sprite + 0];
			if (y == 216) break;
			for (int line = minLine; line < maxLine; ++line) {
				// Calculate line number within the sprite.
				int displayLine = line + displayDelta;
				int spriteLine = (displayLine - y) & 0xFF;
				if (spriteLine >= magSize) {
					// skip ahead till sprite is visible
					line += 256 - spriteLine - 1;
					continue;
				}
				int visibleIndex = spriteCount[line];
				if (visibleIndex == 8) {
					// Nine sprites on a line.
					// According to TMS9918.pdf 5th sprite detection is only
					// active when F flag is zero. Stuck to this for V9938.
					// Dragon Quest 2 needs this
					byte status = vdp.getStatusReg0();
					if ((status & 0xC0) == 0) {
						vdp.setSpriteStatus(
						     0x40 | (status & 0x20) | sprite);
					}
					if (limitSprites) continue;
				}
				if (mag) spriteLine /= 2;
				int colorIndex = (-1 << 10) | (sprite * 16 + spriteLine);
				byte colorAttrib =
					vram.spriteAttribTable.readNP(colorIndex);
				// Sprites with CC=1 are only visible if preceded by
				// a sprite with CC=0.
				if ((colorAttrib & 0x40) && visibleIndex == 0) continue;
				spriteCount[line] = visibleIndex + 1;
				SpriteInfo& sip = spriteBuffer[line][visibleIndex];
				int patternIndex = attributePtr0[4 * sprite + 2] & patternIndexMask;
				sip.pattern = calculatePatternNP(patternIndex, spriteLine);
				sip.x = attributePtr0[4 * sprite + 1];
				if (colorAttrib & 0x80) sip.x -= 32;
				sip.colorAttrib = colorAttrib;
				// Set sentinel. Sentinel is actually only
				// needed for sprites with CC=1.
				// In the past we set the sentinel (for all
				// lines) at the end. But it's slightly faster
				// to do it only for lines that actually
				// contain sprites (even if sentinel gets
				// overwritten a couple of times for lines with
				// many sprites).
				spriteBuffer[line][visibleIndex + 1].colorAttrib = 0;
			}
		}
	}

	byte status = vdp.getStatusReg0();
	if (~status & 0x40) {
		// No 9th sprite detected, store number of latest sprite processed.
		vdp.setSpriteStatus((status & 0x60) | (std::min(sprite, 31)));
	}

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The visibleSprites array is filled now, so we can bail out.
	if (vdp.getStatusReg0() & 0x20) return;

	/*
	Model for sprite collision: (or "coincidence" in TMS9918 data sheet)
	Reset when status reg is read.
	Set when sprite patterns overlap.
	Color doesn't matter: sprites of color 0 can collide.
	  TODO: V9938 data book denies this (page 98).
	Sprites with off-screen position can collide.

	Implemented by checking every pair for collisions.
	For large numbers of sprites that would be slow.
	There are max 8 sprites and therefore max 42 pairs.
	  TODO: Maybe this is slow... Think of something faster.
	        Probably new approach is needed anyway for OR-ing.
	*/
	for (int line = minLine; line < maxLine; ++line) {
		int minXCollision = 999; // no collision
		SpriteInfo* visibleSprites = spriteBuffer[line];
		for (int i = std::min(8, spriteCount[line]); --i >= 1; /**/) {
			// If CC or IC is set, this sprite cannot collide.
			if (visibleSprites[i].colorAttrib & 0x60) continue;

			int x_i = visibleSprites[i].x;
			SpritePattern pattern_i = visibleSprites[i].pattern;
			for (int j = i; --j >= 0; ) {
				// If CC or IC is set, this sprite cannot collide.
				if (visibleSprites[j].colorAttrib & 0x60) continue;

				// Do sprite i and sprite j collide?
				int x_j = visibleSprites[j].x;
				int dist = x_j - x_i;
				if ((-magSize < dist) && (dist < magSize)) {
					SpritePattern pattern_j = visibleSprites[j].pattern;
					if (dist < 0) {
						pattern_j <<= -dist;
					} else {
						pattern_j >>= dist;
					}
					SpritePattern colPat = pattern_i & pattern_j;
					if (colPat) {
						minXCollision = std::min<int>(minXCollision,
						    x_i + Math::countLeadingZeros(colPat));
					}
				}
			}
		}
		if (minXCollision != 999) {
			vdp.setSpriteStatus(vdp.getStatusReg0() | 0x20);
			// x-coord should be increased by 12
			// y-coord                         8
			collisionX = minXCollision + 12;
			collisionY = line - vdp.getLineZero() + 8;
			return; // don't check lines with higher Y-coord
		}
	}
}

void SpriteChecker::updateSprites0(int /*limit*/)
{
	// If this method is called, that means somewhere a check for sprite
	// mode 0 is missing and performance is being wasted.
	// The updateSpritesN methods are called by checkUntil, which is
	// documented as not allowed to be called in sprite mode 0.
	assert(false);
}

template<typename Archive>
void SpriteChecker::serialize(Archive& ar, unsigned /*version*/)
{
	if (ar.isLoader()) {
		// Recalculate from VDP state:
		//   frameStartTime, updateSpritesMethod,
		//   currentLine, mode0, planar
		// Invalidate data in spriteBuffer and spriteCount, will
		// be recalculated when needed.
		frameStartTime.reset(vdp.getFrameStartTime());
		frameStart(vdp.getFrameStartTime());
		setDisplayMode(vdp.getDisplayMode(), frameStartTime.getTime());
	}
	ar.serialize("collisionX", collisionX);
	ar.serialize("collisionY", collisionY);
}
INSTANTIATE_SERIALIZE_METHODS(SpriteChecker);

} // namespace openmsx
