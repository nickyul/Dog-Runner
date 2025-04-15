#include "postgresql.h"

namespace postgre {

    ConnectionPool::ConnectionWrapper ConnectionPool::GetConnection() {
        std::unique_lock lock{ mutex_ };
        // Ѕлокируем текущий поток и ждЄм, пока cond_var_ не получит уведомление и не освободитс€
        // хот€ бы одно соединение
        cond_var_.wait(lock, [this] {
            return used_connections_ < pool_.size();
            });
        // ѕосле выхода из цикла ожидани€ мьютекс остаЄтс€ захваченным

        return { std::move(pool_[used_connections_++]), *this };
    }

    void ConnectionPool::ReturnConnection(ConnectionPtr&& conn) {
        // ¬озвращаем соединение обратно в пул
        {
            std::lock_guard lock{ mutex_ };
            assert(used_connections_ != 0);
            pool_[--used_connections_] = std::move(conn);
        }
        // ”ведомл€ем один из ожидающих потоков об изменении состо€ни€ пула
        cond_var_.notify_one();
    }

    DatabaseImpl::DatabaseImpl(size_t num_threads, const char* db_url)
        : conn_pool_(num_threads, [db_url] { return std::make_shared<pqxx::connection>(db_url); }) {
        ConnectionPool::ConnectionWrapper conn = conn_pool_.GetConnection();

        pqxx::work w(*conn);
        w.exec(
            "CREATE TABLE IF NOT EXISTS retired_players (id UUID CONSTRAINT player_id PRIMARY KEY, "
            "name varchar(100) NOT NULL, "
            "score integer, "
            "play_time_ms integer);"_zv);

        w.exec(
            "CREATE INDEX IF NOT EXISTS record_players ON retired_players (score DESC, play_time_ms, name);"_zv);

        w.commit();
    }

    void DatabaseImpl::SaveRecord(std::string name, int score, uint64_t played_time) {
        ConnectionPool::ConnectionWrapper conn = conn_pool_.GetConnection();
        pqxx::work w(*conn);

        util::detail::UUIDType uuid_id = util::detail::NewUUID();
        std::string id = util::detail::UUIDToString(uuid_id);

        w.exec_params(R"(
                INSERT INTO retired_players (id, name, score, play_time_ms) VALUES ($1, $2, $3, $4);)"_zv,
            id,
            name,
            score,
            played_time);
        w.commit();
    }

    json::array DatabaseImpl::GetRecords(int limit, int offset) {
        ConnectionPool::ConnectionWrapper conn = conn_pool_.GetConnection();
        pqxx::read_transaction r(*conn);
        pqxx::result query_result = r.exec_params(R"(
            SELECT name, score, play_time_ms FROM retired_players ORDER BY score DESC, play_time_ms, name LIMIT $1 OFFSET $2; )"_zv,
            limit, offset);

        json::array result;

        for (const pqxx::row& res_row : query_result) {
            json::object json_row;
            json_row.emplace("name", res_row.at("name"s).as<std::string>());
            json_row.emplace("score", res_row.at("score"s).as<int>());
            json_row.emplace("playTime", res_row["play_time_ms"].as<double>() / 1000.0);
            
            result.emplace_back(json_row);
        }
        return result;
    }
}