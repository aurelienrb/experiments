//#include <vector>
#include "vector.int.h"
#include "a.h"
using namespace std;

int main() {
	vector<int> v;
	check_size(sizeof(v));
	v.emplace_back(10);
	auto v2 = f(v);
	return (int) v2.size();
}
