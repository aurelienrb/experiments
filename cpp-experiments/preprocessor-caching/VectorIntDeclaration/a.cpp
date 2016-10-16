#include <vector>
#include "a.h"
#include <iostream>

namespace std {
	template class vector<int>;
}

void check_size(size_t s) {
	if (s != sizeof std::vector<int>) {
		throw "oops!";
	}
}

std::vector<int> f(std::vector<int> v) {
	std::cout << v.size() << "\n";
	for (int i : v) {
		std::cout << i << " ";
	}
	std::cout << "\n";
	v.emplace_back(10);
	v.push_back(20);
	return v;
}