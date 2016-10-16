#include "Encoder.h"

#include "BitStream.h"

#include "string_view.h"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <cctype>

static int count_nb_digits(const string_view & str, int start_pos) {
	int nb_digits = 0;
	for (int i = start_pos; i < str.length(); ++i) {
		if (std::isdigit(str[i])) {
			nb_digits += 1;
		} else {
			break;
		}
	}
	return nb_digits;
}

static int extract_4_digits_number(string_view str) {
	int number = 0;

	const int length = std::min(4, str.length());
	for (int i = 0; i < length; ++i) {
		char c = str[i];
		assert(std::isdigit(c));
		if (i == 0 && c == '0') {
			// if we face a leading zero, we treat it as a plain number
			assert(number == 0);
			break;
		}
		else {
			number = number * 10 + (c - '0');
		}
	}
	return number;
}

static bool is_case_valid_for_next_letter(string_view str, int index, BitStream::CASE_KIND case_kind) {
	assert(index < str.length());
	for (int i = index + 1; i < str.length(); ++i) {
		char c = str[i];
		if (std::isalpha(c)) {
			if (case_kind == BitStream::CASE_LOWER && std::isupper(c))
				return false;
			if (case_kind == BitStream::CASE_UPPER && std::islower(c))
				return false;
		}
	}
	return true;
}

namespace Encoder {
	void encodeNumber(OutputBitStream & stream, string_view str) {
		assert(!str.empty());

		int number = extract_4_digits_number(str);

		if (number <= 3) {
			stream.appendBits(SYMBOL_NAME_CODES::DIGITS_2BITS, SYMBOL_NAME_CODES::BIT_WIDTH);
			stream.appendBits(number, 2);
			// take care of leading zeros
			if (number == 0 && str.length() > 1) {
				str.remove_prefix(1);
				return encodeNumber(stream, str);
			}
		} else if (number <= 67) {
			stream.appendBits(SYMBOL_NAME_CODES::DIGITS_6BITS, SYMBOL_NAME_CODES::BIT_WIDTH);
			stream.appendBits(number - 4, 6);
		} else if (number <= 1091) {
			stream.appendBits(SYMBOL_NAME_CODES::DIGITS_10BITS, SYMBOL_NAME_CODES::BIT_WIDTH);
			stream.appendBits(number - 68, 10);
			if (str.length() > 4) {
				str.remove_prefix(4);
				return encodeNumber(stream, str);
			}
		} else {
			assert(str.length() > 3);
			number =
				(str[0] - '0') * 100 +
				(str[1] - '0') * 10 +
				(str[2] - '0') * 1;
			stream.appendBits(SYMBOL_NAME_CODES::DIGITS_10BITS, SYMBOL_NAME_CODES::BIT_WIDTH);
			stream.appendBits(number - 68, 10);
			str.remove_prefix(3);
			return encodeNumber(stream, str);
		}
	}

	void handleCurrentCaseMismatch(OutputBitStream & stream, string_view str, int index) {
		if (is_case_valid_for_next_letter(str, index, stream.currentCase())) {
			stream.appendBits(SYMBOL_NAME_CODES::CASE_INVERSE_ONCE, SYMBOL_NAME_CODES::BIT_WIDTH);
		} else {
			stream.appendBits(SYMBOL_NAME_CODES::CASE_INVERSE_PERMANENT, SYMBOL_NAME_CODES::BIT_WIDTH);
			stream.invertCurrentCase();
		}
	}

	void encodeNextSymbolName(OutputBitStream & stream, string_view & str) {
		for (int i = 0; i < str.length(); ++i) {
			char c = str[i];
			if (unlikely(std::isdigit(c))) {
				int nb_digits = count_nb_digits(str, i);
				assert(nb_digits > 0);

				encodeNumber(stream, string_view(str, i, nb_digits));
				i += (nb_digits - 1);
				assert(i < str.length());
			} else {
				unsigned int code;

				if (likely(std::islower(c))) {
					if (stream.currentCase() != BitStream::CASE_LOWER) {
						handleCurrentCaseMismatch(stream, str, i);
					}
					code = SYMBOL_NAME_CODES::LETTER_A + (c - 'a');
				} else if (std::isupper(c)) {
					if (stream.currentCase() != BitStream::CASE_UPPER) {
						handleCurrentCaseMismatch(stream, str, i);
					}
					code = SYMBOL_NAME_CODES::LETTER_A + (c - 'A');
				} else if (c == '_') {
					code = SYMBOL_NAME_CODES::UNDERSCORE;
				} else {
					str.remove_prefix(i);
					return;
				}

				stream.appendBits(code, SYMBOL_NAME_CODES::BIT_WIDTH);
			}
		}
		str.clear();
	}
}

