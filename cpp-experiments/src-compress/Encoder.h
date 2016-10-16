#pragma once

class string_view;
class OutputBitStream;

namespace Encoder {
	void encodeNumber(OutputBitStream & stream, string_view str);
	void handleCurrentCaseMismatch(OutputBitStream & stream, string_view str, int index);
	void encodeNextSymbolName(OutputBitStream & stream, string_view & str);
}

void test_Encoder();
