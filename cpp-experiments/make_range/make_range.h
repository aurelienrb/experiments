#pragma once

template<typename Iterator>
class range {
public:
	constexpr range() = default;

	constexpr range(const Iterator & begin, const Iterator & end) : m_begin(begin), m_end(end) {
	}

	constexpr bool empty() const {
		return m_begin == m_end;
	}

	constexpr Iterator begin() const {
		return m_begin;
	}

	constexpr size_t size() const {
		return empty() ? 0 : end() - begin();
	}

	constexpr Iterator end() const {
		return m_end;
	}
	void clear() {
		m_begin = Iterator{};
		m_end = Iterator{};
	}

	size_t length() const {
		return std::distance(m_begin, m_end);
	}

private:
	Iterator m_begin;
	Iterator m_end;
};

template<typename T>
range<T> make_range(T begin, T end) {
	return range<T>(begin, end);
}

template<typename T>
range<typename T::iterator> make_range(const T & container) {
	return range<typename T::iterator>(container.begin(), container.end());
}
