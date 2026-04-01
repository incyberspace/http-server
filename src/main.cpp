#include <iostream>
#include <spdlog/spdlog.h>

#include "utils.hpp"
#include "config.hpp"

int main()
{
	const std::string addr = config_lib::config.get("LISTEN");
	const bool run = config_lib::config.get("RUN");
	std::cout << addr << std::endl << run << std::endl;
}