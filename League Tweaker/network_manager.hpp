#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <regex>

#include "bootstrap.hpp"
#include "encrypt.hpp"

namespace tweaker
{
	std::string authorization;
	std::string host;

	boost::asio::io_context io_service;
	boost::asio::ssl::context context{ boost::asio::ssl::context::method::tlsv12_client };
	boost::beast::ssl_stream<boost::beast::tcp_stream> stream{ io_service,context };

	inline void initialize_network_manager(credential const& credential, boost::beast::error_code& code) noexcept
	{
		std::stringstream authorization_stream;
		authorization_stream << "riot:" << credential.auth_token;

		std::string const authorization = tweaker::encode_base64(authorization_stream.str());
		std::cout << "Encode: " << authorization_stream.str() << " -> " << authorization << std::endl;

		authorization_stream.clear();
		authorization_stream.str("");
		authorization_stream << "Basic " << authorization;
		std::cout << "Authorization: " << authorization_stream.str() << std::endl;
		tweaker::authorization = authorization_stream.str();

		std::stringstream host_stream;
		host_stream << "127.0.0.1:" << credential.port;
		std::cout << "Host: " << host_stream.str() << std::endl;
		tweaker::host = host_stream.str();

		std::cout << "Set verfy mode" << std::endl;
		context.set_verify_mode(boost::asio::ssl::verify_none, code);
		if (code)
		{
			std::cerr << "Failed to set verify mode: " << code.message() << std::endl;
			return;
		}

		boost::asio::ip::tcp::resolver resolver{ io_service };

		if (!SSL_set_tlsext_host_name(stream.native_handle(), "127.0.0.1"))
		{
			code = { static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
			std::cerr << "Failed to set sni host name: " << code.message() << std::endl;
			return;
		}

		auto const results = resolver.resolve("127.0.0.1", credential.port);
		for (auto&& domain : results)
		{
			std::cout << "Found domain host: " << domain.host_name() << " service: " << domain.service_name() << std::endl;
		}

		std::cout << "Establishing connection" << std::endl;
		boost::beast::get_lowest_layer(stream).connect(results, code);
		if (code)
		{
			std::cerr << "Failed to Establish connection: " << code.message() << std::endl;
			return;
		}

		std::cout << "Perform the SSL handshake" << std::endl;
		stream.handshake(boost::asio::ssl::stream_base::client, code);
		if (code)
		{
			std::cerr << "Failed to perform SSL handshake: " << code.message() << std::endl;
			return;
		}
	}

	inline boost::beast::http::request<boost::beast::http::string_body> request(boost::beast::http::verb const method, std::string const& path) noexcept
	{
		boost::beast::http::request<boost::beast::http::string_body> request{ method, path ,11 };
		request.set(boost::beast::http::field::host, host);
		request.set(boost::beast::http::field::authorization, authorization);
		request.set(boost::beast::http::field::accept, "*/*");
		return std::move(request);
	}

	inline void send_request(boost::beast::http::request<boost::beast::http::string_body> const& request, boost::system::error_code& code) noexcept
	{
		boost::beast::http::write(stream, request, code);
	}

	inline void send(boost::beast::http::verb const method, std::string const& path, boost::system::error_code& code) noexcept
	{
		send_request(request(method, path), code);
	}

	inline boost::beast::http::response<boost::beast::http::dynamic_body> get_response(boost::system::error_code& code) noexcept
	{
		boost::beast::flat_buffer buffer;
		boost::beast::http::response<boost::beast::http::dynamic_body> response;
		boost::beast::http::read(stream, buffer, response, code);
		return response;
	}

	inline std::string get_summoner_name(boost::system::error_code& code) noexcept
	{
		send(boost::beast::http::verb::get, "/lol-summoner/v1/current-summoner", code);
		if (code)
		{
			return {};
		}

		boost::beast::http::response<boost::beast::http::dynamic_body> response = get_response(code);
		std::stringstream response_stream;
		response_stream << boost::beast::make_printable(response.body().data());
		boost::json::value display_name_value = boost::json::parse(response_stream.str(), code).as_object()["displayName"];
		if (code)
		{
			std::cerr << "Failed to parse json: " << code.message() << std::endl;
			return {};
		}

		return std::move(static_cast<std::string>(display_name_value.as_string().operator std::basic_string_view<char, std::char_traits<char>>()));
	}

	inline std::string get_response_body(boost::beast::http::response<boost::beast::http::dynamic_body> const& response) noexcept
	{
		std::stringstream response_stream;
		response_stream << boost::beast::make_printable(response.body().data());
		return response_stream.str();
	}

	inline std::string&& to_string(boost::json::string string) noexcept
	{
		return std::move(static_cast<std::string>(string.operator std::basic_string_view<char, std::char_traits<char>>()));
	}

	inline int64_t get_actor_cell_id(boost::system::error_code& code) noexcept
	{
		tweaker::send(boost::beast::http::verb::get, "/lol-champ-select/v1/session", code);
		if (code)
		{
			std::cerr << "Failed to send request: " << code.message() << std::endl;
			return 0;
		}

		auto&& response = tweaker::get_response(code);
		if (response.result_int() != 200)
		{
			code = boost::system::error_code{ static_cast<int>(response.result_int()), boost::system::generic_category() };
			return 0;
		}

		boost::json::value value = boost::json::parse(tweaker::get_response_body(response), code);
		if (code)
		{
			std::cerr << "Failed to parse actor cell id: " << code.message() << std::endl;
			return 0;
		}

		return value.as_object()["localPlayerCellId"].as_int64();
	}

	inline int64_t wait_for_ban(boost::system::error_code& code) noexcept
	{
		tweaker::send(boost::beast::http::verb::get, "/lol-champ-select/v1/session", code);
		if (code)
		{
			std::cerr << "Failed to send request: " << code.message() << std::endl;
			return 0;
		}

		auto&& response = tweaker::get_response(code);
		if (response.result_int() != 200)
		{
			code = boost::system::error_code{ static_cast<int>(response.result_int()), boost::system::generic_category() };
			return 0;
		}

		boost::json::value value = boost::json::parse(tweaker::get_response_body(response), code);
		if (code)
		{
			std::cerr << "Failed to parse actor cell id: " << code.message() << std::endl;
			return 0;
		}

		auto&& root = value.as_object();
		int64_t const local_player_cell_id = root["localPlayerCellId"].as_int64();
		for (auto&& action : root["actions"].as_array())
		{
			for (auto&& element : action.as_array())
			{
				auto&& action_element = element.as_object();
				if (action_element["actorCellId"].as_int64() == local_player_cell_id && !action_element["completed"].as_bool()
					&& action_element["isInProgress"].as_bool() && action_element["type"].as_string() == "ban")
				{
					return action_element["id"].as_int64();
				}
			}
		}

		return 0;
	}

	inline int64_t wait_for_pick(boost::system::error_code& code) noexcept
	{
		tweaker::send(boost::beast::http::verb::get, "/lol-champ-select/v1/session", code);
		if (code)
		{
			std::cerr << "Failed to send request: " << code.message() << std::endl;
			return 0;
		}

		auto&& response = tweaker::get_response(code);
		if (response.result_int() != 200)
		{
			code = boost::system::error_code{ static_cast<int>(response.result_int()), boost::system::generic_category() };
			return 0;
		}

		boost::json::value value = boost::json::parse(tweaker::get_response_body(response), code);
		if (code)
		{
			std::cerr << "Failed to parse actor cell id: " << code.message() << std::endl;
			return 0;
		}

		auto&& root = value.as_object();
		int64_t const local_player_cell_id = root["localPlayerCellId"].as_int64();
		for (auto&& action : root["actions"].as_array())
		{
			for (auto&& element : action.as_array())
			{
				auto&& action_element = element.as_object();
				if (action_element["actorCellId"].as_int64() == local_player_cell_id && !action_element["completed"].as_bool()
					&& action_element["isInProgress"].as_bool() && action_element["type"].as_string() == "pick")
				{
					return action_element["id"].as_int64();
				}
			}
		}

		return 0;
	}

	inline void banpick_champion(int64_t actor_cell_id, int64_t action, int64_t champion_id, boost::system::error_code& code) noexcept
	{
		std::stringstream path_stream;
		path_stream << "/lol-champ-select/v1/session/actions/" << action;
		auto&& request = tweaker::request(boost::beast::http::verb::patch, path_stream.str());
		request.body() = boost::json::serialize(boost::json::object{ {"actorCellId",actor_cell_id},{"championId", champion_id},{"completed",true},{"id", action}, {"isAllyAction",true},{"type","pick"} });
		request.content_length(request.body().size());
		tweaker::send_request(request, code);
		if (code)
		{
			std::cerr << "Failed to pick champion: " << code.message() << std::endl;
			return;
		}

		auto&& response = tweaker::get_response(code);
		if (response.result_int() != 204)
		{
			std::cerr << "Failed to pick champion" << std::endl
				<< "Response: " << response << std::endl;
		}
	}
}