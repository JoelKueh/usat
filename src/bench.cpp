
#include <filesystem>
#include <chrono>
#include <iostream>

#include "cnf.h"

using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
using namespace std::chrono;

void test_single(std::string path, bool sat)
{
	const std::vector<cnf::var_state> *sol;
	cnf::var_state s;
	cnf::var_t v;
	cnf cnfsat;
	bool result;

	REQUIRE(cnfsat.init(path) == 0);
	result = cnfsat.solve() != nullptr;
	CHECK(result == sat);
}

void test_dir(std::string dirpath, bool sat)
{
	auto start = high_resolution_clock::now();
	int i = 0, count = 0;

	for (const auto& dirent : recursive_directory_iterator(dirpath))
		count += 1;

	for (const auto& dirent : recursive_directory_iterator(dirpath)) {
    	test_single(dirent.path(), sat);
		i += 1;
	}

	auto end = high_resolution_clock::now();
	duration<double, std::milli> elapsed = end - start;

	std::cout << dirpath << " - Average Solve Time: " << elapsed.count() / i << "ms" << std::endl;
}

int main() {
	
}
