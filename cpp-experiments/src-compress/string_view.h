#pragma once

#include <string>
#include <cassert>

class string_view {
public:
	string_view(const char * str) : m_begin(str), m_length(std::strlen(str)) {
	}

	string_view(const string_view & str, int pos, int nb_char) : m_begin(str.m_begin + pos), m_length(nb_char) {
		assert(pos >= 0 && nb_char >= 1);
		assert(pos + nb_char <= str.length());
	}

	int length() const {
		return m_length;
	}

	bool empty() const {
		return m_length == 0;
	}

	void clear() {
		m_length = 0;
	}

	const char * begin() const {
		assert(m_length > 0);
		return m_begin;
	}

	const char * end() const {
		return begin() + m_length;
	}

	char operator[](int pos) const {
		assert(pos < m_length);
		return m_begin[pos];
	}

	void remove_prefix(int n) {
		assert(n <= m_length);
		m_begin += n;
		m_length -= n;
	}

	std::string to_string() const {
		return std::string(begin(), end());
	}

private:
	const char * m_begin;
	int m_length;
};

//bool operator==(const string_view & lhs, const string_view & rhs);
