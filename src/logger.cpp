#include "logger.h"

namespace logger {

    void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {

        strm << json::serialize(json::object{
            {"timestamp", to_iso_extended_string(*rec[timestamp])},
            {"data", *rec[additional_data]},
            {"message", json::string{*rec[logging::expressions::smessage]}}
            });
    }

    void InitBoostLog() {
        logging::add_console_log(
            std::cout,
            keywords::format = &MyFormatter,
            keywords::auto_flush = true
        );
    }




}//namespace logger

