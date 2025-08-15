#include "config.h"

#include <yaml-cpp/yaml.h>
#include <iostream>

bool Config::Load(const std::string & path)
{
    try
    {
        YAML::Node node = YAML::LoadFile(path);
        if (node["hint_delay_seconds"])
        {
            hint_delay_seconds = node["hint_delay_seconds"].as<float>();
        }
        return true;
    }
    catch (const std::exception & e)
    {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        return false;
    }
}
