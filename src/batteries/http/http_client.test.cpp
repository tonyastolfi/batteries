//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/http/http_client.hpp>
//
#include <batteries/http/http_client.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

constexpr bool kInteractiveTesting = false;

TEST(HttpClientTest, Test)
{
    batt::StatusOr<std::unique_ptr<batt::HttpResponse>> result =
        batt::http_get("http://www.google.com/", batt::HttpHeader{"Connection", "close"});

    EXPECT_TRUE(result.ok());

    batt::StatusOr<batt::HttpResponse::Message&> response_message = result->get()->await_message();
    ASSERT_TRUE(response_message.ok());

    if (kInteractiveTesting) {
        std::cout << *response_message << std::endl;
    }

    batt::StatusOr<batt::HttpData&> response_data = result->get()->await_data();
    ASSERT_TRUE(response_data.ok());

    if (kInteractiveTesting) {
        batt::Status status = *response_data | batt::seq::print_out(std::cout);
        ASSERT_TRUE(status.ok());
    }
}

}  // namespace
