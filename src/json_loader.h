#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <boost/json.hpp>
#include <memory>

#include "model.h"
#include "extra_data.h"
#include "loot_generator.h"

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

model::Game LoadGame(const std::filesystem::path& json_path);


void AddMap(model::Game& game, ExtraData& extra_data, json::value& map_info, double default_dog_speed, int default_bag_capacity);
void AddRoad(model::Map& map, const json::value& road_map);
void AddBuild(model::Map& map, const json::value& build_map);
void AddOffice(model::Map& map, const json::value& office_map);
}  // namespace json_loader
