#pragma once
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <sstream>
#include <random>
#include <deque>
#include <cassert>
#include <iomanip>
#include <set>
#include <optional>

#include "collision_detector.h"
#include "tagged.h"
#include "extra_data.h"
#include "loot_generator.h"

using namespace std::literals;


namespace model {

namespace detail {
    struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerTokens {
public:
    Token GenerateToken();
private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{ [this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }() };
    std::mt19937_64 generator2_{ [this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }() };
};

static std::uint64_t id_counter = 0;
static int loot_id_counter = 0;

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct StructWithTwoDouble {
    double x;
    double y;

    bool operator==(const StructWithTwoDouble& other) const {
        return x == other.x && y == other.y;
    }
};

using Velocity = StructWithTwoDouble;

using Position = StructWithTwoDouble;

enum class Direct {
    NORTH, SOUTH, WEST, EAST
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};
   
    struct RoadArea {
        StructWithTwoDouble min_left;
        StructWithTwoDouble max_right;
    };

    Road(HorizontalTag, Point start, Coord end_x) noexcept;

    Road(VerticalTag, Point start, Coord end_y) noexcept;

    bool IsHorizontal() const noexcept;

    bool IsVertical() const noexcept;

    Point GetStart() const noexcept;

    Point GetEnd() const noexcept;

    bool IsPositionOnRoadArea(Position pos) const;

    RoadArea GetRoadArea() const;

private:
    void SetRoadArea();

    Point start_;
    Point end_;
    RoadArea road_area_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept;

    const Rectangle& GetBounds() const noexcept;

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept;

    const Id& GetId() const noexcept;

    Point GetPosition() const noexcept;

    Offset GetOffset() const noexcept;

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    struct PointHash {
        size_t operator()(const Point& point) const;
    };

    using Id = util::Tagged<std::string, Map>;
    using Roads = std::deque<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
    using PositionToRoads = std::unordered_map<Point, std::set<std::shared_ptr<Road>>, PointHash>;

    Map(Id id, std::string name, double dog_speed, int bag_capacity) noexcept;

    double GetSpeed() const noexcept;

    const Id& GetId() const noexcept;

    const std::string& GetName() const noexcept;

    const Buildings& GetBuildings() const noexcept;

    const Roads& GetRoads() const noexcept;

    const Offices& GetOffices() const noexcept;

    void AddRoad(const Road& road);

    void AddBuilding(const Building& building);

    void AddOffice(Office office);

    void SetDogSpeed(double dog_speed);

    int GetCapacity() const noexcept;
    

    std::optional<std::set<std::shared_ptr<Road>>> GetRoadsOnPoint(const Point& point) const;

private:

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    double dog_speed_;
    int bag_capacity_;

    PositionToRoads coord_to_road;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Dog {
public:
    explicit Dog(std::string dog_name, Position position);

    explicit Dog(std::string dog_name, Position position, Velocity velocity, Direct direct, std::uint64_t id);

    std::string GetName() const;

    Position GetPosition() const;

    Velocity GetVelocity() const;

    Direct GetDirect() const;

    void SetDirect(Direct direct);

    void SetSpeed(Velocity velocity);

    void SetNewPosition(Position pos);

    std::uint64_t GetId() const;

private:
    std::string dog_name_;
    Position position_;
    Velocity velocity_;
    Direct direct_ = Direct::NORTH;
    std::uint64_t id_;
};

class Loot {
public:
    explicit Loot(int loot_type, Position pos)
        :loot_id_(loot_id_counter++), loot_type_(loot_type), pos_(pos) {}
    
    Position GetPosition() const noexcept;

    int GetLootType() const noexcept;

    int GetLootId() const noexcept;

    bool IsCollected() const noexcept;

    void SetCollected();

private:
    int loot_id_;
    int loot_type_;
    Position pos_;
    bool is_collected_ = false;
};

class GameSession {
public:
    explicit GameSession(const Map* map);

    const Dog* AddDog(std::shared_ptr<Dog> dog_ptr);

    uint64_t GetNumberOfDogs() const;

    Map::Id GetMapId() const;

    const Map* GetMapPtr() const;

    double GetMapSpeed() const;

    const Map::Roads& GetMapRoads() const;

    const std::deque<std::shared_ptr<Dog>>& GetDogs() const;

    void AddLoot(int loot_type);

    void AddExistLoot(std::shared_ptr<Loot> loot_ptr);

    size_t GetLootCount() const;

    std::shared_ptr<Loot> GetLootPtr(size_t idx);

    void EraseTookedLoot();

    const std::vector<std::shared_ptr<Loot>>& GetLootVector() const;

    uint64_t GetDogId(size_t idx) const;

    void RemoveDog(uint64_t dog_id);

private:
    const Map* map_;
    std::deque<std::shared_ptr<Dog>> dogs_;
    std::vector<std::shared_ptr<Loot>> loots_;
};

class Player {
public:

    Player() = default;

    explicit Player(std::string dog_name, GameSession* session, bool random_spawn);

    std::string GetPetName() const;

    Position GetPetPosition() const;

