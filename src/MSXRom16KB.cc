// $Id$

#include "MSXRom16KB.hh" 
#include <string>
#include <iostream>
#include <fstream>

#include "config.h"

MSXRom16KB::MSXRom16KB(MSXConfig::Device *config) : MSXDevice(config)
{
	PRT_DEBUG("Creating an MSXRom16KB object");
}

MSXRom16KB::~MSXRom16KB()
{
	PRT_DEBUG("Destructing an MSXRom16KB object");
	delete [] memoryBank;
}

void MSXRom16KB::init()
{
	MSXDevice::init();
	loadFile(&memoryBank, 0x4000);
	registerSlots();
}

byte MSXRom16KB::readMem(word address, Emutime &time)
{
	return memoryBank [address & 0x3fff];
}
