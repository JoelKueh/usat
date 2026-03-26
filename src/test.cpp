
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <filesystem>

#include "cnf.h"

using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

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
	for (const auto& dirent : recursive_directory_iterator(dirpath))
    	test_single(dirent.path(), sat);
}

TEST_CASE("cnf uf20-91-1") {
	const std::vector<cnf::var_state> *sol;
	cnf::var_state s;
	cnf::var_t v;
	cnf cnfsat;

	REQUIRE(cnfsat.init("./test/uf20-91/uf20-01.cnf") == 0);
	REQUIRE(cnfsat.get_max_var() == 20);
	REQUIRE(cnfsat.get_clause_cnt() == 91);

	CHECK((void*)cnfsat.solve());
}

TEST_CASE("cnf uuf75-325") {
	const std::vector<cnf::var_state> *sol;
	cnf::var_state s;
	cnf::var_t v;
	cnf cnfsat;

	REQUIRE(cnfsat.init("./test/uuf75-325/uuf75-01.cnf") == 0);
	REQUIRE(cnfsat.get_max_var() == 75);
	REQUIRE(cnfsat.get_clause_cnt() == 325);

	sol = cnfsat.solve();
	if (sol != nullptr) {
		for (v = 0; v < sol->size(); v++) {
			std::cout << v << "\t" << (uint32_t)s.raw << std::endl;
		}
	}

	CHECK_FALSE((void*)sol);
}

TEST_CASE("cnf uf20-91") {
	test_dir("./test/uf20-91/", true);
}

TEST_CASE("cnf uuf75-325") {
	// test_dir("./test/uuf75-325/", false);
}
