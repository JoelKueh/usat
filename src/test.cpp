
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <filesystem>
#include <future>
#include <chrono>
#include <iostream>
#include <iomanip>

#include "cnf.h"


using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
using namespace std::chrono;

const duration<double, std::milli> TIMEOUT = std::chrono::seconds(1000000);

void pbar(std::string title, int cur, int total, duration<double, std::milli>last_time)
{
	int char_cnt = std::to_string(total).length();
	std::cout << "\r\033[2K" << std::flush;
	std::cout << title << " - " << std::setw(char_cnt) << cur << "/"
		<< std::setw(char_cnt) << total << " [";
	for (int i = 0; i < 50; i++) {
		if ((double)cur / (double)total * 50.0 > i)
			std::cout << "#";
		else
			std::cout << " ";
	}
	std::cout << "] " << last_time.count() << "ms" << std::flush;
}

void test_single(std::string path, bool sat)
{
	const std::vector<cnf::var_state> *sol;
	cnf::var_state s;
	cnf::var_t v;
	cnf cnfsat;
	bool cancel = false;
	bool result;

	REQUIRE(cnfsat.init(path) == 0);

	// launch the job asynchronously
	std::future<void*> future = std::async(
		std::launch::async,
	    [cnfsat, cancel]() mutable { return (void*)cnfsat.solve(&cancel); });
	std::future_status status = future.wait_for(TIMEOUT);

	// if the job is not done, then cancel it
	if (status != std::future_status::ready) {
		cancel = true;
		if (future.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
			std::cout << "Solver Cancellation Failed: Terminating" << std::endl;
			exit(1);
		}
	}

	REQUIRE(status == std::future_status::ready);
	result = future.get() != nullptr;
	CHECK(result == sat);
}

void test_dir(std::string dirpath, bool sat)
{
	duration<double, std::milli> total;
	int i = 0, count = 0;

	for (const auto& dirent : recursive_directory_iterator(dirpath))
		count += 1;

	for (const auto& dirent : recursive_directory_iterator(dirpath)) {
		auto start = high_resolution_clock::now();
 	   	test_single(dirent.path(), sat);
 	   	auto end = high_resolution_clock::now();
		duration<double, std::milli> elapsed = end - start;

		total += elapsed;
		i += 1;
		pbar(dirpath, i, count, elapsed);
	}

	std::cout << "\r\033[2K" << std::flush;
	std::cout << dirpath << " - Average Solve Time: " << total.count() / i << "ms" << std::endl;
}

TEST_CASE("cnf uf20-91") {
	test_dir("./test/uf20-91/", true);
}

TEST_CASE("cnf uf75-325") {
	test_dir("./test/uf75-325/", true);
}

TEST_CASE("cnf uuf75-325") {
	test_dir("./test/uuf75-325/", false);
}

TEST_CASE("cnf uf250-1065") {
	test_dir("./test/uf250-1065/", true);
}

TEST_CASE("cnf uuf75-325") {
	test_dir("./test/uuf250-1065/", false);
}
