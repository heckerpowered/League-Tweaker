#pragma once

#include <iostream>
#include <boost/process.hpp>
#include <boost/process/windows.hpp>
#include <regex>

namespace tweaker
{
	struct credential
	{
		std::string port;
		std::string pid;
		std::string auth_token;
	};

	inline std::optional<credential> try_initialize() noexcept
	{	
		boost::process::ipstream stream;
		std::error_code error;
		int const code = boost::process::system("wmic PROCESS WHERE name='LeagueClientUx.exe' GET commandline /value",
			boost::process::std_out > stream, error, boost::process::windows::create_no_window);
		if (error)
		{
			std::cerr << "Failed to create process: " << error.message() << std::endl;
			return {};
		}
		else
		{
			std::string buffer;
			buffer.resize(2048);
			stream.read(buffer.data(), buffer.size());

			std::smatch app_port;
			std::smatch app_pid;
			std::smatch remoting_auth_token;
			if (std::regex_search(buffer, app_port, std::regex("--app-port=([0-9]*)"))
				&& std::regex_search(buffer, app_pid, std::regex("--app-pid=([0-9]*)"))
				&& std::regex_search(buffer, remoting_auth_token, std::regex("\"--remoting-auth-token=(.*?)\"")))
			{
				credential credential{ .port = app_port[1], .pid = app_pid[1], .auth_token = remoting_auth_token[1] };

				std::cout << "Found app port: " << credential.port << std::endl
					<< "Found app pid: " << credential.pid << std::endl
					<< "Found auth token: " << credential.auth_token << std::endl;

				return std::move(credential);
			}
			else
			{
				return {};
			}
		}
	}

	inline credential initialize() noexcept
	{
		std::optional<tweaker::credential> credential = tweaker::try_initialize();
		while (!credential)
		{
			credential = tweaker::try_initialize();
		}

		return credential.value();
	}

	inline void launch(boost::system::error_code& code)
	{
		std::cout << "Initializing League Tweaker" << std::endl;
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
			return;
		}
	}
}