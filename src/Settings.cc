// $Id$

#include "Settings.hh"
#include "CommandController.hh"
#include <sstream>
#include "RenderSettings.hh"


// Force template instantiation
template class EnumSetting<RenderSettings::Accuracy>;
template class EnumSetting<bool>;


// Setting implementation:

Setting::Setting(const std::string &name_, const std::string &description_)
	: name(name_), description(description_)
{
	SettingsManager::instance()->registerSetting(name, this);
}

Setting::~Setting()
{
	SettingsManager::instance()->unregisterSetting(name);
}


// BooleanSetting implementation:

BooleanSetting::BooleanSetting(
	const std::string &name_, const std::string &description_,
	bool initialValue)
	: Setting(name_, description_), value(initialValue)
{
	type = "on - off";
}

std::string BooleanSetting::getValueString() const
{
	if (value) {
		return std::string("on");
	} else {
		return std::string("off");
	}
}

void BooleanSetting::setValueString(const std::string &valueString)
{
	if (valueString == "on") {
		value = true;
	} else if (valueString == "off") {
		value = false;
	} else {
		throw CommandException(
			"Not a valid boolean: \"" + valueString + "\"");
	}
	// TODO: Inform listeners.
}

void BooleanSetting::tabCompletion(std::vector<std::string> &tokens) const
{
	std::list<std::string> values;
	values.push_back("on");
	values.push_back("off");
	CommandController::completeString(tokens, values);
}


// IntegerSetting implementation:

IntegerSetting::IntegerSetting(
	const std::string &name_, const std::string &description_,
	int initialValue, int minValue_, int maxValue_)
	: Setting(name_, description_), value(initialValue),
	  minValue(minValue_), maxValue(maxValue_)
{
	std::ostringstream out;
	out << minValue << " - " << maxValue;
	type = out.str();
}

std::string IntegerSetting::getValueString() const
{
	std::ostringstream out;
	out << value;
	return out.str();
}

void IntegerSetting::setValueString(const std::string &valueString)
{
	char *endPtr;
	long newValue = strtol(valueString.c_str(), &endPtr, 0);
	if (*endPtr != '\0') {
		throw CommandException(
			"Not a valid integer: \"" + valueString + "\"");
	}

	if (newValue < minValue) {
		newValue = minValue;
	} else if (newValue > maxValue) {
		newValue = maxValue;
	}
	value = (int)newValue;

	// TODO: Inform listeners.
}


// EnumSetting implementation:

template <class T>
EnumSetting<T>::EnumSetting(
	const std::string &name_, const std::string &description_,
	const T &initialValue,
	const std::map<const std::string, T> &map_)
	: Setting(name_, description_), value(initialValue), map(map_)
{
	std::ostringstream out;
	MapIterator it = map.begin();
	out << it->first;
	for (it++; it != map.end(); it++) {
		out << ", " << it->first;
	}
	type = out.str();
}

template <class T>
std::string EnumSetting<T>::getValueString() const
{
	MapIterator it = map.begin();
	while (it != map.end()) {
		if (it->second == value) {
			return it->first;
		}
		it++;
	}
	assert(false);
	return std::string("<unknown>");
}

template <class T>
void EnumSetting<T>::setValueString(const std::string &valueString)
{
	MapIterator it = map.find(valueString);
	if (it != map.end()) {
		value = it->second;
	} else {
		throw CommandException(
			"Not a valid value: \"" + valueString + "\"");
	}
}

template <class T>
void EnumSetting<T>::tabCompletion(std::vector<std::string> &tokens) const
{
	std::list<std::string> values;
	for (MapIterator it = map.begin(); it != map.end(); it++) {
		values.push_back(it->first);
	}
	CommandController::completeString(tokens, values);
}

// SettingsManager implementation:

SettingsManager::SettingsManager()
	: setCommand(this)
{
	CommandController::instance()->registerCommand(&setCommand, "set");
}

SettingsManager::~SettingsManager()
{
	CommandController::instance()->unregisterCommand(&setCommand, "set");
}

// SetCommand implementation:

SettingsManager::SetCommand::SetCommand(SettingsManager *manager_)
	: manager(manager_)
{
}

void SettingsManager::SetCommand::execute(
	const std::vector<std::string> &tokens, const EmuTime &time)
{
	int nrTokens = tokens.size();
	if (nrTokens == 0 || nrTokens > 3) {
		throw CommandException("Wrong number of parameters");
	}

	if (nrTokens == 1) {
		std::map<std::string, Setting *>::const_iterator it =
			manager->settingsMap.begin();
		for (; it != manager->settingsMap.end(); it++) {
			print(it->first);
		}
		return;
	}

	const std::string &name = tokens[1];
	Setting *setting = manager->getByName(name);
	if (!setting) {
		throw CommandException("There is no setting named \"" + name + "\"");
	}

	if (nrTokens == 2) {
		// Info.
		print(setting->getDescription());
		print("current value   : " + setting->getValueString());
		print("possible values : " + setting->getTypeString());
	} else {
		// Change.
		const std::string &valueString = tokens[2];
		setting->setValueString(valueString);
	}

}

void SettingsManager::SetCommand::help(
	const std::vector<std::string> &tokens) const
{
	print("set            : list all settings");
	print("set name       : information on setting");
	print("set name value : change setting's value");
}

void SettingsManager::SetCommand::tabCompletion(
	std::vector<std::string> &tokens) const
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			std::list<std::string> settings;
			std::map<std::string, Setting *>::const_iterator it
				= manager->settingsMap.begin();
			for (; it != manager->settingsMap.end(); it++) {
				settings.push_back(it->first);
			}
			CommandController::completeString(tokens, settings);
			break;
		}
		case 3: {
			// complete setting value
			std::map<std::string, Setting*>::iterator it =
				manager->settingsMap.find(tokens[1]);
			if (it != manager->settingsMap.end()) {
				it->second->tabCompletion(tokens);
			}
			break;
		}
	}
}
