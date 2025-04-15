#include "request_handler.h"

namespace http_handler {

    StringResponse MakeStringResponse(http::status status, std::string_view body, size_t body_size, unsigned http_version, bool keep_alive, std::string_view content_type) {

        StringResponse response(status, http_version);
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.content_length(body_size);
        response.keep_alive(keep_alive);
        return response;
    }

    RequestTarget ApiHandler::GetRequestTarget(std::string_view target) {
        if (target == "/api/v1/game/join"sv) {
            return RequestTarget::JOIN;
        }
        else if (target == "/api/v1/maps"sv) {
            return RequestTarget::MAPS;
        }
        else if (target.substr(0, 13) == "/api/v1/maps/"sv) {
            return RequestTarget::MAP;
        }
        else if (target == "/api/v1/game/players"sv) {
            return RequestTarget::PLAYERS;
        }
        else if (target == "/api/v1/game/state"sv) {
            return RequestTarget::STATE;
        }
        else if (target == "/api/v1/game/player/action"sv) {
            return RequestTarget::ACTION;
        }
        else if (target == "/api/v1/game/tick"sv) {
            return RequestTarget::TICK;
        }
        else if (target.substr(0,20) == "/api/v1/game/records"sv) {
            return RequestTarget::RECORDS;
        }
        return RequestTarget::UNKNOWN;
    }

    std::unordered_map<std::string, std::string> ApiHandler::ParseURI(const std::string& query) const {
        std::unordered_map<std::string, std::string> params;
        size_t start = query.find('?') + 1;
        while (start < query.size()) {
            size_t eq_pos = query.find('=', start);
            if (eq_pos == std::string::npos) {
                break;
            }
            size_t amp_pos = query.find('&', eq_pos);
            if (amp_pos == std::string::npos) {
                amp_pos = query.size();
            }
            std::string key = query.substr(start, eq_pos - start);
            std::string value = query.substr(eq_pos + 1, amp_pos - eq_pos - 1);
            params[key] = value;

            start = amp_pos + 1;
        }
        return params;
    }

    void ApiHandler::FillJsonMapData(json::object& map, const model::Map* map_ptr) const {

        json::array roads;

        for (const model::Road& road : map_ptr->GetRoads()) {
            json::object road_map;
            road_map.emplace("x0", road.GetStart().x);
            road_map.emplace("y0", road.GetStart().y);
            if (road.IsHorizontal()) {
                road_map.emplace("x1", road.GetEnd().x);
            }
            else if (road.IsVertical()) {
                road_map.emplace("y1", road.GetEnd().y);
            }
            roads.emplace_back(road_map);
        }
        map.emplace("roads", roads);

        json::array builds;
        for (const model::Building& build : map_ptr->GetBuildings()) {
            json::object build_map;
            model::Rectangle build_data = build.GetBounds();
            build_map.emplace("x", build_data.position.x);
            build_map.emplace("y", build_data.position.y);
            build_map.emplace("w", build_data.size.width);
            build_map.emplace("h", build_data.size.height);
            builds.emplace_back(build_map);
        }
        map.emplace("buildings", builds);

        json::array offices;
        for (const model::Office& office : map_ptr->GetOffices()) {
            json::object office_map;
            office_map.emplace("id", *office.GetId());
            office_map.emplace("x", office.GetPosition().x);
            office_map.emplace("y", office.GetPosition().y);
            office_map.emplace("offsetX", office.GetOffset().dx);
            office_map.emplace("offsetY", office.GetOffset().dy);
            offices.emplace_back(office_map);
        }
        map.emplace("offices", offices);
    }

    std::string RequestHandler::DecodeUrl(beast::string_view target) const {
        std::string coded_target(target);
        std::string decoded_target(static_path_.string());

        for (int i = 0; i < coded_target.size(); ++i) {
            if (coded_target[i] != '%') {
                decoded_target += coded_target[i];
            }
            else {
                if (i + 2 < coded_target.size()) {
                    std::string hex_code = coded_target.substr(i + 1, 2);
                    int ch = std::stoi(hex_code, 0, 16);
                    decoded_target += static_cast<char>(ch);
                    i += 2;
                }
                else {
                    decoded_target += coded_target[i];
                }
            }
        }
        return decoded_target;
    }

    bool RequestHandler::IsSubPath(fs::path path) const {
        // Приводим оба пути к каноничному виду (без . и ..)
        path = fs::weakly_canonical(path);
        fs::path base = fs::weakly_canonical(fs::path(static_path_));
        // Проверяем, что все компоненты base содержатся внутри path
        for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
            if (p == path.end() || *p != *b) {
                return false;
            }
        }
        return true;
    }

    void RequestHandler::AddContentType(http::response<http::file_body>& res, fs::path& path) const {
        if (path == fs::weakly_canonical(static_path_)) {
            res.insert(http::field::content_type, ContentType::TEXT_HTML);
            return;
        }
        const auto lower_extension = [](const fs::path& path) {
            std::string result;
            for (char ch : path.extension().string()) {
                result += std::tolower(ch);
            }
            return result;
            };
        const std::string extension(lower_extension(path));

        if (extension == ".htm"s || extension == ".html"s) {
            res.insert(http::field::content_type, ContentType::TEXT_HTML);
            return;
        }
        if (extension == ".css"s) {
            res.insert(http::field::content_type, ContentType::CSS);
            return;
        }
        if (extension == ".txt"s) {
            res.insert(http::field::content_type, ContentType::TEXT_PLAIN);
            return;
        }
        if (extension == ".js"s) {
            res.insert(http::field::content_type, ContentType::JS);
            return;
        }
        if (extension == ".json"s) {
            res.insert(http::field::content_type, ContentType::JSON);
            return;
        }
        if (extension == ".xml"s) {
            res.insert(http::field::content_type, ContentType::XML);
            return;
        }
        if (extension == ".png"s) {
            res.insert(http::field::content_type, ContentType::PNG);
            return;
        }
        if (extension == ".jpg"s ||
            extension == ".jpe"s ||
            extension == ".jpep"s) {
            res.insert(http::field::content_type, ContentType::JPEG);
            return;
        }
        if (extension == ".gif"s) {
            res.insert(http::field::content_type, ContentType::GIF);
            return;
        }
        if (extension == ".bmp"s) {
            res.insert(http::field::content_type, ContentType::BMP);
            return;
        }
        if (extension == ".ico"s) {
            res.insert(http::field::content_type, ContentType::ICO);
            return;
        }
        if (extension == ".tiff"s || extension == ".tif"s) {
            res.insert(http::field::content_type, ContentType::TIFF);
            return;
        }
        if (extension == ".svg"s || extension == ".svgz"s) {
            res.insert(http::field::content_type, ContentType::SVG);
            return;
        }
        if (extension == ".mp3"s) {
            res.insert(http::field::content_type, ContentType::MP3);
            return;
        }
        res.insert(http::field::content_type, ContentType::OCTET_STREAM);
    }


}  // namespace http_handler