    Velocity GetPetVelocity() const;

    Direct GetPetDirect() const;

    void SetUpDir();

    void SetDownDir();

    void SetLeftDir();

    void SetRightDir();

    void SetStopDir();

    void MakeMove(int64_t delta_time);

    GameSession* GetSessionPtr() const;

    std::uint64_t GetId() const;

    size_t GetLootCount() const noexcept;

    void TakeLoot(std::shared_ptr<Loot> loot_ptr);

    const std::vector<std::shared_ptr<Loot>>& GetLootVector() const;

    int GetScore() const noexcept;

    void ReturnLoot(const json::array& map_info);

    void SetSession(GameSession* session);

    void SetScore(int score) noexcept;

    void SetDog(std::shared_ptr<Dog> dog_ptr);
    
    void UpdateActivity();

    std::optional<uint64_t> GetInactivityTime() const;

    uint64_t GetPlayTime() const;

    void UpdatePlayTime(uint64_t time_delta);

private:
    Position GetAvailablePos(const std::set<std::shared_ptr<Road>>& roads);

    GameSession* session_;
    std::shared_ptr<Dog> dog_;
    std::vector<std::shared_ptr<Loot>> loots_;
    int score_ = 0;
    uint64_t play_time_ = 0;
    std::optional<uint64_t> inactivity_time_ = 0;
};

struct PairHasher {

    size_t operator()(const std::pair<Map::Id, uint64_t>& hash) const;

};

class Players {
public:
    using TokenToPlayer = std::unordered_map<Token, Player*, util::TaggedHasher<Token>>;

    std::pair<Token, Player&> Add(std::string dog_name, GameSession* session, bool random_spawn);

    Player* FindByDogIdAndMapId(uint64_t dog_id, Map::Id map_id);
    
    std::deque<Player>& GetPlayers();

    Player* FindPlayerByToken(Token token) const;

    const TokenToPlayer& GetTokenToPlayer() const;

    void AddExistPlayer(const Player& player, Token token);

    void RemovePlayer(const Player& player);

private:
    std::unordered_map<std::pair<Map::Id, uint64_t>, uint64_t, PairHasher> map_id_dog_id_to_index_;
    std::deque<Player> players_;
    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;

};

class ApplicationListener {
protected:
    ~ApplicationListener() = default;
public:
    virtual void OnTick(int64_t time_delta) = 0;
};

class Database {
public:
    virtual void SaveRecord(std::string name, int score, uint64_t played_time) = 0;
    virtual json::array GetRecords(int limit, int offset) = 0;
    virtual ~Database() = default;
};

class Game {
public:
    using Maps = std::vector<Map>;

    static constexpr size_t MAX_COUNT_PLAYERS_ON_SESSION = 100;
    static constexpr double PLAYER_WIDTH = 0.6;
    static constexpr double BASE_WIDTH = 0.5;
    static constexpr double LOOT_WIDTH = 0.;
    static constexpr double ROAD_WIDTH = 0.8;

    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using MapIdToSession = std::unordered_map<Map::Id, std::deque<GameSession>, MapIdHasher>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept;

    const Map* FindMap(const Map::Id& id) const noexcept;

    std::pair<Token, Player&> AddPlayer(std::string dog_name, GameSession* session);

    Player* FindPlayerByToken(Token token) const;

    std::deque<Player>& GetPlayers();

    GameSession& GetSession(const Map::Id& id);

    void GameTick(int64_t time_delta);

    void SetInternalTicker();

    bool IsTickerInternal() const;

    void SetRandomSpawnTrue();
    
    bool IsSpawnRandom() const;

    void SetExtraData(std::shared_ptr<ExtraData> extra_data);

    void SetLootGenerator(std::shared_ptr<loot_gen::LootGenerator> loot_generator);

    int GetRandomLootType(Map::Id map_id) const;

    json::array GetMapInfoJson(Map::Id id) const;

    Player* FindByDogIdAndMapId(uint64_t dog_id, Map::Id map_id);

    void SetApplicationListener(ApplicationListener* listener);

    const Players& GetPlayersClass() const;

    const MapIdToSession& GetMapIdToSession() const;

    void AddExistPlayer(const Player& player, Token token);

    void SetDogRetirementTime(const double retirement_time) noexcept;

    void CheckInactivePlayers(int64_t time_delta);

    void SetDb(std::shared_ptr<Database> db_ptr);

    json::array GetRecords(int limit, int offset);


private:
    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    MapIdToSession map_id_to_sessions_;

    Players players_;

    std::shared_ptr<ExtraData> extra_data_;
    std::shared_ptr<loot_gen::LootGenerator> loot_generator_;

    bool internal_ticker_ = false;
    bool random_spawn_ = false;
    ApplicationListener* listener_ = nullptr;
    double dog_retirement_time_ = 60;
    std::shared_ptr<Database> db_ = nullptr;
};

bool PosIsAvailable(const std::set<std::shared_ptr<Road>>& roads, Position pos);

Position GetStartPos(const GameSession* session);

Position GetRandomPos(const GameSession* session);

}  // namespace model
