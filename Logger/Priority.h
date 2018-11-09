#pragma once

#include <string>

namespace Priority
{
	enum Level : uint8_t
	{
		Off,
		Fatal,
		Error,
		Warning,
		Info,
		Communication,
		Debug,
		Verbose
	};

	std::string toString(Level priority)
	{
		switch (priority)
		{
		case Off: return "Off";
		case Fatal: return "Fatal";
		case Error: return "Error";
		case Warning: return "Warning";
		case Info: return "Info";
		case Communication: return "Communication";
		case Debug: return "Debug";
		case Verbose: return "Verbose";
		default: throw std::exception("Invalid priority recieved");
		}
	}
}
