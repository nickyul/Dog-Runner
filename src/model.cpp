#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

Token PlayerTokens::GenerateToken() {
    std::stringstream ss;
    ss << std::setw(16) << std::setfill('0') << std::hex << generator1_();
    ss << std::setw(16) << std::setfill('0') << std::hex << generator2_();
    std::string token = ss.str();
    assert(token.length() == 32);
    return Token(token);
}

bool PosIsAvailable(const std::set<std::shared_ptr<Road>>& roads, Position pos) {
    for (std::shared_ptr road_ptr : roads) {
        if (road_ptr->IsPositionOnRoadArea(pos)) {
            return true;
        }
    }
    return false;
}

Position GetStartPos(const GameSession* session) {
    const Map::Roads& roads = session->GetMapRoads();
    return Position(roads[0].GetStart().x, roads[0].GetStart().y);
}

Position GetRandomPos(const GameSession* session) {
    const model::Map::Roads& roads = session->GetMapRoads();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist_roads_size(0, roads.size() - 1);
    int road_index = dist_roads_size(gen);
    Road road = roads[road_index];

    std::uniform_real_distribution<> dist_road_width(-(Game::ROAD_WIDTH / 2), Game::ROAD_WIDTH / 2);
    double width = dist_road_width(gen);
    width = std::round(width * 100.) / 100;

    Point road_start = road.GetStart();
    Point road_end = road.GetEnd();

    Position pos{};
    if (road.IsHorizontal()) {
        std::uniform_real_distribution<> dist_road_length(std::min(road_start.x, road_end.x), std::max(road_start.x, road_end.x));
        pos.x = dist_road_length(gen);
        pos.y = road.GetStart().y + width;
    }
    if (road.IsVertical()) {
        std::uniform_real_distribution<> dist_road_length(std::min(road_start.y, road_end.y), std::max(road_start.y, road_end.y));
        pos.y = dist_road_length(gen);
        pos.x = road.GetStart().x + width;
    }
    pos.x = std::round(pos.x * 100.) / 100;
    pos.y = std::round(pos.y * 100.) / 100;

    return pos;
}

Map::Map(Id id, std::string name, double dog_speed, int bag_capacity) noexcept
    : id_(std::move(id))
    , name_(std::move(name))
    , dog_speed_(dog_speed)
    , bag_capacity_(bag_capacity) {}

double Map::GetSpeed() const noexcept {
    return dog_speed_;
}

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
    std::shared_ptr<Road> road_ptr = std::make_shared<Road>(roads_.back());

    if (road.IsHorizontal()) {
        for (Coord x = std::min(road_ptr->GetStart().x, road_ptr->GetEnd().x); x <= std::max(road_ptr->GetStart().x, road_ptr->GetEnd().x); ++x) {
            coord_to_road[{x, road_ptr->GetStart().y}].emplace(road_ptr);
        }
    }
    if (road.IsVertical()) {
        for (Coord y = std::min(road_ptr->GetStart().y, road_ptr->GetEnd().y); y <= std::max(road_ptr->GetStart().y, road_ptr->GetEnd().y); ++y) {
            coord_to_road[{road_ptr->GetStart().x, y}].emplace(road_ptr);
        }
    }
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Map::SetDogSpeed(double dog_speed) {
    dog_speed_ = dog_speed;
}

int Map::GetCapacity() const noexcept {
    return bag_capacity_;
}

