#pragma once
#include <boost/json.hpp>

#include <pqxx/pqxx>

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string>

#include "model.h"
#include "tagged_uuid.h"

using namespace std::literals;
// libpqxx ���������� zero-terminated ���������� �������� ����� "abc"_zv;
using pqxx::operator"" _zv;

namespace json = boost::json;

namespace postgre {

    class ConnectionPool {
        using PoolType = ConnectionPool;
        using ConnectionPtr = std::shared_ptr<pqxx::connection>;

    public:
        class ConnectionWrapper {
        public:
            ConnectionWrapper(std::shared_ptr<pqxx::connection>&& conn, PoolType& pool) noexcept
                : conn_{ std::move(conn) }
                , pool_{ &pool } {
            }

            ConnectionWrapper(const ConnectionWrapper&) = delete;
            ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

            ConnectionWrapper(ConnectionWrapper&&) = default;
            ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

            pqxx::connection& operator*() const& noexcept {
                return *conn_;
            }
            pqxx::connection& operator*() const&& = delete;

            pqxx::connection* operator->() const& noexcept {
                return conn_.get();
            }

            ~ConnectionWrapper() {
                if (conn_) {
                    pool_->ReturnConnection(std::move(conn_));
                }
            }

        private:
            std::shared_ptr<pqxx::connection> conn_;
            PoolType* pool_;
        };

        // ConnectionFactory is a functional object returning std::shared_ptr<pqxx::connection>
        template <typename ConnectionFactory>
        ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory) {
            pool_.reserve(capacity);
            for (size_t i = 0; i < capacity; ++i) {
                pool_.emplace_back(connection_factory());
            }
        }

        ConnectionWrapper GetConnection();

    private:
        void ReturnConnection(ConnectionPtr&& conn);

        std::mutex mutex_;
        std::condition_variable cond_var_;
        std::vector<ConnectionPtr> pool_;
        size_t used_connections_ = 0;
    };


    class DatabaseImpl : public model::Database {
    public:
        DatabaseImpl(size_t num_threads, const char* db_url);

        DatabaseImpl(const DatabaseImpl&) = delete;
        DatabaseImpl& operator=(const DatabaseImpl&) = delete;

        void SaveRecord(std::string name, int score, uint64_t played_time) override;

        json::array GetRecords(int limit, int offset) override;

    private:

        ConnectionPool conn_pool_;
    };


} //namespace postgre

