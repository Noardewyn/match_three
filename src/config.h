#pragma once
#include <string>

struct Config
{
    float hint_delay_seconds {5.0f};

    bool Load(const std::string & path);
};