// ----------------------------------------------------------------

static void test_encode_number(const char * number, std::string expected_str) {
	OutputBitStream stream;
	Encoder::encodeNumber(stream, number);
	std::string encoded = stream.toString();
	assert(encoded == expected_str);
}

static void test_encode(const char * text, std::string expected_str) {
	OutputBitStream stream;
	string_view str(text);
	Encoder::encodeNextSymbolName(stream, str);
	assert(str.empty());
	std::string encoded = stream.toString();
	if (encoded != expected_str) {
		std::cerr << "Failure on \"" << text << "\":\n";
		std::cerr << "- '" << encoded << "' instead of\n";
		std::cerr << "- '" << expected_str << "'\n\n";
	}
}

static std::string to_string(SYMBOL_NAME_CODES::ENUM e) {
	return OutputBitStream::toString(static_cast<unsigned char>(e), SYMBOL_NAME_CODES::BIT_WIDTH);
}

static std::string quick_encode(unsigned short number, SYMBOL_NAME_CODES::ENUM e) {
	assert(number <= 1091);
	switch (e) {
	case SYMBOL_NAME_CODES::DIGITS_2BITS:
		return to_string(e) + OutputBitStream::toString(number, 2);
	case SYMBOL_NAME_CODES::DIGITS_6BITS:
		return to_string(e) + OutputBitStream::toString(number - 4, 6);
	case SYMBOL_NAME_CODES::DIGITS_10BITS:
		return to_string(e) + OutputBitStream::toString(number - 68, 10);
	}
	assert(false);
	throw "invalid params";
}

static void test_deserialize_encoding() {
	OutputBitStream output;
	string_view str("a_1");
	Encoder::encodeNextSymbolName(output, str);
	assert(str.empty());
	InputBitStream input(output.toString());
	assert(input.readSymbolCode() == SYMBOL_NAME_CODES::LETTER_A);
	assert(input.readSymbolCode() == SYMBOL_NAME_CODES::UNDERSCORE);
	assert(input.readSymbolCode() == SYMBOL_NAME_CODES::DIGITS_2BITS);
	assert(input.readBits(2) == 1);
	assert(input.isEmpty());
}

static void test_leading_zero_is_well_encoded() {
	test_encode_number("00", quick_encode(0, SYMBOL_NAME_CODES::DIGITS_2BITS) + quick_encode(0, SYMBOL_NAME_CODES::DIGITS_2BITS));
	test_encode_number("03", quick_encode(0, SYMBOL_NAME_CODES::DIGITS_2BITS) + quick_encode(3, SYMBOL_NAME_CODES::DIGITS_2BITS));
	test_encode_number("000", quick_encode(0, SYMBOL_NAME_CODES::DIGITS_2BITS) + quick_encode(0, SYMBOL_NAME_CODES::DIGITS_2BITS) + quick_encode(0, SYMBOL_NAME_CODES::DIGITS_2BITS));
	test_encode_number("200001", quick_encode(200, SYMBOL_NAME_CODES::DIGITS_10BITS) + quick_encode(0, SYMBOL_NAME_CODES::DIGITS_2BITS) + quick_encode(0, SYMBOL_NAME_CODES::DIGITS_2BITS) + quick_encode(1, SYMBOL_NAME_CODES::DIGITS_2BITS));
}

