#pragma once
#include <string>
#include "inipp.h"

// Why did you make that function?
// Yes.
std::string to_string(const std::string& s) { return s; }
template<typename T>
bool InitializeValue(
	const std::string& section,
	const std::string& param,
	T& var, const T& def, inipp::Ini<char>& ini)
{
	if (!inipp::extract(ini.sections[section][param], var))
	{
		ini.sections[section][param] = to_string(def);
		var = def;
		return false;
	}
	else
		return true;
}