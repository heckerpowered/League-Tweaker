#define _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS

#include <sdkddkver.h>

#include <iostream>

#include "util.hpp"
#include "bootstrap.hpp"
#include "encrypt.hpp"
#include "network_manager.hpp"

using namespace std::chrono_literals;

int main(int argc, char** argv)
{
	std::cout.sync_with_stdio(false);
	std::cerr.sync_with_stdio(false);
	std::cin.sync_with_stdio(false);

	std::cout << "Welcome to League Tweaker, version " << tweaker::version << std::endl;
	std::cout << "Running args: [";
	for (int i{}; i < argc; i++)
	{
		if (i != 0)
		{
			std::cout << ',';
		}

		std::cout << argv[i];
	}
	std::cout << ']' << std::endl;

	std::cout << "Set console output code page to UTF-8" << std::endl;
	SetConsoleOutputCP(CP_UTF8);

	std::cout << "Connecting to League Client." << std::endl;

	boost::system::error_code code;
	tweaker::launch(code);

	std::optional<tweaker::credential> credential = tweaker::try_initialize();
	while (!credential)
	{
		credential = tweaker::try_initialize();
	}

	

	std::cout << "Initializing network manager" << std::endl;
	tweaker::initialize_network_manager(credential.value(), code);
	if (code)
	{
		std::cerr << "Failed to initializing network manager: " << code.message() << std::endl;
		system("pause");
		return 0;
	}

	std::string summoner_name = tweaker::get_summoner_name(code);
	if (code)
	{
		std::cerr << "Failed to get summoner name: " << code.message() << std::endl;
		system("pause");
		return 0;
	}

	std::cout << "Summoner name: " << summoner_name << std::endl;

	int64_t champion_id{}, ban_champion_id{};

	std::cout << "Input champion id: ";
	std::cin >> champion_id;
	
	std::cout << "Input ban champion id: ";
	std::cin >> ban_champion_id;
	while (true)
	{
		std::cout << "Waiting for match" << std::endl;

		int64_t action{}, actor_cell_id{};

		while (true)
		{
			actor_cell_id = tweaker::get_actor_cell_id(code);
			if (code)
			{
				code = {};
				continue;
			}

			std::cout << "Actor cell id: " << actor_cell_id << std::endl;
			std::cout << "Wait for pick" << std::endl;
			while ((action = tweaker::wait_for_pick(code)) == 0)
			{
				if (action = tweaker::wait_for_ban(code) != 0)
				{
					tweaker::banpick_champion(actor_cell_id, action, ban_champion_id, code);
					action = 0;
				}

				if (code)
				{
					code = {};
				}
			}
			break;
		}

		tweaker::banpick_champion(actor_cell_id, action, champion_id, code);
		if (code)
		{
			std::cerr << "Failed to pick champion: " << code.message() << std::endl;
			system("pause");
			return 0;
		}
	}
	return 0;
}
