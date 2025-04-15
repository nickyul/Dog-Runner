#include <catch2/catch_test_macros.hpp>

#include "../src/model.h"

using namespace std::literals;

SCENARIO("Game model") {

	GIVEN("game class") {

		model::Game game;
		std::shared_ptr<ExtraData> extra_data = std::make_shared<ExtraData>();

		game.SetExtraData(extra_data);

		WHEN("in the game where no maps") {
			THEN("here no maps and no players") {
				CHECK(game.GetMaps().size() == 0);
				CHECK(game.GetPlayers().size() == 0);
			}
			AND_WHEN("map added") {
				const model::Map::Id first_map_id("testmap");
				game.AddMap(model::Map(first_map_id, "Test map", 1, 3));

				boost::json::array first_map_info;
				first_map_info.emplace_back("loot1");
				first_map_info.emplace_back("loot2");
				extra_data->InsertMapInfo(first_map_info);
				
				THEN("game have 1 map and loot_id between 0 and 1") {
					CHECK(game.GetMaps().size() == 1);
					CHECK(game.GetMaps().at(0).GetId() == first_map_id);
					CHECK(extra_data->GetLootCount(game.GetMaps().size() - 1) == 2);

					for (int i = 0; i < 10; ++i) {
						int loot_id = game.GetRandomLootType(first_map_id);
						CHECK(((loot_id >= 0) && (loot_id <= 1)));
					}
				}

				AND_WHEN("another map added") {
					const model::Map::Id second_map_id("testmap2");
					game.AddMap(model::Map(second_map_id, "Test map2", 1, 3));

					boost::json::array second_map_info;
					second_map_info.emplace_back("loot1");
					second_map_info.emplace_back("loot2");
					second_map_info.emplace_back("loot3");
					second_map_info.emplace_back("loot4");
					extra_data->InsertMapInfo(second_map_info);

					THEN("game have 2 maps and loot_id between 0 and 3") {
						CHECK(game.GetMaps().size() == 2);
						CHECK(game.GetMaps().at(1).GetId() == second_map_id);
						CHECK(extra_data->GetLootCount(game.GetMaps().size() - 1) == 4);

						for (int i = 0; i < 10; ++i) {
							const int loot_id = game.GetRandomLootType(second_map_id);
							CHECK(((loot_id >= 0) && (loot_id <= 3)));
						}
					}

				}
			}
		}
	}

	GIVEN("game session") {
		model::Map::Id map_id("testmap");
		model::Map map(map_id, "Test map", 1, 3);
		model::Road road(model::Road::HORIZONTAL, { 0,0 }, 10);
		map.AddRoad(road);

		model::GameSession game_session(&map);

		WHEN("adding loot") {
			game_session.AddLoot(1);
			THEN("session contain 1 loot")
				CHECK(game_session.GetLootCount() == 1);
		}
	}
	
}