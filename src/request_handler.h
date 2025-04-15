#pragma once
#include <boost/json.hpp>
#include <boost/asio/io_context.hpp>
#include "http_server.h"
#include "model.h"
#include "logger.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>
#include <unordered_map>

namespace http_handler {

    namespace json = boost::json;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace fs = std::filesystem;
    namespace sys = boost::system;
    namespace net = boost::asio;

    using namespace std::literals;

    // Запрос, тело которого представлено в виде строки
    using StringRequest = http::request<http::string_body>;
    // Ответ, тело которого представлено в виде строки
    using StringResponse = http::response<http::string_body>;
    // Ответ, тело которого представлено в виде файла
    using FileResponse = http::response<http::file_body>;
    // Ответ RequestHendler для логгера
    using HandlerResponse = std::variant<StringResponse, FileResponse>;

    struct ContentType {
        ContentType() = delete;
        constexpr static std::string_view TEXT_HTML = "text/html"sv;
        constexpr static std::string_view JSON = "application/json"sv;
        constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
        constexpr static std::string_view CSS = "text/css"sv;
        constexpr static std::string_view JS = "text/javascript"sv;
        constexpr static std::string_view XML = "application/xml"sv;
        constexpr static std::string_view PNG = "image/png"sv;
        constexpr static std::string_view JPEG = "image/jpeg"sv;
        constexpr static std::string_view GIF = "image/gif"sv;
        constexpr static std::string_view BMP = "image/bmp"sv;
        constexpr static std::string_view ICO = "image/vnd.microsoft.icon"sv;
        constexpr static std::string_view TIFF = "image/tiff"sv;
        constexpr static std::string_view SVG = "image/svg+xml"sv;
        constexpr static std::string_view MP3 = "audio/mpeg"sv;
        constexpr static std::string_view OCTET_STREAM = "octet-stream"sv;
    };

    StringResponse MakeStringResponse(http::status status, std::string_view body, size_t body_size,
        unsigned http_version, bool keep_alive, std::string_view content_type = ContentType::TEXT_HTML);

    enum class RequestTarget {
        UNKNOWN, PLAYERS, JOIN, MAPS, MAP, STATE, ACTION, TICK, RECORDS
    };

    class ApiHandler;

    class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
    public:
        using Strand = net::strand<net::io_context::executor_type>;

        explicit RequestHandler(model::Game& game, std::string static_path, Strand& api_strand)
            : static_path_{ fs::path(static_path) }, game_(game), api_strand_(api_strand) {}

