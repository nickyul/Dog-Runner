#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "../src/model.h"
#include "../src/model_serialization.h"

using namespace model;
using namespace serialization;
using namespace std::literals;

namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
        const auto dog = [] {
            Dog dog{ "Pluto"s, {42.2, 12.5}};
            dog.SetDirect(model::Direct::EAST);
            dog.SetSpeed({2.3, -1.2});
            return dog;
        }();

        WHEN("dog is serialized") {
            {
                serialization::DogRepr repr{dog};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();
                
                CHECK(dog.GetName() == restored.GetName());
                CHECK(dog.GetPosition() == restored.GetPosition());
                CHECK(dog.GetVelocity() == restored.GetVelocity());;
                CHECK(dog.GetDirect() == restored.GetDirect());
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Loot Serialization") {
    GIVEN("a loot") {
        const auto loot = [] {
            Loot loot{ 1,{1.,2.} };
            return loot;
        }();

        WHEN("loot is serialized") {
            {
                serialization::LootRepr repr{ loot };
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{ strm };
                serialization::LootRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(loot.GetLootType() == restored.GetLootType());
                CHECK(loot.GetPosition() == restored.GetPosition());
            }
        }
    }
}
