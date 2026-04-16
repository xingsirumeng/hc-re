#include <CLI/CLI.hpp>
#include <hc/config.h>
#include <hc/server.h>
#include <hc/version.h>
#include <hc/xdg-basedir.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <print>
#include <spdlog/spdlog.h>
#include <sqlpp23/postgresql/postgresql.h>
#include <sqlpp23/sqlpp23.h>

int main(int argc, char **argv)
{
    try {
        using config::datahome;
        using config::verbose;
        auto version = false;
        auto ask = false;
        auto port = std::uint16_t{8080};

        CLI::App app("homework-collection-remastered", "hc");
        app.add_flag("-V,--version", version, "Print hc version and exit");
        app.add_flag("-v,--verbose", verbose(), "Use debug mode");
        app.add_flag("--ask", ask,
                     "Ask username and password for database connection");
        app.add_option("-p,--port", port, "Port of the web server");
        CLI11_PARSE(app, argc, argv);

        spdlog::set_level(verbose() ? spdlog::level::debug
                                    : spdlog::level::info);
        spdlog::debug("datahome={}", datahome().string());
        spdlog::debug("verbose={}", verbose());
        spdlog::debug("version={}", version);
        spdlog::debug("port={}", port);

        if (version) {
            std::println("hc version {}", HCRE_VERSION);
            return 0;
        }

        // Create a connection configuration.
        auto config = sqlpp::postgresql::connection_config{};
        config.host = "localhost";
        config.dbname = "hc";
        config.user = "postgres";
        if (ask) {
            std::print("Input your db username: ");
            std::getline(std::cin, config.user);
            std::print("Input your db password: ");
            std::getline(std::cin, config.password);
        }

        Server server(config);
        server.start("127.0.0.1", port);

        using namespace std::chrono_literals;
        // Blocks until something is triggered (such as shutdown command).
        while (server.is_running()) {
            std::this_thread::sleep_for(10ms); // Gives out CPU
        }
    }
    catch (std::exception const &e) {
        spdlog::critical("Fatal error: {}", e.what());
    }
}
