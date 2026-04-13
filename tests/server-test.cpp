#include <fstream>
#include <gtest/gtest.h>
#include <hc/mock/mock-client.h>
#include <hc/server.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <pqxx/pqxx>
#include <spdlog/spdlog.h>
#include <sstream>

#ifndef HCRE_TEST_DB
#error [dev] HCHCRE_TEST_DB not defined, should be defined in CMakeLists.txt
#endif

using namespace hc::mock;

using httplib::StatusCode;

class TestDB {
  public:
    TestDB() : connection_(HCRE_TEST_DB
#ifdef HCRE_TEST_DB_PASSWORD
		" password=" + std::string(HCRE_TEST_DB_PASSWORD)
#endif // HCRE_TEST_DB_PASSWORD
    )
    {
        // TODO(ShelpAm): consider creating test database via pqxx.
        // pqxx::nontransaction ntx(connection_);
        // ntx.exec("CREATE DATABASE hcre_test;");
        // ntx.commit(); // For nontransaction, this is a no-op.
        pqxx::work tx(connection_);
        tx.exec(file_content("scripts/table.sql"));
        tx.commit();
    }
    TestDB(TestDB const &) = delete;
    TestDB(TestDB &&) = delete;
    TestDB &operator=(TestDB const &) = delete;
    TestDB &operator=(TestDB &&) = delete;
    ~TestDB()
    {
        pqxx::work tx(connection_);
        tx.exec(file_content("scripts/clean.sql"));
        tx.commit();
    }

    [[nodiscard]] pqxx::connection &connection()
    {
        return connection_;
    }

  private:
    static std::string file_content(std::string const &filename)
    {
        std::ifstream ifs(filename);
        if (!ifs.is_open()) {
            throw std::runtime_error{
                std::format("cannot access '{}': No such file", filename)};
        }
        std::stringstream ss;
        ss << ifs.rdbuf();
        return ss.str();
    }

    pqxx::connection connection_;
};

class ServerTest : public testing::Test {
  protected:
    ServerTest() : c_("localhost", 10010), s_(make_config())
    {
        using namespace std::chrono_literals;
        c_.set_max_timeout(3s);
        s_.start("localhost", 10010);
    }

    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    httplib::Client c_;

  private:
    sqlpp::postgresql::connection_config make_config()
    {
        auto config = sqlpp::postgresql::connection_config{};
        // Under Windows this breaks. We should write specialized code for
        // different platform. And we may need to create test db instead of
        // using production db.
        config.host = testdb_.connection().hostname();
        config.dbname = testdb_.connection().dbname();
        config.user = testdb_.connection().username();
#ifdef HCRE_TEST_DB_PASSWORD
        config.password = HCRE_TEST_DB_PASSWORD;
#endif // HCRE_TEST_DB_PASSWORD
        return config;
    }

    // Should not change the order because initialization of s_ depends on
    // testdb_.
    TestDB testdb_;

  protected:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    Server s_;
};

TEST_F(ServerTest, ConnectivityTest)
{
    auto r = c_.Get("/hi");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->body, "Hello World!");
}

TEST_F(ServerTest, WrongPath)
{
    auto r = c_.Get("/api/assignments/");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->status, httplib::StatusCode::NotFound_404);
}

TEST_F(ServerTest, Valid)
{
    auto r = c_.Get("/api/assignments");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->status, httplib::StatusCode::OK_200);
}

TEST_F(ServerTest, AddAssignment)
{
    auto const *const body = R"({
        "name": "Test Assignment",
        "start_time": "2025-11-26T00:00:00Z",
        "end_time": "2025-11-26T00:00:00Z",
        "submissions": {}
    })";
    auto r = c_.Post("/api/assignments/add", body, "application/json");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->status, httplib::StatusCode::OK_200);
    r = c_.Post("/api/assignments/add", body, "application/json");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->status, httplib::StatusCode::BadRequest_400); // Already exists
}

