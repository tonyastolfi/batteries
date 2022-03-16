#include <batteries/url_parse.hpp>
//
#include <batteries/url_parse.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

static_assert(batt::CanBeEqCompared<batt::UrlParse>{}, "");

batt::StatusOr<batt::UrlParse> ok_value(const batt::UrlParse& parse)
{
    return {parse};
}

TEST(UrlParseTest, Examples)
{
    EXPECT_EQ(batt::parse_url(""),  //
              ok_value(batt::UrlParse{
                  .scheme = "",
                  .user = "",
                  .host = "",
                  .port = batt::None,
                  .path = "",
                  .query = "",
                  .fragment = "",
              }));

    EXPECT_EQ(batt::parse_url("ldap://[2001:db8::7/c=GB?objectClass?one").status(),
              batt::StatusCode::kInvalidArgument);

    EXPECT_EQ(batt::parse_url("https://www.server.net:NOTANUMBER/a/b/c/d?q=1&p=2#section").status(),
              batt::StatusCode::kInvalidArgument);

    EXPECT_EQ(batt::parse_url("/a/b/c/d"),  //
              ok_value(batt::UrlParse{
                  .scheme = "",
                  .user = "",
                  .host = "",
                  .port = batt::None,
                  .path = "/a/b/c/d",
                  .query = "",
                  .fragment = "",
              }));

    EXPECT_EQ(batt::parse_url("/a/b/c/d?q=1&p=2"),  //
              ok_value(batt::UrlParse{
                  .scheme = "",
                  .user = "",
                  .host = "",
                  .port = batt::None,
                  .path = "/a/b/c/d",
                  .query = "q=1&p=2",
                  .fragment = "",
              }));

    EXPECT_EQ(batt::parse_url("/a/b/c/d?q=1&p=2#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "",
                  .user = "",
                  .host = "",
                  .port = batt::None,
                  .path = "/a/b/c/d",
                  .query = "q=1&p=2",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("http:/a/b/c/d?q=1&p=2#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "http",
                  .user = "",
                  .host = "",
                  .port = batt::None,
                  .path = "/a/b/c/d",
                  .query = "q=1&p=2",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("file:/a/b/c/d?q=1&p=2#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "file",
                  .user = "",
                  .host = "",
                  .port = batt::None,
                  .path = "/a/b/c/d",
                  .query = "q=1&p=2",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("file:///a/b/c/d?q=1&p=2#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "file",
                  .user = "",
                  .host = "",
                  .port = batt::None,
                  .path = "/a/b/c/d",
                  .query = "q=1&p=2",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("https://www.server.net/a/b/c/d?q=1&p=2#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "https",
                  .user = "",
                  .host = "www.server.net",
                  .port = batt::None,
                  .path = "/a/b/c/d",
                  .query = "q=1&p=2",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("https://www.server.net/a/b/c/d#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "https",
                  .user = "",
                  .host = "www.server.net",
                  .port = batt::None,
                  .path = "/a/b/c/d",
                  .query = "",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("https://www.server.net/a/b/c/d?#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "https",
                  .user = "",
                  .host = "www.server.net",
                  .port = batt::None,
                  .path = "/a/b/c/d",
                  .query = "",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("https://1.2.3.4/a/b/c/d?#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "https",
                  .user = "",
                  .host = "1.2.3.4",
                  .port = batt::None,
                  .path = "/a/b/c/d",
                  .query = "",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("https://theplace:889/a/b/c/d?#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "https",
                  .user = "",
                  .host = "theplace",
                  .port = 889,
                  .path = "/a/b/c/d",
                  .query = "",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("https://myself@theplace:889/a/b/c/d?#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "https",
                  .user = "myself",
                  .host = "theplace",
                  .port = 889,
                  .path = "/a/b/c/d",
                  .query = "",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("https://myself@:889/a/b/c/d?#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "https",
                  .user = "myself",
                  .host = "",
                  .port = 889,
                  .path = "/a/b/c/d",
                  .query = "",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("https://myself@host.abc.xyz/#section"),  //
              ok_value(batt::UrlParse{
                  .scheme = "https",
                  .user = "myself",
                  .host = "host.abc.xyz",
                  .port = batt::None,
                  .path = "/",
                  .query = "",
                  .fragment = "section",
              }));

    EXPECT_EQ(batt::parse_url("https://myself@host.abc.xyz/?a=b"),  //
              ok_value(batt::UrlParse{
                  .scheme = "https",
                  .user = "myself",
                  .host = "host.abc.xyz",
                  .port = batt::None,
                  .path = "/",
                  .query = "a=b",
                  .fragment = "",
              }));

    EXPECT_EQ(batt::parse_url("ftp://ftp.is.co.za/rfc/rfc1808.txt"),  //
              ok_value(batt::UrlParse{
                  .scheme = "ftp",
                  .user = "",
                  .host = "ftp.is.co.za",
                  .port = batt::None,
                  .path = "/rfc/rfc1808.txt",
                  .query = "",
                  .fragment = "",
              }));

    EXPECT_EQ(batt::parse_url("http://www.ietf.org/rfc/rfc2396.txt"),  //
              ok_value(batt::UrlParse{
                  .scheme = "http",
                  .user = "",
                  .host = "www.ietf.org",
                  .port = batt::None,
                  .path = "/rfc/rfc2396.txt",
                  .query = "",
                  .fragment = "",
              }));

    EXPECT_EQ(batt::parse_url("ldap://[2001:db8::7]/c=GB?objectClass?one"),  //
              ok_value(batt::UrlParse{
                  .scheme = "ldap",
                  .user = "",
                  .host = "2001:db8::7",
                  .port = batt::None,
                  .path = "/c=GB",
                  .query = "objectClass?one",
                  .fragment = "",
              }));

    EXPECT_EQ(batt::parse_url("mailto:John.Doe@example.com"),  //
              ok_value(batt::UrlParse{
                  .scheme = "mailto",
                  .user = "",
                  .host = "",
                  .port = batt::None,
                  .path = "John.Doe@example.com",
                  .query = "",
                  .fragment = "",
              }));

    EXPECT_EQ(batt::parse_url("http://a/b/c/d;p?q"),  //
              ok_value(batt::UrlParse{
                  .scheme = "http",
                  .user = "",
                  .host = "a",
                  .port = batt::None,
                  .path = "/b/c/d;p",
                  .query = "q",
                  .fragment = "",
              }));

    EXPECT_EQ(batt::parse_url("/g"),  //
              ok_value(batt::UrlParse{
                  .scheme = "",
                  .user = "",
                  .host = "",
                  .port = batt::None,
                  .path = "/g",
                  .query = "",
                  .fragment = "",
              }));
}

}  // namespace
