#pragma once

#include <string>

//class string_view;
class InputBitStream;

namespace Decoder {
	std::string decodeNextSymbolName(InputBitStream & stream);
};