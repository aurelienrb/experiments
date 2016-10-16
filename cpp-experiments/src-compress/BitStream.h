#pragma once

#include "EncodingTables.h"

#include <vector>
#include <string>
#include <stdexcept>

#define make_logic_error(str) std::logic_error(std::string("Error (" __FUNCTION__ ") : ") + str)
#define likely(x) x
#define unlikely(x) x

class BitStream {
public:
	enum CASE_KIND { CASE_LOWER, CASE_UPPER };

	CASE_KIND currentCase() const {
		return m_currentCase;
	}
	void invertCurrentCase() {
		m_currentCase = (m_currentCase == CASE_LOWER) ? CASE_UPPER : CASE_LOWER;
	}
private:
	CASE_KIND m_currentCase = CASE_LOWER;
};

// Allows to serialize data by adding a few bits (from 1 to 32) at a time.
class OutputBitStream : public BitStream {
public:
	static std::string toString(unsigned int c, unsigned int nb_bits);

public:
	unsigned int sizeInBits() const {
		return static_cast<unsigned int>(m_data.size()) * 8 + m_pending_bits;
	}

	void appendBits(unsigned int value, unsigned int nb_bits);

	std::string toString() const;

private:
	unsigned char m_pending_data = 0;
	unsigned int m_pending_bits = 0;
	std::vector<unsigned char> m_data;
};

// Allows to de-serialize (extract) data from a flow a few bits at a time.
class InputBitStream : public BitStream {
public:
	InputBitStream(std::string stream);

	unsigned int remainingBits() const {
		return m_size_in_bits - m_current_bit;
	}

	bool isEmpty() const;

	bool readSymbolCode(SYMBOL_NAME_CODES::ENUM & code);
	
	SYMBOL_NAME_CODES::ENUM readSymbolCode() {
		SYMBOL_NAME_CODES::ENUM e;
		if (!readSymbolCode(e))
			throw make_logic_error("can't read symbol code");
		return e;
	}

	unsigned int readBits(unsigned int nb_bits) {
		unsigned int value;
		if (!readBits(nb_bits, value))
			throw make_logic_error("can't read bits");
		return value;
	}

	bool readBits(unsigned int nb_bits, unsigned int & result);

private:
	unsigned int m_size_in_bits = 0;
	unsigned int m_current_bit = 0;
	std::vector<unsigned char> m_data;
};

void test_OutputBitStream();
void test_InputBitStream();
