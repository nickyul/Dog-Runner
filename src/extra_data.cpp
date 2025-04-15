#include "extra_data.h"

ExtraData::ExtraData(int loot_period, double loot_probality)
	:loot_period_(loot_period), loot_probality_(loot_probality) {}

void ExtraData::SetLootInfo(int loot_period, double loot_probality) {
	loot_period_ = loot_period;
	loot_probality_ = loot_probality;
}

void ExtraData::InsertMapInfo(json::array& loot_types) {	
	loots_.emplace_back(loot_types);
}

size_t ExtraData::GetLootCount(size_t index) const {
	return loots_.at(index).size();
}

json::array ExtraData::GetInfoByIndex(size_t index) const {
	return loots_.at(index);
}
