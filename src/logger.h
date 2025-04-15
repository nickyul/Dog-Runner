#pragma once
#include <boost/log/trivial.hpp>     // для BOOST_LOG_TRIVIAL
#include <boost/log/core.hpp>        // для logging::core
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/date_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/json.hpp>

#include "sdk.h"
#include <string_view>



namespace logger {

    using namespace std::literals;
    namespace logging = boost::log;
    namespace keywords = boost::log::keywords;
    namespace sinks = boost::log::sinks;
    namespace json = boost::json;

    BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value);
    BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

    void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm);

    void InitBoostLog();


}//namespace logger


