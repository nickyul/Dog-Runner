#pragma once

#include <boost/json.hpp>

#include <vector>
#include <string>
#include <unordered_map>

#include "tagged.h"

namespace json = boost::json;

class ExtraData {
public:
	ExtraData() = default;
	ExtraData(int loot_period, double loot_probality);

	void SetLootInfo(int loot_period, double loot_probality);

	void InsertMapInfo(json::array& loot_types);

	size_t GetLootCount(size_t index) const;

	json::array GetInfoByIndex(size_t index) const;

private:
	std::vector<json::array> loots_;
	int loot_period_ = 0;
	double loot_probality_ = 0;
};