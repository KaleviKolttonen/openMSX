// $Id$

// This class implements the Intel 8255 chip
//
// * Only the 8255 is emulated, no surrounding hardware. 
//   Use the class I8255Interface to do that.
// * Only mode 0 (basic input/output) is implemented
// * For techncal details on 8255 see
//    http://w3.qahwah.net/joost/openMSX/8255.pdf

#ifndef __I8255_HH__
#define __I8255_HH__

#include "openmsx.hh"

namespace openmsx {

class EmuTime;

class I8255Interface
{
public:
	virtual ~I8255Interface() {}
	virtual byte readA(const EmuTime &time) = 0;
	virtual byte readB(const EmuTime &time) = 0;
	virtual nibble readC0(const EmuTime &time) = 0;
	virtual nibble readC1(const EmuTime &time) = 0;
	virtual void writeA(byte value, const EmuTime &time) = 0;
	virtual void writeB(byte value, const EmuTime &time) = 0;
	virtual void writeC0(nibble value, const EmuTime &time) = 0;
	virtual void writeC1(nibble value, const EmuTime &time) = 0;
};

class I8255
{
public:
	I8255(I8255Interface &interf, const EmuTime &time); 
	~I8255(); 
	
	void reset(const EmuTime &time);

	byte readPortA(const EmuTime &time);
	byte readPortB(const EmuTime &time);
	byte readPortC(const EmuTime &time);
	byte readControlPort(const EmuTime &time);
	void writePortA(byte value, const EmuTime &time);
	void writePortB(byte value, const EmuTime &time);
	void writePortC(byte value, const EmuTime &time);
	void writeControlPort(byte value, const EmuTime &time);
	
private:
	byte readC0(const EmuTime &time);
	byte readC1(const EmuTime &time);
	void outputPortA(byte value, const EmuTime &time);
	void outputPortB(byte value, const EmuTime &time);
	void outputPortC(byte value, const EmuTime &time);

	int control;
	int latchPortA;
	int latchPortB;
	int latchPortC;

	I8255Interface &interface;
};

} // namespace openmsx

#endif
