#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>

#include "ticker.h"
#include "logger.h"
#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "model_serialization.h"
#include "postgresql.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace http = boost::beast::http;
namespace json = boost::json;
using Strand = net::strand<net::io_context::executor_type>;

constexpr const char DB_URL[] = "GAME_DB_URL";

namespace {
// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

struct Args {
    std::string config_file_path;
    std::string static_root;
    std::string state_file_path;
    unsigned int tick_period = 0;
    unsigned int state_period = 0;
    bool random_spawn = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;
    po::options_description desc{ "Allowed options:" };
    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<unsigned int>(&args.tick_period)->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value(&args.config_file_path)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.static_root)->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", po::value<bool>(&args.random_spawn), "spawn dogs at random position")
        ("state-file", po::value(&args.state_file_path)->value_name("file"s), "set state file path")
        ("save-state-period", po::value<unsigned int>(&args.state_period)->value_name("milliseconds"s), "set save state period");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }

    if (vm.contains("config-file") && vm.contains("www-root")) {
        return args;
    }
    else {
        throw std::runtime_error(R"(Usage: game_server --tick-period[int, optional] 
                                 --config-file <game-config-json> 
                                 --www-root <dir-to-content> 
                                 --randomize-spawn-points[bool, optional]
                                 --state-file <dir-to-file>
                                 --save-state-period[int])");
    }
    return std::nullopt;
}


int main(int argc, const char* argv[]) {
    Args command_line_args;
    try {
        if (auto args = ParseCommandLine(argc, argv)) {
            command_line_args = *args;
        }
        else {
            return EXIT_FAILURE;
        }
    }

    catch (const std::exception& ex) {
        std::cout << "Failed parsing command line arguments" << std::endl;
        return EXIT_FAILURE;
    }

    // Инициализация логгера
    logger::InitBoostLog();
    try {
        
        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(command_line_args.config_file_path);
        
        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        
        const char* db_url = std::getenv(DB_URL);
        if (!db_url) {
            throw std::runtime_error("DB URL is not specified");
        }

        std::shared_ptr<postgre::DatabaseImpl> db_ptr(std::make_shared<postgre::DatabaseImpl>(num_threads, db_url));

        game.SetDb(db_ptr);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
            });

        std::filesystem::path static_path{ command_line_args.static_root };
        
        Strand strand = net::make_strand(ioc);
        
        if (command_line_args.tick_period > 0) {
            game.SetInternalTicker();
            auto ticker = std::make_shared<Ticker>(strand, std::chrono::milliseconds(command_line_args.tick_period), [&game](std::chrono::milliseconds delta) {
                game.GameTick(delta.count()); }
            );
            ticker->Start();
        }
        
        if (command_line_args.random_spawn) {
            game.SetRandomSpawnTrue();
        }


        serialization::SerializingListener listener(std::chrono::milliseconds(command_line_args.state_period), game, command_line_args.state_file_path);

        if (!command_line_args.state_file_path.empty()) {
            game.SetApplicationListener(&listener);
            listener.RestoreGame(game);
        }

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        auto handler = std::make_shared<http_handler::RequestHandler>(
            game, command_line_args.static_root, strand);

        http_handler::LoggingRequestHandler logging_handler{ handler };

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, { address, port }, [&logging_handler](auto&& req, auto&& send, boost::posix_time::ptime time, std::string ip) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send), time, ip);
        });
        
        // Эта надпись сообщает //тестам// о том, что сервер запущен и готов обрабатывать запросы
        json::value server_start{ {"port"s, port}, {"address"s, address.to_string()}};
        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(logger::additional_data, server_start)
            << boost::log::add_value(logger::timestamp, boost::posix_time::microsec_clock::local_time())
            << "server started"sv;

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        if (!command_line_args.state_file_path.empty()) {
            listener.SaveStateGame();
        }

    } catch (const std::exception& ex) {
        json::value error_data{ {"code"s, "EXIT_FAILURE"s}, {"exception", ex.what()}};
        BOOST_LOG_TRIVIAL(error) << boost::log::add_value(logger::additional_data, error_data)
            << boost::log::add_value(logger::timestamp, boost::posix_time::microsec_clock::local_time())
            << "server exited"sv;

        return EXIT_FAILURE;
    }

    json::value close_data{ {"code"s, "0"s} };
    BOOST_LOG_TRIVIAL(error) << boost::log::add_value(logger::additional_data, close_data)
        << boost::log::add_value(logger::timestamp, boost::posix_time::microsec_clock::local_time())
        << "server exited"sv;
    return EXIT_SUCCESS;
}
