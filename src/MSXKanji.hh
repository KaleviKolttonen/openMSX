// $Id$

#ifndef __MSXKANJI_HH__
#define __MSXKANJI_HH__

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class MSXKanji : public MSXDevice
{
public:
	MSXKanji(const XMLElement& config, const EmuTime& time);
	virtual ~MSXKanji();
	
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);
	virtual void reset(const EmuTime& time);

private:
	Rom rom;
	int adr1, adr2;
};

} // namespace openmsx

#endif //__MSXKANJI_HH__

