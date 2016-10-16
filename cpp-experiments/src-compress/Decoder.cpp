#include "Decoder.h"
#include "BitStream.h"

#include <cassert>

static int decode2BitsNumber(InputBitStream & stream) {
	assert(stream.remainingBits() >= 2);
	return static_cast<int>(stream.readBits(2));
}

static int decode6BitsNumber(InputBitStream & stream) {
	assert(stream.remainingBits() >= 6);
	return static_cast<int>(stream.readBits(6)) + 4;
}

static int decode10BitsNumber(InputBitStream & stream) {
	assert(stream.remainingBits() >= 10);
	return static_cast<int>(stream.readBits(10)) + 68;
}

namespace Decoder {
	std::string decodeNextSymbolName(InputBitStream & stream) {
		std::string str;

		bool caseIsInversedOnce = false;
		while (stream.remainingBits() >= SYMBOL_NAME_CODES::BIT_WIDTH) {
			auto code = stream.readSymbolCode();
			if (code >= SYMBOL_NAME_CODES::LETTER_A && code <= SYMBOL_NAME_CODES::LETTER_Z) {
				static_assert(SYMBOL_NAME_CODES::LETTER_Z - SYMBOL_NAME_CODES::LETTER_A == 25, "Problem with A-Z");
				int letterNumber = static_cast<int>(code - SYMBOL_NAME_CODES::LETTER_A);
				if (stream.currentCase() == BitStream::CASE_LOWER) {
					str += (caseIsInversedOnce ? 'A' : 'a') + letterNumber;
				}
				else if (stream.currentCase() == BitStream::CASE_UPPER) {
					str += (caseIsInversedOnce ? 'a' : 'A') + letterNumber;
				}
				else {
					throw make_logic_error("Invalid current case!");
				}
				caseIsInversedOnce = false;
			}
			else if (code == SYMBOL_NAME_CODES::UNDERSCORE) {
				str += '_';
			}
			else if (code == SYMBOL_NAME_CODES::CASE_INVERSE_ONCE) {
				caseIsInversedOnce = true;
			}
			else if (code == SYMBOL_NAME_CODES::CASE_INVERSE_PERMANENT) {
				stream.invertCurrentCase();
			}
			else if (code == SYMBOL_NAME_CODES::DIGITS_2BITS) {
				str += std::to_string(decode2BitsNumber(stream));
			}
			else if (code == SYMBOL_NAME_CODES::DIGITS_6BITS) {
				str += std::to_string(decode6BitsNumber(stream));
			}
			else if (code == SYMBOL_NAME_CODES::DIGITS_10BITS) {
				str += std::to_string(decode10BitsNumber(stream));
			}
			else {
				throw make_logic_error("Invalid symbol name code!");
			}
		}

		return str;
	}
}