TEST_F(ServerTest, AddStudent)
{
    auto const *const body = R"({
        "student_id": "202326202022",
        "name": "刘家福"
    })";
    auto r = c_.Post("/api/students/add", body, "application/json");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->status, httplib::StatusCode::OK_200);
}

TEST_F(ServerTest, SubmitToAssignment)
{
    successfully_add_assignment_testassignmentinfinite(c_);

    {
        auto const *const empty_body = R"()";
        auto r =
            c_.Post("/api/assignments/submit", empty_body, "application/json");
        ASSERT_TRUE(r);
        EXPECT_EQ(r->status, httplib::StatusCode::BadRequest_400);

        auto const *const normal_body = R"({
            "student_id": "202326202022",
            "student_name": "刘家福",
            "assignment_name": "Test Assignment",
            "file": {
                "filename": "ljf sb",
                "content": "U0IgTEpG"
            }
        })";
        r = c_.Post("/api/assignments/submit", normal_body, "application/json");
        ASSERT_TRUE(r);
        // Time out of bound
        EXPECT_EQ(r->status, httplib::StatusCode::BadRequest_400);
    }

    {
        auto const *const empty_body = R"()";
        auto r =
            c_.Post("/api/assignments/submit", empty_body, "application/json");
        ASSERT_TRUE(r);
        EXPECT_EQ(r->status, httplib::StatusCode::BadRequest_400);

        auto const *const normal_body = R"({
            "student_id": "202326202022",
            "student_name": "刘家福",
            "assignment_name": "Test Assignment Infinite",
            "file": {
                "filename": "ljf sb",
                "content": "U0IgTEpG"
            }
        })";
        r = c_.Post("/api/assignments/submit", normal_body, "application/json");
        ASSERT_TRUE(r);
        // No such student
        EXPECT_EQ(r->status, httplib::StatusCode::BadRequest_400);
    }

    successfully_add_student_ljf(c_);

    {
        auto const *const empty_body = R"()";
        auto r =
            c_.Post("/api/assignments/submit", empty_body, "application/json");
        ASSERT_TRUE(r);
        EXPECT_EQ(r->status, httplib::StatusCode::BadRequest_400);
    }

    ljf_successfully_submit_to_testassignmentinfinite(c_);
}

TEST_F(ServerTest, Export)
{
    successfully_add_assignment_testassignmentinfinite(c_);
    successfully_add_student_ljf(c_);
    ljf_successfully_submit_to_testassignmentinfinite(c_);

    {
        auto const *const empty_body = R"()";
        auto r =
            c_.Post("/api/assignments/export", empty_body, "application/json");
        ASSERT_TRUE(r);
        EXPECT_EQ(r->status, httplib::StatusCode::BadRequest_400);
    }

    {
        auto const *const body = R"({
            "assignment_name": "Test Assignment Infinite"
        })";
        auto r = c_.Post("/api/assignments/export", body, "application/json");
        ASSERT_TRUE(r);
        EXPECT_EQ(r->status, StatusCode::OK_200);

        {
            auto const j = nlohmann::json::parse(r->body);
            auto res = j.get<AssignmentsExportResult>();
            auto download = c_.Get(res.exported_uri);
            ASSERT_TRUE(download);
            EXPECT_EQ(download->status, StatusCode::OK_200);
        }
    }
}

TEST_F(ServerTest, Stop)
{
    successfully_hi(c_);
    auto const r = c_.Post("/api/stop");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->status, httplib::StatusCode::OK_200);
    s_.wait_until_stopped(); // MUST be kept. Otherwise, there could be a race
                             // condition.
    hi_unreachable(c_);
}

TEST_F(ServerTest, DISABLED_AdminLogin)
{
    throw std::runtime_error{"Unimplemented"};
}

TEST_F(ServerTest, DISABLED_AdminVerification)
{
    throw std::runtime_error{"Unimplemented"};
}

int main(int argc, char **argv)
{
    spdlog::set_level(spdlog::level::trace); // Toggle when debugging
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