std::optional<std::set<std::shared_ptr<Road>>> Map::GetRoadsOnPoint(const Point& point) const {
    if (auto it = coord_to_road.find(point); it != coord_to_road.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

std::pair<Token, Player&> Game::AddPlayer(std::string dog_name, GameSession* session) {
    return players_.Add(std::move(dog_name), session, random_spawn_);
}

Player* Game::FindPlayerByToken(Token token) const {
    return players_.FindPlayerByToken(token);
}

std::deque<Player>& Game::GetPlayers() {
    return players_.GetPlayers();
}

GameSession& Game::GetSession(const Map::Id& id) {
    if (auto it = map_id_to_sessions_.find(id); it != map_id_to_sessions_.end()) {
        if (it->second.empty()) {
            return map_id_to_sessions_[id].emplace_back(FindMap(id));
        }
        auto session_it = std::find_if(it->second.begin(), it->second.end(),
            [](const GameSession& session) {
                return session.GetNumberOfDogs() < MAX_COUNT_PLAYERS_ON_SESSION;
            });

        if (session_it != it->second.end()) {
            return *session_it;
        }
    }
    return map_id_to_sessions_[id].emplace_back(FindMap(id));
}

void Game::GameTick(int64_t time_delta) {

    CheckInactivePlayers(time_delta);

    for (auto& [map_id, session_container] : map_id_to_sessions_) {
        for (GameSession& session : session_container) {
            //collisions
            collision_detector::ItemGatherer item_gatherer;
            
            for (std::shared_ptr<Dog> dog_ptr : session.GetDogs()) {
                Player* player_ptr = players_.FindByDogIdAndMapId(dog_ptr->GetId(), session.GetMapId());

                collision_detector::Gatherer gatherer;
                gatherer.start_pos = { player_ptr->GetPetPosition().x, player_ptr->GetPetPosition().y };
                player_ptr->MakeMove(time_delta);
                gatherer.end_pos = { player_ptr->GetPetPosition().x, player_ptr->GetPetPosition().y };
                gatherer.width = PLAYER_WIDTH;

                item_gatherer.AddGatherer(gatherer);
            }

            for (std::shared_ptr<Loot> item : session.GetLootVector()) {
                item_gatherer.AddItem(collision_detector::Item({item->GetPosition().x, item->GetPosition().y}, LOOT_WIDTH));
            }      

            for (const Office& office : session.GetMapPtr()->GetOffices()) {
                item_gatherer.AddItem(collision_detector::Item( { static_cast<double>(office.GetPosition().x), static_cast<double>(office.GetPosition().y) }, BASE_WIDTH ));
            }

            for (const collision_detector::GatheringEvent& event : collision_detector::FindGatherEvents(item_gatherer)) {
                Player* player_ptr = players_.FindByDogIdAndMapId(session.GetDogId(event.gatherer_id), session.GetMapId());
                // dog found loot
                if (event.item_id < session.GetLootCount()) {
                    if (session.GetLootPtr(event.item_id)->IsCollected()) {
                        continue;
                    }
                    // bag_capacity let take a loot
                    if (session.GetMapPtr()->GetCapacity() > player_ptr->GetLootCount()) {
                        player_ptr->TakeLoot(session.GetLootPtr(event.item_id));
                    }
                }
                // dog found office
                else {
                    player_ptr->ReturnLoot(GetMapInfoJson(session.GetMapId()));
                }

            }

            session.EraseTookedLoot();

            // Adding loot
            unsigned loots = loot_generator_->Generate(std::chrono::milliseconds(time_delta), session.GetLootCount(), session.GetNumberOfDogs());
            while (loots) {
                session.AddLoot(GetRandomLootType(session.GetMapId()));
                --loots;
            }
        }
    }

    if (listener_) {
        listener_->OnTick(time_delta);
    }
}

void Game::SetInternalTicker() {
    internal_ticker_ = true;
}

bool Game::IsTickerInternal() const {
    return internal_ticker_;
}

void Game::SetRandomSpawnTrue() {
    random_spawn_ = true;
}

bool Game::IsSpawnRandom() const {
    return random_spawn_;
}

void Game::SetExtraData(std::shared_ptr<ExtraData> extra_data) {
    extra_data_ = extra_data;
}

void Game::SetLootGenerator(std::shared_ptr<loot_gen::LootGenerator> loot_generator) {
    loot_generator_ = loot_generator;
}

int Game::GetRandomLootType(Map::Id map_id) const {
    size_t loot_count = extra_data_->GetLootCount(map_id_to_index_.at(map_id));
    std::random_device rd;
    std::mt19937 gen(rd());
    
    std::uniform_int_distribution<> dis(0, static_cast<int>(loot_count) - 1);

    return dis(gen);
}

json::array Game::GetMapInfoJson(Map::Id id) const {
    return extra_data_->GetInfoByIndex(map_id_to_index_.at(id));
}

Player* Game::FindByDogIdAndMapId(uint64_t dog_id, Map::Id map_id) {
    return players_.FindByDogIdAndMapId(dog_id, map_id);
}

void Game::SetApplicationListener(ApplicationListener* listener) {
    listener_ = listener;
}

const Players& Game::GetPlayersClass() const {
    return players_;
}

const Game::MapIdToSession& Game::GetMapIdToSession() const {
    return map_id_to_sessions_;
}

void Game::AddExistPlayer(const Player& player, Token token) {
    players_.AddExistPlayer(player, token);
}

void Game::SetDogRetirementTime(const double retirement_time) noexcept {
    dog_retirement_time_ = retirement_time;
}

void Game::CheckInactivePlayers(int64_t time_delta) {
    for (Player& player : players_.GetPlayers()) {
        if (std::optional<uint64_t> inactivity_time = player.GetInactivityTime(); inactivity_time.value_or(0) + time_delta >= 15000) {
            player.UpdatePlayTime(time_delta);
            db_->SaveRecord(player.GetPetName(), player.GetScore(), player.GetPlayTime());
            GameSession* session = player.GetSessionPtr();
            session->RemoveDog(player.GetId());
            players_.RemovePlayer(player);
        }
        else {
            player.UpdatePlayTime(time_delta);
        }
    }
}

void Game::SetDb(std::shared_ptr<Database> db_ptr) {
    db_ = db_ptr;
}

json::array Game::GetRecords(int limit, int offset) {
    return db_->GetRecords(limit, offset);
}

Road::Road(HorizontalTag, Point start, Coord end_x) noexcept
    : start_{ start }
    , end_{ end_x, start.y } {
    SetRoadArea();
}

Road::Road(VerticalTag, Point start, Coord end_y) noexcept
    : start_{ start }
    , end_{ start.x, end_y } {
    SetRoadArea();
}

bool Road::IsHorizontal() const noexcept {
    return start_.y == end_.y;
}

bool Road::IsVertical() const noexcept {
    return start_.x == end_.x;
}

Point Road::GetStart() const noexcept {
    return start_;
}

Point Road::GetEnd() const noexcept {
    return end_;
}

bool Road::IsPositionOnRoadArea(Position pos) const {
    return (road_area_.min_left.x <= pos.x && pos.x <= road_area_.max_right.x) &&
        (road_area_.min_left.y <= pos.y && pos.y <= road_area_.max_right.y);
}

Road::RoadArea Road::GetRoadArea() const {
    return road_area_;
}

void Road::SetRoadArea() {
    if (this->IsHorizontal()) {
        road_area_.min_left = { std::min(start_.x, end_.x) - 0.4, start_.y - 0.4 };
        road_area_.max_right = { std::max(start_.x, end_.x) + 0.4, start_.y + 0.4 };
    }
    if (this->IsVertical()) {
        road_area_.min_left = { start_.x - 0.4, std::min(start_.y, end_.y) - 0.4 };
        road_area_.max_right = { start_.x + 0.4, std::max(start_.y, end_.y) + 0.4 };
    }
}

Building::Building(Rectangle bounds) noexcept
    : bounds_{ bounds } {
}

const Rectangle& Building::GetBounds() const noexcept {
    return bounds_;
}

Office::Office(Id id, Point position, Offset offset) noexcept
    : id_{ std::move(id) }
    , position_{ position }
    , offset_{ offset } {
}

const Office::Id& Office::GetId() const noexcept {
    return id_;
}
Point Office::GetPosition() const noexcept {
    return position_;
}

Offset Office::GetOffset() const noexcept {
    return offset_;
}

size_t Map::PointHash::operator()(const Point& point) const {
    size_t h1 = std::hash<int>()(point.x);
    size_t h2 = std::hash<int>()(point.y);
    return h1 ^ (h2 << 1);
}

Dog::Dog(std::string dog_name, Position position)
    : dog_name_(std::move(dog_name)), position_(position), velocity_({ .0,.0 }), direct_(Direct::NORTH), id_(id_counter++) {}

Dog::Dog(std::string dog_name, Position position, Velocity velocity, Direct direct, std::uint64_t id)
    : dog_name_(dog_name), position_(position), velocity_(velocity), direct_(direct), id_(id) {}

std::string Dog::GetName() const {
    return dog_name_;
}

Position Dog::GetPosition() const {
    return position_;
}

Velocity Dog::GetVelocity() const {
    return velocity_;
}

Direct Dog::GetDirect() const {
    return direct_;
}

void Dog::SetDirect(Direct direct) {
    direct_ = direct;
}

void Dog::SetSpeed(Velocity velocity) {
    velocity_ = velocity;
}

void Dog::SetNewPosition(Position pos) {
    position_ = pos;
}

std::uint64_t Dog::GetId() const {
    return id_;
}

GameSession::GameSession(const Map* map)
    :map_(map) {}

const Dog* GameSession::AddDog(std::shared_ptr<Dog> dog_ptr) {
    return dogs_.emplace_back(dog_ptr).get();
}

uint64_t GameSession::GetNumberOfDogs() const {
    return dogs_.size();
}

Map::Id GameSession::GetMapId() const {
    return map_->GetId();
}

const Map* GameSession::GetMapPtr() const {
    return map_;
}

double GameSession::GetMapSpeed() const {
    return map_->GetSpeed();
}

const Map::Roads& GameSession::GetMapRoads() const {
    return map_->GetRoads();
}

const std::deque<std::shared_ptr<Dog>>& GameSession::GetDogs() const {
    return dogs_;
}

size_t GameSession::GetLootCount() const {
    return loots_.size();
}

std::shared_ptr<Loot> GameSession::GetLootPtr(size_t idx) {
    return loots_.at(idx);
}

void GameSession::EraseTookedLoot() {
    for (std::vector<std::shared_ptr<Loot>>::iterator it = loots_.begin(); it != loots_.end();) {
        if (it->get()->IsCollected()) {
            it = loots_.erase(it);
        }
        else {
            ++it;
        }
    }
}

const std::vector<std::shared_ptr<Loot>>& GameSession::GetLootVector() const {
    return loots_;
}

uint64_t GameSession::GetDogId(size_t idx) const {
    assert(dogs_.size() > idx);
    return dogs_.at(idx)->GetId();
}

void GameSession::RemoveDog(uint64_t dog_id) {
    auto it = std::find_if(dogs_.begin(), dogs_.end(), [dog_id](const std::shared_ptr<Dog>& dog) {
        return dog->GetId() == dog_id;
        });

    dogs_.erase(it);
}

void GameSession::AddLoot(int loot_type) {
    Position pos = GetRandomPos(this);
    loots_.emplace_back(std::make_shared<Loot>(loot_type, pos));
}

void GameSession::AddExistLoot(std::shared_ptr<Loot> loot_ptr) {
    loots_.emplace_back(loot_ptr);
}

Player::Player(std::string dog_name, GameSession* session, bool random_spawn)
    :session_(session) {
    if (random_spawn) {
        dog_ = std::make_shared<Dog>(std::move(dog_name), GetRandomPos(session_));
    }
    else {
        dog_ = std::make_shared<Dog>(std::move(dog_name), GetStartPos(session_));
    }
    session->AddDog(dog_);
}

std::string Player::GetPetName() const {
    return dog_->GetName();
}

Position Player::GetPetPosition() const {
    return dog_->GetPosition();
}

Velocity Player::GetPetVelocity() const {
    return dog_->GetVelocity();
}

Direct Player::GetPetDirect() const {
    return dog_->GetDirect();
}

void Player::SetUpDir() {
    double dog_speed = session_->GetMapSpeed();
    dog_->SetSpeed({ 0,-1 * dog_speed });
    dog_->SetDirect(Direct::NORTH);
    UpdateActivity();
}

void Player::SetDownDir() {
    double dog_speed = session_->GetMapSpeed();
    dog_->SetSpeed({ 0,dog_speed });
    dog_->SetDirect(Direct::SOUTH);
    UpdateActivity();
}

void Player::SetLeftDir() {
    double dog_speed = session_->GetMapSpeed();
    dog_->SetSpeed({ -1* dog_speed,0 });
    dog_->SetDirect(Direct::WEST);
    UpdateActivity();
}

void Player::SetRightDir() {
    double dog_speed = session_->GetMapSpeed();
    dog_->SetSpeed({ dog_speed,0 });
    dog_->SetDirect(Direct::EAST);
    UpdateActivity();
}

void Player::SetStopDir() {
    dog_->SetSpeed({ .0,.0 });
    inactivity_time_ = 0;
}

void Player::MakeMove(int64_t delta_time) {
    session_->GetMapSpeed();

    Position curr_pos = dog_->GetPosition();
    Position new_pos = curr_pos;

    switch (dog_->GetDirect()) {
    case Direct::NORTH:
        new_pos.y += dog_->GetVelocity().y * (static_cast<double>(delta_time) / 1000);
        break;

    case Direct::SOUTH:
        new_pos.y += dog_->GetVelocity().y * (static_cast<double>(delta_time) / 1000);
        break;

    case Direct::WEST:
        new_pos.x += dog_->GetVelocity().x * (static_cast<double>(delta_time) / 1000);
        break;

    case Direct::EAST:
        new_pos.x += dog_->GetVelocity().x * (static_cast<double>(delta_time) / 1000);
        break;
    }

    Point curr_point = Point{ static_cast<Coord>(std::round(curr_pos.x)), static_cast<Coord>(std::round(curr_pos.y)) };

    if (PosIsAvailable(session_->GetMapPtr()->GetRoadsOnPoint(curr_point).value(), new_pos)) {
        dog_->SetNewPosition(new_pos);
    }
    else {
        dog_->SetNewPosition(GetAvailablePos(session_->GetMapPtr()->GetRoadsOnPoint(curr_point).value()));
    }
}

GameSession* Player::GetSessionPtr() const {
    return session_;
}

std::uint64_t Player::GetId() const {
    return dog_->GetId();
}

size_t Player::GetLootCount() const noexcept {
    return loots_.size();
}

void Player::TakeLoot(std::shared_ptr<Loot> loot_ptr) {
    loots_.emplace_back(loot_ptr)->SetCollected();
}

const std::vector<std::shared_ptr<Loot>>& Player::GetLootVector() const {
    return loots_;
}

int Player::GetScore() const noexcept {
    return score_;
}

void Player::ReturnLoot(const json::array& map_info) {
    for (int i = loots_.size() - 1; i >= 0; --i) {
        score_ += map_info.at(loots_.at(i)->GetLootType()).as_object().at("value").as_int64();
        loots_.pop_back();
    }
}

void Player::SetSession(GameSession* session) {
    session_ = session;
}

void Player::SetScore(int score) noexcept {
    score_ = score;
}

void Player::SetDog(std::shared_ptr<Dog> dog_ptr) {
    dog_ = dog_ptr;
}

void Player::UpdateActivity() {
    inactivity_time_ = std::nullopt;
}

std::optional<uint64_t> Player::GetInactivityTime() const {
    return inactivity_time_;
}

uint64_t Player::GetPlayTime() const {
    return play_time_;
}

void Player::UpdatePlayTime(uint64_t time_delta) {
    play_time_ += time_delta;
    if (inactivity_time_.has_value()) {
        inactivity_time_ = inactivity_time_.value() + time_delta;
    }
}

Position Player::GetAvailablePos(const std::set<std::shared_ptr<Road>>& roads) {
    Position near_new_pos = dog_->GetPosition();
    double coor;
    switch (dog_->GetDirect()) {
    case Direct::NORTH:
        coor = dog_->GetPosition().y;
        for (std::shared_ptr<Road> road : roads) {
            if (double iter = road->GetRoadArea().min_left.y; (iter < coor) && (std::abs(iter - coor) < 0.4)) {
                coor = iter;
            }
        }
        dog_->SetSpeed({ .0,.0 });
        near_new_pos.y = coor;
        break;

    case Direct::SOUTH:
        coor = dog_->GetPosition().y;
        for (std::shared_ptr<Road> road : roads) {
            if (double iter = road->GetRoadArea().max_right.y; (iter > coor) && (std::abs(iter - coor) < 0.4)) {
                coor = iter;
            }
        }
        dog_->SetSpeed({ .0,.0 });
        near_new_pos.y = coor;
        break;

    case Direct::WEST:
        coor = dog_->GetPosition().x;
        for (std::shared_ptr<Road> road : roads) {
            if (double iter = road->GetRoadArea().min_left.x; (iter < coor) && (std::abs(iter - coor) < 0.4)) {
                coor = iter;
            }
        }
        dog_->SetSpeed({ .0,.0 });
        near_new_pos.x = coor;
        break;

    case Direct::EAST:
        coor = dog_->GetPosition().x;
        for (std::shared_ptr<Road> road : roads) {
            if (double iter = road->GetRoadArea().max_right.x; (iter > coor) && (std::abs(iter - coor) < 0.4)) {
                coor = iter;
            }
        }
        dog_->SetSpeed({ .0,.0 });
        near_new_pos.x = coor;
        break;
    }
    return near_new_pos;
}

size_t PairHasher::operator()(const std::pair<Map::Id, uint64_t>& hash) const {
    size_t result = 0;
    int counter = 0;
    for (char ch : *hash.first) {
        result += static_cast<int>(ch) + hash.second;
        result *= static_cast<size_t>(std::pow(17, counter));
        ++counter;
    }
    return result;
}

std::pair<Token, Player&> Players::Add(std::string dog_name, GameSession* session, bool random_spawn) {
    Player& player = players_.emplace_back(std::move(dog_name), session, random_spawn);
    Token token = PlayerTokens().GenerateToken();
    token_to_player_[token] = &player;
    map_id_dog_id_to_index_[{session->GetMapId(), player.GetId()}] = players_.size() - 1;
    return { token, player };
}

Player* Players::FindByDogIdAndMapId(uint64_t dog_id, Map::Id map_id) {
    if (auto it = map_id_dog_id_to_index_.find({ map_id, dog_id }); it != map_id_dog_id_to_index_.end()) {
        return &players_[it->second];
    }
    else {
        return nullptr;
    }
}

std::deque<Player>& Players::GetPlayers() {
    return players_;
}

Player* Players::FindPlayerByToken(Token token) const {
    auto it = token_to_player_.find(token);
    return it != token_to_player_.end() ? it->second : nullptr;
}

const Players::TokenToPlayer& Players::GetTokenToPlayer() const {
    return token_to_player_;
}

void Players::AddExistPlayer(const Player& player, Token token) {
    token_to_player_[token] = &players_.emplace_back(player);
    map_id_dog_id_to_index_[{player.GetSessionPtr()->GetMapId(), player.GetId()}] = players_.size() - 1;
}

void Players::RemovePlayer(const Player& player) {
    auto it = std::find_if(players_.begin(), players_.end(), [&player](const Player& p) {
        return p.GetId() == player.GetId();
        });

    if (it != players_.end()) {
        for (auto token_it = token_to_player_.begin(); token_it != token_to_player_.end(); ) {
            if (token_it->second == &(*it)) {
                token_it = token_to_player_.erase(token_it);
            }
            else {
                ++token_it;
            }
        }
        auto map_dog_it = map_id_dog_id_to_index_.find({ player.GetSessionPtr()->GetMapId(), player.GetId() });
        if (map_dog_it != map_id_dog_id_to_index_.end()) {
            map_id_dog_id_to_index_.erase(map_dog_it);
        }
        uint64_t removed_index = std::distance(players_.begin(), it);
        players_.erase(it);
        for (auto& [key, index] : map_id_dog_id_to_index_) {
            if (index > removed_index) {
                --index;
            }
        }
    }
}

Position Loot::GetPosition() const noexcept {
    return pos_;
}

int Loot::GetLootType() const noexcept {
    return loot_type_;
}

int Loot::GetLootId() const noexcept {
    return loot_id_;
}

bool Loot::IsCollected() const noexcept {
    return is_collected_;
}

void Loot::SetCollected() {
    is_collected_ = true;
}

}  // namespace model
