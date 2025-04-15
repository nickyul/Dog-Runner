#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

#include <iostream>

#include "model.h"


namespace serialization {

    using namespace model;

    template <typename Archive>
    void serialize(Archive& ar, StructWithTwoDouble& str, [[maybe_unused]] const unsigned version) {
        ar& str.x;
        ar& str.y;
    }

    template <typename Archive>
    void serialize(Archive& ar, Map::Id& id, [[maybe_unused]] const unsigned version) {
        ar&* id;
        ar&* id;
    }

    class DogRepr {
    public:
        DogRepr() = default;

        explicit DogRepr(const Dog& dog)
            : dog_name_(dog.GetName()), position_(dog.GetPosition()), velocity_(dog.GetVelocity()), direct_(dog.GetDirect()), id_(dog.GetId()) {}

        [[nodiscard]] Dog Restore() const {
            return Dog{ dog_name_, position_, velocity_, direct_, id_ };
        }



        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& dog_name_;
            serialization::serialize(ar, position_, version);
            serialization::serialize(ar, velocity_, version);
            ar& direct_;
            ar& id_;
        }

    private:
        std::string dog_name_;
        Position position_;
        Velocity velocity_;
        Direct direct_ = Direct::NORTH;
        std::uint64_t id_;
    };

    class LootRepr {
    public:
        LootRepr() = default;

        explicit LootRepr(const Loot& loot)
            : loot_type_(loot.GetLootType()), pos_(loot.GetPosition()), is_collected_(loot.IsCollected()) {}

        [[nodiscard]] Loot Restore() const {
            Loot loot{ loot_type_, pos_ };
            if (is_collected_) {
                loot.SetCollected();
            }

            return loot;
        }

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& loot_type_;
            serialization::serialize(ar, pos_, version);
            ar& is_collected_;
        }

    private:
        int loot_type_;
        Position pos_;
        bool is_collected_ = false;
    };


    class PlayerRepr {
    public:
        PlayerRepr() = default;
        explicit PlayerRepr(const Player& player, std::string token)
            : score_(player.GetScore()), token_(token), id_(player.GetId()) {
            for (std::shared_ptr<Loot> loot : player.GetLootVector()) {
                LootRepr loot_repr(*loot);
                loots_.emplace_back(loot_repr);
            }
        }

        int GetPlayerScore() const {
            return score_;
        }

        const std::vector<LootRepr>& GetPlayerLootVector() const {
            return loots_;
        }

        const std::string& GetPlayerToken() const {
            return token_;
        }

        uint64_t GetPlayerId() const {
            return id_;
        }

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& loots_;
            ar& score_;
            ar& token_;
            ar& id_;
        }

    private:
        std::vector<LootRepr> loots_;
        int score_ = 0;
        std::string token_;
        uint64_t id_ = 0;
    };

    class PlayersRepr {
    public:

        PlayersRepr() = default;
        explicit PlayersRepr(const Players& players) {
            for (const auto& [token, player_ptr] : players.GetTokenToPlayer()) {
                PlayerRepr player_repr(*player_ptr, *token);
                players_.emplace_back(player_repr);
            }
        }

        const PlayerRepr& GetPlayerByDogId(uint64_t dog_id) const {
            const std::vector<PlayerRepr>::const_iterator it = std::find_if(players_.begin(), players_.end(), [dog_id](const PlayerRepr& player) {
                return player.GetPlayerId() == dog_id;
            });

            if (it != players_.end()) {
                return *it;
            }
            else {
                throw std::runtime_error("Can't recover player by dog_id");
            }
        }

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& players_;
        }

    private:
        std::vector<PlayerRepr> players_;
    };

    class GameSessionRepr {
    public:
        GameSessionRepr() = default;

        explicit GameSessionRepr(const GameSession& session) {
            for (std::shared_ptr dog : session.GetDogs()) {
                DogRepr dog_repr{ *dog };
                dogs_.emplace_back(dog_repr);
            }
            for (std::shared_ptr loot : session.GetLootVector()) {
                LootRepr loot_repr{ *loot };
                loots_.emplace_back(loot_repr);
            }
        }

        const std::vector<LootRepr>& GetLoots() const {
            return loots_;
        }

        const std::vector<DogRepr>& GetDogs() const {
            return dogs_;
        }


        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& dogs_;
            ar& loots_;
        }

    private:
        std::vector<DogRepr> dogs_;
        std::vector<LootRepr> loots_;
    };

    class SerializedData {
    public:
        SerializedData() = default;

        explicit SerializedData(const Game& game)
            :players_(game.GetPlayersClass()) {
            for (const auto& [map_id, sessions] : game.GetMapIdToSession()) {
                for (const GameSession& session : sessions) {
                    GameSessionRepr session_repr(session);
                    map_to_sessions_[*map_id].emplace_back(session_repr);
                }
            }
        }

        void Restore(Game& game) const {

            for (const auto& [string_map_id, sessions_repr] : map_to_sessions_) {
                for (const GameSessionRepr& session_repr : sessions_repr) {
                    GameSession& session = game.GetSession(Map::Id{ string_map_id });

                    for (const LootRepr& loot_repr : session_repr.GetLoots()) {
                        std::shared_ptr<Loot> loot_ptr = std::make_shared<Loot>(loot_repr.Restore());
                        session.AddExistLoot(loot_ptr);
                    }

                    for (const DogRepr& dog_repr : session_repr.GetDogs()) {
                        std::shared_ptr<Dog> dog_ptr = std::make_shared<Dog>(dog_repr.Restore());
                        session.AddDog(dog_ptr);

                        Player player{};
                        player.SetDog(dog_ptr);
                        player.SetSession(&session);

                        const PlayerRepr& player_repr = players_.GetPlayerByDogId(dog_ptr->GetId());

                        for (const LootRepr& loot_repr : player_repr.GetPlayerLootVector()) {
                            player.TakeLoot(std::make_shared<Loot>(loot_repr.Restore()));
                        }
                        player.SetScore(player_repr.GetPlayerScore());

                        game.AddExistPlayer(player, Token{ player_repr.GetPlayerToken() });

                    }
                }
            }
        }

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& map_to_sessions_;
            ar& players_;
        }

    private:
        std::unordered_map<std::string, std::deque<GameSessionRepr>> map_to_sessions_;
        PlayersRepr players_;
    };

    class SerializingListener : public ApplicationListener {
    public:
        SerializingListener() = default;

        SerializingListener(std::chrono::milliseconds save_period, Game& game, const std::filesystem::path& state_file_path)
            :save_period_(save_period), game_(game), state_file_path_(state_file_path) {}

        virtual ~SerializingListener() = default;

        void OnTick(int64_t time_delta) override {
            if (save_period_ == std::chrono::milliseconds{ 0 }) {
                return;
            }
            time_since_save_ += std::chrono::milliseconds{ time_delta };
            if (time_since_save_ > save_period_) {
                SaveStateGame();
                time_since_save_ = std::chrono::milliseconds{ 0 };
            }
        }

        void SaveStateGame() const {

            std::filesystem::path tmp_path = state_file_path_;
            tmp_path += ".tmp";
            std::ofstream ofs(tmp_path);

            if (!ofs.is_open()) {
                throw std::runtime_error("Failed to open state file");
            }
            boost::archive::text_oarchive oa(ofs);

            SerializedData data(game_);
            oa << data;

            ofs.close();
            try {
                std::filesystem::rename(tmp_path, state_file_path_);
            }
            catch (const std::filesystem::filesystem_error& e) {
                std::filesystem::remove(tmp_path);
            }
        }

        void RestoreGame(Game& game) const {
            std::ifstream ifs(state_file_path_);
            
            if (!ifs.is_open()) {
                return;
            }
            boost::archive::text_iarchive ia(ifs);
            SerializedData data;
            ia >> data;
            data.Restore(game);
        }

    private:
        std::chrono::milliseconds time_since_save_ = std::chrono::milliseconds{ 0 };
        std::chrono::milliseconds save_period_;
        Game& game_;
        const std::filesystem::path state_file_path_;
    };

}  