        RequestHandler(const RequestHandler&) = delete;
        RequestHandler& operator=(const RequestHandler&) = delete;

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            if (req.target().substr(0, 5) == "/api/"sv) {
                auto handle = [self = shared_from_this(), send, req = std::forward<decltype(req)>(req)] {
                    assert(self->api_strand_.running_in_this_thread());
                    return send(self->HandleApiRequest(req));
                };
                return net::dispatch(api_strand_, handle);                    
            }
            else if (req.method() == http::verb::get || req.method() == http::verb::head) {
                send(ResponseStaticFile(std::move(req)));
            }
            else {
                send(ResponseBadRequestStatic(std::move(req)));
            }
        }
        
    private:

        template <typename Body, typename Allocator>
        HandlerResponse HandleApiRequest(http::request<Body, http::basic_fields<Allocator>> req) {
            ApiHandler api(game_);
            return api(std::move(req));
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseStaticFile(http::request<Body, http::basic_fields<Allocator>>&& req) const {
            std::string decoded_target(DecodeUrl(req.target()));

            fs::path path{ decoded_target };
            fs::path target_path = fs::weakly_canonical(static_path_ / path);
            if (!IsSubPath(target_path)) {
                return ResponseBadRequestStatic(std::move(req));
            }
            if (target_path == fs::weakly_canonical(static_path_)) {
                decoded_target += "index.html"s;
            }

            FileResponse res;
            res.version(req.version());
            res.result(http::status::ok);
            AddContentType(res, target_path);

            http::file_body::value_type file;
            if (sys::error_code ec; file.open(decoded_target.c_str(), beast::file_mode::read, ec), ec) {
                return ResponseNotFoundStatic(std::move(req));
            }

            res.body() = std::move(file);
            res.prepare_payload();

            return HandlerResponse(std::move(res));
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseBadRequestStatic(http::request<Body, http::basic_fields<Allocator>>&& req) const {
            const auto string_response = [&req](http::status status, std::string_view text, size_t body_size) {
                return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::TEXT_PLAIN);
                };

            json::object response;
            response.emplace("code", "badRequest");
            response.emplace("message", "Bad request");

            std::string str_response = std::move(json::serialize(response));

            return HandlerResponse(string_response(http::status::bad_request, str_response, str_response.size()));
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseNotFoundStatic(http::request<Body, http::basic_fields<Allocator>>&& req) const {
            const auto string_response = [&req](http::status status, std::string_view text, size_t body_size) {
                return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::TEXT_PLAIN);
                };

            json::object response;
            response.emplace("code", "fileNotFound");
            response.emplace("message", "File not found");

            std::string str_response = std::move(json::serialize(response));

            return HandlerResponse(string_response(http::status::not_found, str_response, str_response.size()));
        }

        std::string DecodeUrl(beast::string_view target) const;

        // Возвращает true, если каталог p содержится внутри base_path.
        bool IsSubPath(fs::path path) const;

        void AddContentType(http::response<http::file_body>& res, fs::path& path) const;

        const fs::path static_path_;
        model::Game& game_;
        Strand api_strand_;
    };

    class ApiHandler {
    public:
        explicit ApiHandler(model::Game& game)
            : game_(game) {}

        template <typename Body, typename Allocator>
        HandlerResponse operator()(http::request<Body, http::basic_fields<Allocator>>&& req) {
            switch (GetRequestTarget(req.target())) {
            
            case RequestTarget::JOIN:
                return ResponseJoinTarget(std::move(req));
            
            case RequestTarget::PLAYERS:
                return ResponsePlayersResponse(std::move(req));
            
            case RequestTarget::MAPS:
                return ResponseMaps(std::move(req));
            
            case RequestTarget::MAP:
                return ResponseMapsById(std::move(req));
            
            case RequestTarget::STATE:
                return ResponseStateTarget(std::move(req));

            case RequestTarget::ACTION:
                return ResponseActionTarget(std::move(req));
            
            case RequestTarget::TICK:
                return ResponseTickTarget(std::move(req));

            case RequestTarget::RECORDS:
                return ResponseRecordsTarget(std::move(req));
            
            case RequestTarget::UNKNOWN:
                return ResponseBadRequestApi(std::move(req), "badRequest", "Bad request");
            }
        }

    private:

        RequestTarget GetRequestTarget(std::string_view target);

        std::unordered_map<std::string, std::string> ParseURI(const std::string& query) const;

        template <typename Body, typename Allocator>
        HandlerResponse ResponseRecordsTarget(http::request<Body, http::basic_fields<Allocator>>&& req) {
            if (req.method() == http::verb::get) {
                const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                    return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
                    };

                std::unordered_map<std::string, std::string> params = ParseURI(std::string(req.target()));
                int start;
                int max_items;
                try {
                    if (params.count("start")) {
                        start = std::stoi(params.at("start"));
                    }

                    if (params.count("maxItems")) {
                        if (std::stoi(params.at("maxItems")) > 100) {
                            return ResponseBadRequestApi(std::move(req), "invalidArgument", "maxItems mast be under 100");
                        }
                        max_items = std::stoi(params.at("maxItems"));
                    }
                }
                catch (const std::exception& e ) {
                    return ResponseBadRequestApi(std::move(req), "invalidArgument", e.what());
                }

                std::string str_response = std::move(json::serialize(game_.GetRecords(max_items, start)));
                StringResponse result_response = json_response(http::status::ok, str_response, str_response.size());
                result_response.set(http::field::cache_control, "no-cache");

                return HandlerResponse(result_response);
            }
            else {
                return ResponseMethodNotAllowed(std::move(req), "invalidMethod", "Invalid method", "POST");
            }
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseTickTarget(http::request<Body, http::basic_fields<Allocator>>&& req) {
            if (game_.IsTickerInternal()) {
                return ResponseBadRequestApi(std::move(req), "badRequest", "Invalid endpoint");
            }
            if (req.method() == http::verb::post) {
                if (auto it = req.find(http::field::content_type); it == req.end() || it->value() != "application/json") {
                    return ResponseBadRequestApi(std::move(req), "invalidArgument", "Invalid content type");
                }

                json::value request;
                int64_t time_delta = 0;
                try {
                    request = json::parse(req.body());
                    if (request.as_object().find("timeDelta") == request.as_object().end()) {
                        return ResponseBadRequestApi(std::move(req), "invalidArgument", "Failed to parse tick request JSON");
                    }
                    time_delta = request.as_object().at("timeDelta").as_int64();
                }
                catch (const std::exception& e) {
                    return ResponseBadRequestApi(std::move(req), "invalidArgument", "Failed to parse tick request JSON");
                }

                game_.GameTick(time_delta);

                return ResponseOkAction(std::move(req));
            }
            else {
                return ResponseMethodNotAllowed(std::move(req), "invalidMethod", "Invalid method", "POST");
            }
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseActionTarget(http::request<Body, http::basic_fields<Allocator>>&& req) {
            if (req.method() == http::verb::post) {
                if (req.find(http::field::authorization) == req.end()) {
                    return ResponseUnauthorized(std::move(req), "invalidToken", "Authorization header is missing");
                }
                std::string token = std::string(req.at(http::field::authorization));

                if (std::string start = "Bearer "s; !token.starts_with(start) || token.size() != 39) {
                    return ResponseUnauthorized(std::move(req), "invalidToken", "Authorization header not correct");
                }

                if (auto it = req.find(http::field::content_type); it == req.end() || it->value() != "application/json") {
                    return ResponseBadRequestApi(std::move(req), "invalidArgument", "Invalid content type");
                }
                
                if (model::Player* player = game_.FindPlayerByToken(model::Token(token.substr(7))); player) {
                    return ResponseAction(std::move(req), player);

                }
                else {
                    return ResponseUnauthorized(std::move(req), "unknownToken", "Player token has not been found");
                }
            }
            else {
                return ResponseMethodNotAllowed(std::move(req), "invalidMethod", "Invalid method", "POST");
            }
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseAction(http::request<Body, http::basic_fields<Allocator>>&& req, model::Player* player) {
            if (req.method() == http::verb::post) {
                json::value request;
                std::string move;
                try {
                    request = json::parse(req.body());
                    if (request.as_object().find("move") == request.as_object().end() || request.as_object().at("move").as_string().size() > 1) {
                        return ResponseBadRequestApi(std::move(req), "invalidArgument", "Failed to parse action");
                    }
                    move = std::string(request.as_object().at("move").as_string());

                }
                catch (const std::exception& e) {
                    return ResponseBadRequestApi(std::move(req), "invalidArgument", "Failed to parse action");
                }

                if (move.empty()) {
                    player->SetStopDir();
                    return ResponseOkAction(std::move(req));
                }
                if (move[0] == 'L') {
                    player->SetLeftDir();
                    return ResponseOkAction(std::move(req));
                }
                if (move[0] == 'R') {
                    player->SetRightDir();
                    return ResponseOkAction(std::move(req));
                }
                if (move[0] == 'U') {
                    player->SetUpDir();
                    return ResponseOkAction(std::move(req));
                }
                if (move[0] == 'D') {
                    player->SetDownDir();
                    return ResponseOkAction(std::move(req));
                }

                return ResponseBadRequestApi(std::move(req), "invalidArgument", "Failed to parse action");
                    
            }
            else {
                return ResponseMethodNotAllowed(std::move(req), "invalidMethod", "Invalid method", "POST");
            }
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseOkAction(http::request<Body, http::basic_fields<Allocator>>&& req) {
            const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
                };
            json::object result;

            std::string str_response = std::move(json::serialize(result));
            StringResponse result_response = json_response(http::status::ok, str_response, str_response.size());
            result_response.set(http::field::cache_control, "no-cache");

            return HandlerResponse(result_response);
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseStateTarget(http::request<Body, http::basic_fields<Allocator>>&& req) {
            if (req.method() == http::verb::get || req.method() == http::verb::head) {
                if (req.find(http::field::authorization) == req.end()) {
                    return ResponseUnauthorized(std::move(req), "invalidToken", "Authorization header is missing");
                }
                std::string token = std::string(req.at(http::field::authorization));

                if (std::string start = "Bearer "s; !token.starts_with(start) || token.size() != 39) {
                    return ResponseUnauthorized(std::move(req), "invalidToken", "Authorization header not correct");
                }

                if (const model::Player* player = game_.FindPlayerByToken(model::Token(token.substr(7))); player) {
                    return ResponseState(std::move(req), player->GetSessionPtr());

                }
                else {
                    return ResponseUnauthorized(std::move(req), "unknownToken", "Player token has not been found");
                }
            }
            else {
                return ResponseMethodNotAllowed(std::move(req), "invalidMethod", "Invalid method", "GET, HEAD");
            }
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseState(http::request<Body, http::basic_fields<Allocator>>&& req, const model::GameSession* session_ptr) {
            const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
            };

            json::object players;

            for (const std::shared_ptr<model::Dog>& dog : session_ptr->GetDogs()) {

                json::object obj_to_player;

                json::array pos;
                pos.emplace_back(dog->GetPosition().x);
                pos.emplace_back(dog->GetPosition().y);
                obj_to_player.emplace("pos", pos);

                json::array speed;
                speed.emplace_back(dog->GetVelocity().x);
                speed.emplace_back(dog->GetVelocity().y);
                obj_to_player.emplace("speed", speed);

                switch (dog->GetDirect()) {

                case model::Direct::EAST:
                    obj_to_player.emplace("dir", "R");
                    break;

                case model::Direct::NORTH:
                    obj_to_player.emplace("dir", "U");
                    break;

                case model::Direct::SOUTH:
                    obj_to_player.emplace("dir", "D");
                    break;

                case model::Direct::WEST:
                    obj_to_player.emplace("dir", "L");
                    break;
                }

                json::array bags;

                model::Player* player_ptr = game_.FindByDogIdAndMapId(dog->GetId(), session_ptr->GetMapId());
                for (std::shared_ptr<model::Loot> loot : player_ptr->GetLootVector()) {
                    json::object item;
                    item.emplace("id", loot->GetLootId());
                    item.emplace("type", loot->GetLootType());
                    bags.emplace_back(item);
                }

                obj_to_player.emplace("bag", bags);

                obj_to_player.emplace("score", player_ptr->GetScore());

                players.emplace(std::to_string(dog->GetId()), obj_to_player);
            }

            json::object lost_objects;

            const std::vector<std::shared_ptr<model::Loot>>& loots = session_ptr->GetLootVector();
            for (int i = 0; i < loots.size(); ++i) {
                json::object info;
                info.emplace("type", loots.at(i)->GetLootType());

                json::array pos;
                pos.emplace_back(loots.at(i)->GetPosition().x);
                pos.emplace_back(loots.at(i)->GetPosition().y);
                info.emplace("pos", pos);

                lost_objects.emplace(std::to_string(i), info);
            }

            json::object result;
            result.emplace("players", players);
            result.emplace("lostObjects", lost_objects);

            std::string str_response = std::move(json::serialize(result));
            StringResponse result_response = json_response(http::status::ok, str_response, str_response.size());
            result_response.set(http::field::cache_control, "no-cache");

            return HandlerResponse(result_response);
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseJoinTarget(http::request<Body, http::basic_fields<Allocator>>&& req) {
            if (req.method() == http::verb::post) {
                json::value request;
                std::string dog_name;
                try {
                    request = json::parse(req.body());
                    if (request.as_object().find("userName") == request.as_object().end() || request.as_object().at("userName").as_string().empty()) {
                        return ResponseBadRequestApi(std::move(req), "invalidArgument", "Invalid name");
                    }
                    if (request.as_object().find("mapId") == request.as_object().end() || request.as_object().at("mapId").as_string().empty()) {
                        return ResponseBadRequestApi(std::move(req), "invalidArgument", "Invalid map");
                    }
                    model::Map::Id map_id(std::string(request.as_object().at("mapId").as_string()));
                    if (!game_.FindMap(map_id)) {
                        return ResponseMapNotFound(std::move(req));
                    }
                    dog_name = std::string(request.as_object().at("userName").as_string());
                }
                catch (const std::exception& e) {
                    return ResponseBadRequestApi(std::move(req), "invalidArgument", "Join game request parse error");
                }
                model::GameSession& session = game_.GetSession(model::Map::Id(std::string(request.as_object().at("mapId").as_string())));
                auto [token, player] = game_.AddPlayer(dog_name, &session);
                return ResponseJoin(std::move(req), *token, player.GetId());
            }
            else {
                return ResponseMethodNotAllowed(std::move(req), "invalidMethod", "Only POST method is expected", "POST");
            }

        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponsePlayersResponse(http::request<Body, http::basic_fields<Allocator>>&& req) {
            if (req.method() == http::verb::get || req.method() == http::verb::head) {
                if (req.find(http::field::authorization) == req.end()) {
                    return ResponseUnauthorized(std::move(req), "invalidToken", "Authorization header is missing");
                }
                std::string token = std::string(req.at(http::field::authorization));

                if (std::string start = "Bearer "s; !token.starts_with(start)) {
                    return ResponseUnauthorized(std::move(req), "invalidToken", "Authorization header not correct");
                }

                if (const model::Player* player = game_.FindPlayerByToken(model::Token(token.substr(7))); player) {
                    return ResponsePlayers(std::move(req));
                }
                else {
                    return ResponseUnauthorized(std::move(req), "unknownToken", "Player token has not been found");
                }
            }
            else {
                return ResponseMethodNotAllowed(std::move(req), "invalidMethod", "Invalid method", "GET, HEAD");
            }
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseMethodNotAllowed(http::request<Body, http::basic_fields<Allocator>>&& req,
            std::string code,
            std::string message,
            std::string allowed_methods) const {
            const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
            };

            json::object response;
            response.emplace("code", code);
            response.emplace("message", message);

            std::string str_response = std::move(json::serialize(response));
            StringResponse result_response = json_response(http::status::method_not_allowed, str_response, str_response.size());
            result_response.set(http::field::cache_control, "no-cache");
            result_response.set(http::field::allow, allowed_methods);

            return HandlerResponse(result_response);
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponsePlayers(http::request<Body, http::basic_fields<Allocator>>&& req) {
            const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
                };

            json::object object;
            for (const model::Player& player : game_.GetPlayers()) {
                json::object player_object;
                player_object.emplace("name", player.GetPetName());
                object.emplace(std::to_string(player.GetId()), player_object);
            }
            std::string str_response = std::move(json::serialize(object));
            StringResponse result_response = json_response(http::status::ok, str_response, str_response.size());
            result_response.set(http::field::cache_control, "no-cache");

            return HandlerResponse(result_response);
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseUnauthorized(http::request<Body, http::basic_fields<Allocator>>&& req, std::string code, std::string message) const {
            const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
                };
            json::object response;
            response.emplace("code", code);
            response.emplace("message", message);

            std::string str_response = std::move(json::serialize(response));
            StringResponse result_response = json_response(http::status::unauthorized, str_response, str_response.size());
            result_response.set(http::field::cache_control, "no-cache");

            return HandlerResponse(result_response);
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseMapNotFound(http::request<Body, http::basic_fields<Allocator>>&& req) const {
            const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
                };
            json::object response;
            response.emplace("code", "mapNotFound");
            response.emplace("message", "Map not found");

            std::string str_response = std::move(json::serialize(response));
            StringResponse result_response = json_response(http::status::not_found, str_response, str_response.size());
            result_response.set(http::field::cache_control, "no-cache");

            return HandlerResponse(result_response);
        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseJoin(http::request<Body, http::basic_fields<Allocator>>&& req, std::string token , std::uint64_t player_id) {
            const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
                };

            json::object object;
            object.emplace("authToken", token);
            object.emplace("playerId", player_id);
            std::string str_response = std::move(json::serialize(object));
            StringResponse result_response = json_response(http::status::ok, str_response, str_response.size());
            result_response.set(http::field::cache_control, "no-cache");

            return HandlerResponse(result_response);
        }

        void FillJsonMapData(json::object& map, const model::Map* map_ptr) const;

        template <typename Body, typename Allocator>
        HandlerResponse ResponseMaps(http::request<Body, http::basic_fields<Allocator>>&& req) const {
            if (req.method() == http::verb::get || req.method() == http::verb::head) {
                const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                    return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
                };

                const model::Game::Maps& maps = game_.GetMaps();
                json::array response;

                for (const model::Map& map : maps) {
                    json::object map_info;
                    map_info.emplace("id", *map.GetId());
                    map_info.emplace("name", map.GetName());
                    response.emplace_back(map_info);
                }
                std::string str_response = std::move(json::serialize(response));

                return HandlerResponse(json_response(http::status::ok, str_response, str_response.size()));;
            }
            else {
                return ResponseMethodNotAllowed(std::move(req), "invalidMethod", "Invalid method", "GET, HEAD");
            }

        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseMapsById(http::request<Body, http::basic_fields<Allocator>>&& req) const {
            if (req.method() == http::verb::get || req.method() == http::verb::head) {
                const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                    return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
                };

                model::Map::Id id(std::string(req.target().substr(13)));
                const model::Map* map_ptr = game_.FindMap(id);

                if (!map_ptr) {
		    return ResponseMapNotFound(std::move(req));
                }

                json::value response;
                json::object& map = response.emplace_object();

                map.emplace("id", *map_ptr->GetId());
                map.emplace("name", map_ptr->GetName());

                FillJsonMapData(map, map_ptr);

                map.emplace("lootTypes", game_.GetMapInfoJson(id));


                std::string str_response = std::move(json::serialize(response));
                return HandlerResponse(json_response(http::status::ok, str_response, str_response.size()));
            }
            else {
                return ResponseMethodNotAllowed(std::move(req), "invalidMethod", "Invalid method", "GET, HEAD");
            }

        }

        template <typename Body, typename Allocator>
        HandlerResponse ResponseBadRequestApi(http::request<Body, http::basic_fields<Allocator>>&& req, std::string code, std::string message) const {
            const auto json_response = [&req](http::status status, std::string_view text, size_t body_size) {
                return MakeStringResponse(status, text, body_size, req.version(), req.keep_alive(), ContentType::JSON);
                };
            json::object response;
            response.emplace("code", code);
            response.emplace("message", message);

            std::string str_response = std::move(json::serialize(response));        
            StringResponse result_response = json_response(http::status::bad_request, str_response, str_response.size());
            result_response.set(http::field::cache_control, "no-cache");

            return HandlerResponse(result_response);
        
        }

        model::Game& game_;
    };

    template<class SomeRequestHandler>
    class LoggingRequestHandler {

        std::shared_ptr<SomeRequestHandler> decorated_;

        static void LogRequest(std::string_view ip, std::string_view method, std::string_view target) {
            json::value request_data{ {"ip"s, ip}, {"URI"s, target}, {"method"s, method} };
            BOOST_LOG_TRIVIAL(info) << boost::log::add_value(logger::additional_data, request_data)
                << boost::log::add_value(logger::timestamp, boost::posix_time::microsec_clock::local_time())
                << "request received"sv;
        }

    public:
        explicit LoggingRequestHandler(std::shared_ptr<SomeRequestHandler> decorated)
            :decorated_(decorated) {}

        template <typename Body, typename Allocator, typename Send>
        void operator() (http::request<Body, http::basic_fields<Allocator>>&& req,
            Send&& send,
            boost::posix_time::ptime now,
            std::string ip) const {

            LogRequest(ip, http::to_string(req.method()), req.target());

            auto response_handle = [s = std::move(send), now](HandlerResponse response) {
                std::string content_type = "null"s;
                int result_code;

                if (std::holds_alternative<StringResponse>(response)) {
                    StringResponse resp = std::move(std::get<StringResponse>(response));
                    result_code = resp.result_int();
                    if (resp.find(http::field::content_type) != resp.end()) {
                        content_type = std::string(resp.at(http::field::content_type));
                    }
                    s(resp);
                }
                else if (std::holds_alternative<FileResponse>(response)) {
                    FileResponse resp = std::move(std::get<FileResponse>(response));
                    result_code = resp.result_int();
                    if (resp.find(http::field::content_type) != resp.end()) {
                        content_type = std::string(resp.at(http::field::content_type));
                    }
                    s(resp);
                }
                boost::posix_time::time_duration duration = boost::posix_time::microsec_clock::local_time() - now;
                json::value response_data{ {"response_time"s, duration.total_milliseconds()},
                    {"code"s, result_code},
                    {"content_type"s, content_type} };
                BOOST_LOG_TRIVIAL(info) << boost::log::add_value(logger::additional_data, response_data)
                    << boost::log::add_value(logger::timestamp, boost::posix_time::microsec_clock::local_time())
                    << "response sent"sv;
                };

            (*decorated_)(std::move(req), response_handle);

        }
    };

}  // namespace http_handler
