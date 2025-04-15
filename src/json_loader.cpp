#include "json_loader.h"

//#include <boost/json/src.hpp>

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    std::ifstream in_file;
    in_file.open(json_path);
    if (!in_file.is_open()) {
        throw std::runtime_error("Failed to open file");
    }

    std::stringstream buffer;
    buffer << in_file.rdbuf();
    std::string json_string = buffer.str();
    
    // Распарсить строку как JSON, используя boost::json::parse
    json::value value = json::parse(json_string);

    // Загрузить модель игры из файла
    model::Game game;

    ExtraData extra_data;

    if (value.as_object().at("maps").as_array().empty()) {
        throw std::invalid_argument("Empty maps array in JSON");
    }

    double default_dog_speed = 1;

    if (value.as_object().count("defaultDogSpeed")) {
        default_dog_speed = value.as_object().at("defaultDogSpeed").as_double();
    }

    int default_bag_capacity = 3;

    if (value.as_object().count("defaultBagCapacity")) {
        default_bag_capacity = value.as_object().at("defaultBagCapacity").as_int64();
    }

    if (value.as_object().at("lootGeneratorConfig").as_object().count("period") &&
        value.as_object().at("lootGeneratorConfig").as_object().count("probability")) {

        int loot_period = static_cast<int>(value.as_object().at("lootGeneratorConfig").as_object().at("period").as_double());
        double loot_probality = value.as_object().at("lootGeneratorConfig").as_object().at("probability").as_double();

        std::shared_ptr<loot_gen::LootGenerator> loot_generator = std::make_shared<loot_gen::LootGenerator>(std::chrono::seconds{ loot_period }, loot_probality);
        game.SetLootGenerator(loot_generator);

        extra_data.SetLootInfo(loot_period, loot_probality);
    }
    else {
        throw std::invalid_argument("Empty loot info in conf JSON");
    }

    double dog_retirement_time = 60;
    if (value.as_object().count("dogRetirementTime")) {
        dog_retirement_time = value.as_object().at("dogRetirementTime").as_double();
    }
    game.SetDogRetirementTime(dog_retirement_time);

    for (json::value& map_info : value.as_object().at("maps").as_array()) {
        AddMap(game, extra_data, map_info, default_dog_speed, default_bag_capacity);
    }
    game.SetExtraData(std::make_shared<ExtraData>(extra_data));

    return game;
}


void AddMap(model::Game& game, ExtraData& extra_data, json::value& map_info, double default_dog_speed, int default_bag_capacity) {
    util::Tagged<std::string, model::Map> id(map_info.as_object().at("id").as_string().c_str());
    std::string name(map_info.as_object().at("name").as_string());

    double dog_speed = default_dog_speed;
    if (map_info.as_object().count("dogSpeed")) {
        dog_speed = map_info.as_object().at("dogSpeed").as_double();
    }

    int bag_capacity = default_bag_capacity;
    if (map_info.as_object().count("bagCapacity")) {
        bag_capacity = map_info.as_object().at("bagCapacity").as_int64();
    }

    model::Map map(id, name, dog_speed, bag_capacity);

    if (map_info.as_object().at("roads").as_array().empty()) {
        throw std::invalid_argument("Empty roads array at map");
    }
    if (map_info.as_object().at("lootTypes").as_array().empty()) {
        throw std::invalid_argument("Empty lootTypes array at map");
    }
    for (json::value& road : map_info.as_object().at("roads").as_array()) {
        AddRoad(map, road);
    }
    for (json::value& build : map_info.as_object().at("buildings").as_array()) {
        AddBuild(map, build);
    }
    for (json::value& office : map_info.as_object().at("offices").as_array()) {
        AddOffice(map, office);
    }
    extra_data.InsertMapInfo(map_info.as_object().at("lootTypes").as_array());
    game.AddMap(map);
}

void AddRoad(model::Map& map, const json::value& road_map) {
    if (road_map.as_object().find("x1") != road_map.as_object().end()) {
        model::Point start{ road_map.as_object().at("x0").as_int64(), road_map.as_object().at("y0").as_int64() };
        model::Coord end_x = road_map.as_object().at("x1").as_int64();

        model::Road road(model::Road::HORIZONTAL, start, end_x);
        map.AddRoad(road);
    }
    else if (road_map.as_object().find("y1") != road_map.as_object().end()) {
        model::Point start{ road_map.as_object().at("x0").as_int64(), road_map.as_object().at("y0").as_int64() };
        model::Coord end_y = road_map.as_object().at("y1").as_int64();

        model::Road road(model::Road::VERTICAL, start, end_y);
        map.AddRoad(road);
    }
    else {
        throw std::invalid_argument("Non-format of road in JSON");
    }
}

void AddBuild(model::Map& map, const json::value& build_map) {
    model::Point point{ build_map.as_object().at("x").as_int64(), build_map.as_object().at("y").as_int64() };
    model::Size size{ build_map.as_object().at("w").as_int64(), build_map.as_object().at("h").as_int64() };
    model::Rectangle bounds{ point, size };
    model::Building build(bounds);

    map.AddBuilding(build);
}

void AddOffice(model::Map& map, const json::value& office_map) {
    model::Office::Id id(office_map.as_object().at("id").as_string().c_str());
    model::Point position{ office_map.as_object().at("x").as_int64(), office_map.as_object().at("y").as_int64() };
    model::Offset offset{ office_map.as_object().at("offsetX").as_int64(), office_map.as_object().at("offsetY").as_int64() };
    model::Office office(id, position, offset);
    
    map.AddOffice(office);
}

}  // namespace json_loader
