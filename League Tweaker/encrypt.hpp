#pragma once

#include <string>

namespace tweaker
{
    std::string encode_base64(std::string const& data)
    {
        auto length = data.length();
        std::string source = data;

        constexpr char const* key = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string encode(length * 4 / 3 + 4, '\0');
        auto loop = length % 3 == 0 ? length : length - 3;
        int i{}, n{};
        for (; i < loop; i += 3)
        {
            encode[n] = key[source[i] >> 2];
            encode[n + 1ull] = key[((source[i] & 3) << 4) | ((source[i + 1ull] & 0xF0) >> 4)];
            encode[n + 2ull] = key[((source[i + 1ull] & 0x0f) << 2) | ((source[i + 2ull] & 0xc0) >> 6)];
            encode[n + 3ull] = key[source[i + 2ull] & 0x3F];
            n += 4;
        }

        switch (length % 3)
        {
            case 0:
                encode[n] = '\0';
                break;

            case 1:
                encode[n] = key[source[i] >> 2];
                encode[n + 1ull] = key[((source[i] & 3) << 4) | ((0 & 0xf0) >> 4)];
                encode[n + 2ull] = '=';
                encode[n + 3ull] = '=';
                encode[n + 4ull] = '\0';
                break;

            case 2:
                encode[n] = key[source[i] >> 2];
                encode[n + 1ull] = key[((source[i] & 3) << 4) | ((source[i + 1ull] & 0xf0) >> 4)];
                encode[n + 2ull] = key[((source[i + 1ull] & 0xf) << 2) | ((0 & 0xc0) >> 6)];
                encode[n + 3ull] = '=';
                encode[n + 4ull] = '\0';
                break;
        }

        return encode.c_str();
    }
}