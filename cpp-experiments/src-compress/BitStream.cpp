#include "BitStream.h"

#include <cassert>
#include <algorithm>

static unsigned char extract_char_bits_in_range(unsigned int value, unsigned int first_bit, unsigned int nb_bits) {
	assert(first_bit <= 31);
	assert(nb_bits > 0 && nb_bits <= 8);
	assert(first_bit >= nb_bits - 1);
	value >>= first_bit - (nb_bits - 1);
	unsigned int mask = (1 << nb_bits) - 1;
	return static_cast<unsigned char>(value & mask);
}

std::string OutputBitStream::toString(unsigned int c, unsigned int nb_bits) {
	assert(nb_bits <= 32);

	std::string str;
	str.reserve(nb_bits);

	for (unsigned int i = 0; i < nb_bits; ++i) {
		unsigned int mask = 1 << i;
		if (c & mask) {
			str = "1" + str;
		} else {
			str = "0" + str;
		}
	}
	assert(str.length() == nb_bits);
	return str;
}

std::string OutputBitStream::toString() const {
	std::string str;
	str.reserve(m_data.size() * 8 + m_pending_bits);

	for (unsigned char c : m_data) {
		str += toString(c, 8);
	}
	str += toString(m_pending_data, m_pending_bits);

	return str;
}

void OutputBitStream::appendBits(unsigned int value, unsigned int nb_bits_to_encode) {
	assert(nb_bits_to_encode <= 32);
	if (nb_bits_to_encode == 0)
		return;

	assert(m_pending_bits < 8);
	const unsigned int remaining_bits = 8 - m_pending_bits;
	const unsigned int nb_bits = std::min(remaining_bits, nb_bits_to_encode);
	m_pending_data <<= nb_bits;
	m_pending_data += extract_char_bits_in_range(value, nb_bits_to_encode - 1, nb_bits);
	m_pending_bits += nb_bits;
	assert(m_pending_bits <= 8);

	if (m_pending_bits == 8) {
		m_data.push_back(m_pending_data);
		m_pending_data = 0;
		m_pending_bits = 0;
	}

	if (nb_bits != nb_bits_to_encode) {
		return appendBits(value, nb_bits_to_encode - nb_bits);
	}
}

// ----------------------------------------------------------------

bool InputBitStream::isEmpty() const {
	assert(m_current_bit <= m_size_in_bits);
	return m_current_bit == m_size_in_bits;
}

InputBitStream::InputBitStream(std::string stream) {
	unsigned char value = 0;
	for (char c : stream) {
		assert(c == '0' || c == '1');
		value <<= 1;
		if (c == '1')
			value += 1;
		m_size_in_bits += 1;
		if (m_size_in_bits % 8 == 0) {
			m_data.push_back(value);
			value = 0;
		}
	}
	if (m_size_in_bits % 8 > 0) {
		if (value != 0) {
			int padding_size = 8 - (m_size_in_bits % 8);
			value <<= padding_size;
		}
		m_data.push_back(value);
	}
}

bool InputBitStream::readSymbolCode(SYMBOL_NAME_CODES::ENUM & code) {
	unsigned int value;
	if (!readBits(SYMBOL_NAME_CODES::BIT_WIDTH, value)) {
		return false;
	}
	code = static_cast<SYMBOL_NAME_CODES::ENUM>(value);
	return true;
}

bool InputBitStream::readBits(unsigned int nb_bits, unsigned int & result) {
	assert(nb_bits > 0 && nb_bits <= 32);
	if (m_current_bit + nb_bits > m_size_in_bits)
		return false;

	result = 0;
	for (unsigned int i = 0; i < nb_bits; ++i) {
		int char_pos = m_current_bit / 8;
		int bit_pos = 7 - (m_current_bit % 8);
		auto data = m_data[char_pos];
		int bit_value = data & (1 << bit_pos) ? 1 : 0;
		result <<= 1;
		if (bit_value == 1)
			result += 1;
		m_current_bit += 1;
	}
	assert(m_current_bit <= m_size_in_bits);
	return true;
}

// ----------------------------------------------------------------

static void test_appendBits() {
	OutputBitStream stream;
	stream.appendBits(0b10010, 5);
	assert(stream.sizeInBits() == 5);
	assert(stream.toString() == "10010");
	stream.appendBits(0b1011, 4);
	assert(stream.sizeInBits() == 9);
	assert(stream.toString() == "100101011");
	stream.appendBits(0b10110010, 8);
	assert(stream.sizeInBits() == 17);
	assert(stream.toString() == "10010101110110010");
	stream.appendBits(0b1011001, 7);
	assert(stream.sizeInBits() == 24);
	assert(stream.toString() == "100101011101100101011001");
}

void test_OutputBitStream() {
	assert(extract_char_bits_in_range(0b1010, 0, 1) == 0);
	assert(extract_char_bits_in_range(0b1011, 0, 1) == 1);
	assert(extract_char_bits_in_range(0b1010, 1, 1) == 1);
	assert(extract_char_bits_in_range(0b1001, 1, 1) == 0);
	assert(extract_char_bits_in_range(0b1001, 2, 2) == 0);
	assert(extract_char_bits_in_range(0b1001, 2, 3) == 1);
	assert(extract_char_bits_in_range(0b1001, 3, 2) == 0b10);
	assert(extract_char_bits_in_range(0b1001, 3, 4) == 0b1001);

	assert(OutputBitStream::toString(0, 0) == "");
	assert(OutputBitStream::toString(0, 3) == "000");
	assert(OutputBitStream::toString(0b100, 3) == "100");
	assert(OutputBitStream::toString(0b11001, 5) == "11001");
	assert(OutputBitStream::toString(0b10001110, 8) == "10001110");
	assert(OutputBitStream::toString(0b1110110000110001110, 19) == "1110110000110001110");

	test_appendBits();
}

void test_InputBitStream() {
	{
		InputBitStream stream("1101011");
		assert(!stream.isEmpty());
		assert(stream.readBits(3) == 0b110);
		assert(!stream.isEmpty());
		assert(stream.readBits(4) == 0b1011);
		assert(stream.isEmpty());
	}
	{
		InputBitStream stream("000011111");
		assert(stream.readBits(9) == 0b000011111);
		assert(stream.isEmpty());
	}
}