void test_Encoder() {
	assert(count_nb_digits("", 0) == 0);
	assert(count_nb_digits("1", 0) == 1);
	assert(count_nb_digits("a", 0) == 0);
	assert(count_nb_digits("a1", 0) == 0);
	assert(count_nb_digits("a1", 1) == 1);
	assert(count_nb_digits("123", 0) == 3);
	assert(count_nb_digits("a123b", 1) == 3);

	assert(extract_4_digits_number("0") == 0);
	assert(extract_4_digits_number("1") == 1);
	assert(extract_4_digits_number("01") == 0);
	assert(extract_4_digits_number("10") == 10);
	assert(extract_4_digits_number("12") == 12);
	assert(extract_4_digits_number("123") == 123);
	assert(extract_4_digits_number("1234") == 1234);
	assert(extract_4_digits_number("12345") == 1234);

	assert(to_string(SYMBOL_NAME_CODES::LETTER_B) == "00001");

	test_encode_number("0", quick_encode(0, SYMBOL_NAME_CODES::DIGITS_2BITS));
	test_encode_number("3", quick_encode(3, SYMBOL_NAME_CODES::DIGITS_2BITS));
	test_encode_number("4", quick_encode(4, SYMBOL_NAME_CODES::DIGITS_6BITS));
	test_encode_number("67", quick_encode(67, SYMBOL_NAME_CODES::DIGITS_6BITS));
	test_encode_number("68", quick_encode(68, SYMBOL_NAME_CODES::DIGITS_10BITS));
	test_encode_number("1091", quick_encode(1091, SYMBOL_NAME_CODES::DIGITS_10BITS));
	test_encode_number("1092", quick_encode(109, SYMBOL_NAME_CODES::DIGITS_10BITS) + quick_encode(2, SYMBOL_NAME_CODES::DIGITS_2BITS));
	test_encode_number("109167", quick_encode(1091, SYMBOL_NAME_CODES::DIGITS_10BITS) + quick_encode(67, SYMBOL_NAME_CODES::DIGITS_6BITS));
	test_encode_number("109168", quick_encode(1091, SYMBOL_NAME_CODES::DIGITS_10BITS) + quick_encode(68, SYMBOL_NAME_CODES::DIGITS_10BITS));
	test_encode_number("109268", quick_encode(109, SYMBOL_NAME_CODES::DIGITS_10BITS) + quick_encode(268, SYMBOL_NAME_CODES::DIGITS_10BITS));

	test_encode("a", to_string(SYMBOL_NAME_CODES::LETTER_A));
	test_encode("_", to_string(SYMBOL_NAME_CODES::UNDERSCORE));
	test_encode("A", to_string(SYMBOL_NAME_CODES::CASE_INVERSE_ONCE) + to_string(SYMBOL_NAME_CODES::LETTER_A));
	test_encode("zA", to_string(SYMBOL_NAME_CODES::LETTER_Z) + to_string(SYMBOL_NAME_CODES::CASE_INVERSE_ONCE) + to_string(SYMBOL_NAME_CODES::LETTER_A));
	test_encode("Az", to_string(SYMBOL_NAME_CODES::CASE_INVERSE_ONCE) + to_string(SYMBOL_NAME_CODES::LETTER_A) + to_string(SYMBOL_NAME_CODES::LETTER_Z));
	test_encode("AZ", to_string(SYMBOL_NAME_CODES::CASE_INVERSE_PERMANENT) + to_string(SYMBOL_NAME_CODES::LETTER_A) + to_string(SYMBOL_NAME_CODES::LETTER_Z));
	test_encode("A_", to_string(SYMBOL_NAME_CODES::CASE_INVERSE_ONCE) + to_string(SYMBOL_NAME_CODES::LETTER_A) + to_string(SYMBOL_NAME_CODES::UNDERSCORE));
	test_encode("A_z", to_string(SYMBOL_NAME_CODES::CASE_INVERSE_ONCE) + to_string(SYMBOL_NAME_CODES::LETTER_A) + to_string(SYMBOL_NAME_CODES::UNDERSCORE) + to_string(SYMBOL_NAME_CODES::LETTER_Z));
	test_encode("A_Z", to_string(SYMBOL_NAME_CODES::CASE_INVERSE_PERMANENT) + to_string(SYMBOL_NAME_CODES::LETTER_A) + to_string(SYMBOL_NAME_CODES::UNDERSCORE) + to_string(SYMBOL_NAME_CODES::LETTER_Z));

	test_deserialize_encoding();
	test_leading_zero_is_well_encoded();
}