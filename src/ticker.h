#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/steady_timer.hpp>

#include <memory>

namespace net = boost::asio;
namespace sys = boost::system;

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // ������� handler ����� ���������� ������ strand � ���������� period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler);

    void Start();

private:
    void ScheduleTick();

    void OnTick(sys::error_code ec);

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{ strand_ };
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};