
#include <future>
#include <chrono>
#include <iostream>

#include "cnf.h"

using namespace std::chrono;

const duration<double, std::milli> TIMEOUT = std::chrono::seconds(1000000);

int main(int argc, char *argv[])
{
	const std::vector<cnf::var_state> *sol;
	cnf::var_state s;
	cnf::var_t v;
	cnf cnfsat;
	bool cancel = false;
	bool result;

	if (argc != 2) {
		std::cerr << "Invalid usage";
		return 1;
	}
	cnfsat.init(argv[1]);

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

	result = future.get() != nullptr;

	return 0;
}
