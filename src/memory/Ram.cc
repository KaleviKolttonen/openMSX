// $Id$

#include "Ram.hh"
#include "DeviceConfig.hh"
#include "SimpleDebuggable.hh"
#include "XMLElement.hh"
#include "Base64.hh"
#include "HexDump.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <algorithm>
#include <cstring>
#include <zlib.h>

using std::string;

namespace openmsx {

class RamDebuggable : public SimpleDebuggable
{
public:
	RamDebuggable(MSXMotherBoard& motherBoard, const string& name,
	              const string& description, Ram& ram);
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);
private:
	Ram& ram;
};


Ram::Ram(const DeviceConfig& config, const string& name,
         const string& description, unsigned size)
	: xml(*config.getXML())
	, ram(size)
	, debuggable(new RamDebuggable(config.getMotherBoard(), name, description, *this))
{
	clear();
}

Ram::Ram(const DeviceConfig& config, unsigned size)
	: xml(*config.getXML())
	, ram(size)
{
	clear();
}

Ram::~Ram()
{
}

void Ram::clear(byte c)
{
	if (const XMLElement* init = xml.findChild("initialContent")) {
		// get pattern (and decode)
		const string& encoding = init->getAttribute("encoding");
		unsigned done = 0;
		if (encoding == "gz-base64") {
			string tmp = Base64::decode(init->getData());
			uLongf dstLen = ram.size();
			if (uncompress(reinterpret_cast<Bytef*>(ram.data()), &dstLen,
			               reinterpret_cast<const Bytef*>(tmp.data()), uLong(tmp.size()))
			     != Z_OK) {
				throw MSXException("Error while decompressing initialContent.");
			}
			done = dstLen;
		} else if ((encoding == "hex") || (encoding == "base64")) {
			string out = (encoding == "hex")
			           ? HexDump::decode(init->getData())
				   : Base64 ::decode(init->getData());
			done = std::min<unsigned>(ram.size(), out.size());
			memcpy(ram.data(), out.data(), done);
		} else {
			throw MSXException("Unsupported encoding \"" + encoding + "\" for initialContent");
		}

		// repeat pattern over whole ram
		unsigned left = ram.size() - done;
		while (left) {
			unsigned tmp = std::min(done, left);
			memcpy(&ram[done], &ram[0], tmp);
			done += tmp;
			left -= tmp;
		}
	} else {
		// no init pattern specified
		memset(ram.data(), c, ram.size());
	}

}

const string& Ram::getName() const
{
	return debuggable.get()->getName();
}

RamDebuggable::RamDebuggable(MSXMotherBoard& motherBoard,
                             const string& name,
                             const string& description, Ram& ram_)
	: SimpleDebuggable(motherBoard, name, description, ram_.getSize())
	, ram(ram_)
{
}

byte RamDebuggable::read(unsigned address)
{
	return ram[address];
}

void RamDebuggable::write(unsigned address, byte value)
{
	ram[address] = value;
}


template<typename Archive>
void Ram::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize_blob("ram", ram.data(), ram.size());
}
INSTANTIATE_SERIALIZE_METHODS(Ram);

} // namespace openmsx
