#include <cassert>
#include <iostream>

#include "string_view.h"

#include "BitStream.h"
#include "EncodingTables.h"
#include "Encoder.h"
#include "Decoder.h"

//template<typename T>
//class optional {
//public:
//
//private:
//	bool m_has_value = false;
//	T m_value;
//};



enum BLOCK_TYPE {
	NEW_SYMBOL_NAME_LOCAL_SCOPE,
	NEW_SYMBOL_NAME_GLOBAL_SCOPE,
	SYMBOL_NAME_REFERENCE_LOCAL,
	SYMBOL_NAME_REFERENCE_GLOBAL,

	//COMMENT_BLOCK,
	//ENCODED_SYMBOL,
	//SEPARATOR,
	//ENCODED_NUMBER,
	//STRING_BLOCK,
	//SPECIAL_CHAR_BLOCK,
	//REPEATER,
	//UNDETERMINED_BLOCK,

	//INDENT_BLOCK,
	//NEW_LINE,
};

struct SymbolName {
	std::string text;
	unsigned int id;
};

struct Block {
	string_view text;
	BLOCK_TYPE type;
};

//Block get_next_block();


void test_StringView() {
	string_view str("abc");
	str.remove_prefix(0);
	assert(str.length() == 3);
	assert(str[0] == 'a');
}

static void test_symbol_name_encode_decode(const char * text) {
	OutputBitStream output;
	string_view str(text);
	Encoder::encodeNextSymbolName(output, str);
	assert(str.empty());
	InputBitStream input(output.toString());
	assert(input.remainingBits() == output.sizeInBits());
	std::string decoded = Decoder::decodeNextSymbolName(input);
	assert(decoded == text);
	assert(input.isEmpty());
}

int main() {
	//Block b = get_next_block();
	//if (b.type == SYMBOL_TEXT) {
	//	encode_symbol_text(b.text);
	//}

	test_StringView();
	test_OutputBitStream();
	test_InputBitStream();
	test_Encoder();

	test_symbol_name_encode_decode("ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz_0123456789");
	test_symbol_name_encode_decode("aClass");
	test_symbol_name_encode_decode("AClass");
	test_symbol_name_encode_decode("A_Class__");
	test_symbol_name_encode_decode("_A__C_lass");
	test_symbol_name_encode_decode("AClass2");
	test_symbol_name_encode_decode("uint64_t");
	test_symbol_name_encode_decode("AClass_1024");
	test_symbol_name_encode_decode("_123456");
	test_symbol_name_encode_decode("_1091673");
	test_symbol_name_encode_decode("_3671091");
	test_symbol_name_encode_decode("_10911092");
}

