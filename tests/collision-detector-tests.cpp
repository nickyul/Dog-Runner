#define _USE_MATH_DEFINES

#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/collision_detector.h"

// Тесты для функции collision_detector::FindGatherEvents

using Catch::Matchers::WithinRel;
using namespace collision_detector;

SCENARIO("Collision detection") {
	GIVEN("class ItemGatherer") {

		WHEN("gatherer moving x-axis") {
			ItemGatherer item_gatherer{};
			const Item item_x = { {10.0, 0.}, 0.6 };
			const Gatherer gatherer_x_moving = { {0.,0.}, {20.,0.}, 0.6 };
			item_gatherer.AddItem(item_x);
			item_gatherer.AddGatherer(gatherer_x_moving);

			THEN("founds 1 item") {
				auto result = FindGatherEvents(item_gatherer);
				CHECK(result.size() == 1);
				CHECK(result[0].item_id == 0);
				CHECK(result[0].gatherer_id == 0);
				CHECK_THAT(result[0].sq_distance, WithinRel(0., 1e-10));
				CHECK_THAT(result[0].time, WithinRel((item_x.position.x / gatherer_x_moving.end_pos.x), 1e-10));
			}
		}

		WHEN("gatherer moving y-axis") {
			ItemGatherer item_gatherer{};
			const Item item_y = { {0.0, 10.}, 0.6 };
			const Gatherer gatherer_y_moving = { {0.,0.}, {0.,20.}, 0.6 };
			item_gatherer.AddItem(item_y);
			item_gatherer.AddGatherer(gatherer_y_moving);
			THEN("founds 1 item") {
				auto result = FindGatherEvents(item_gatherer);
				CHECK(result.size() == 1);
				CHECK(result[0].item_id == 0);
				CHECK(result[0].gatherer_id == 0);
				CHECK_THAT(result[0].sq_distance, WithinRel(0., 1e-10));
				CHECK_THAT(result[0].time, WithinRel((item_y.position.y / gatherer_y_moving.end_pos.y), 1e-10));
			}
		}

		WHEN("gatherer moving x-axis") {
			ItemGatherer item_gatherer{};
			const Item item_x_1 = { {10.0, 0.}, 0.6 };
			const Item item_x_2 = { {20.0, 0.}, 0.6 };
			const Gatherer gatherer_x_moving = { {0.,0.}, {30.,0.}, 0.6 };
			item_gatherer.AddItem(item_x_1);
			item_gatherer.AddItem(item_x_2);
			item_gatherer.AddGatherer(gatherer_x_moving);

			THEN("founds 2 item") {
				auto result = FindGatherEvents(item_gatherer);
				CHECK(result.size() == 2);
				CHECK(result[0].item_id == 0);
				CHECK(result[1].item_id == 1);
				CHECK(result[0].gatherer_id == 0);
				CHECK(result[1].gatherer_id == 0);
			}
		}

		WHEN("gatherer moving y-axis") {
			ItemGatherer item_gatherer{};
			const Item item_y_1 = { {0.0, 10.}, 0.6 };
			const Item item_y_2 = { {0.0, 20.}, 0.6 };
			const Gatherer gatherer_y_moving = { {0.,0.}, {0.,30.}, 0.6 };
			item_gatherer.AddItem(item_y_1);
			item_gatherer.AddItem(item_y_2);
			item_gatherer.AddGatherer(gatherer_y_moving);

			THEN("founds 2 item") {
				auto result = FindGatherEvents(item_gatherer);
				CHECK(result.size() == 2);
				CHECK(result[0].item_id == 0);
				CHECK(result[1].item_id == 1);
				CHECK(result[0].gatherer_id == 0);
				CHECK(result[1].gatherer_id == 0);
			}
		}
	}
}