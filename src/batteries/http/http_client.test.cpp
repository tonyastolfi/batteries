#include <batteries/http/http_client.hpp>
//
#include <batteries/http/http_client.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

TEST(HttpClientTest, Test)
{
    // auto result = batt::http_get("http://www.google.com/", batt::HttpHeader{"Connection", "close"});
    auto result = batt::http_post("http://localhost:9995/a/b/c",            //
                                  batt::HttpHeader{"Connection", "close"},  //
                                  batt::HttpData{std::string_view{"My Request!"}});
    EXPECT_FALSE(result.ok());
}

}  // namespace
