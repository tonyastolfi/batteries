#include <batteries/pico_http/parser.hpp>
//
#include <batteries/pico_http/parser.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/assert.hpp>
#include <batteries/stream_util.hpp>

#include <string>

namespace {

using namespace batt::int_types;

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
const std::string kFirefoxRequest = R"http(GET / HTTP/1.1
Host: localhost:19991
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8
Accept-Language: en-US,en;q=0.5
Accept-Encoding: gzip, deflate
Connection: keep-alive
Upgrade-Insecure-Requests: 1
Sec-Fetch-Dest: document
Sec-Fetch-Mode: navigate
Sec-Fetch-Site: none
Sec-Fetch-User: ?1

)http";

TEST(PicoHttpParserTest, FirefoxRequestOk)
{
    pico_http::Request request;
    int result = request.parse(kFirefoxRequest.data(), kFirefoxRequest.size());
    EXPECT_EQ(result, (int)kFirefoxRequest.size());

    EXPECT_THAT(request.method, ::testing::StrEq("GET"));
    EXPECT_THAT(request.path, ::testing::StrEq("/"));
    EXPECT_EQ(request.major_version, 1);
    EXPECT_EQ(request.minor_version, 1);

    ASSERT_EQ(request.headers.size(), 11u);

    EXPECT_THAT(request.headers[0].name, ::testing::StrEq("Host"));
    EXPECT_THAT(request.headers[0].value, ::testing::StrEq("localhost:19991"));

    EXPECT_THAT(request.headers[1].name, ::testing::StrEq("User-Agent"));
    EXPECT_THAT(request.headers[1].value,
                ::testing::StrEq(
                    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0"));

    EXPECT_THAT(request.headers[2].name, ::testing::StrEq("Accept"));
    EXPECT_THAT(request.headers[2].value,
                ::testing::StrEq(
                    "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8"));

    EXPECT_THAT(request.headers[3].name, ::testing::StrEq("Accept-Language"));
    EXPECT_THAT(request.headers[3].value, ::testing::StrEq("en-US,en;q=0.5"));

    EXPECT_THAT(request.headers[4].name, ::testing::StrEq("Accept-Encoding"));
    EXPECT_THAT(request.headers[4].value, ::testing::StrEq("gzip, deflate"));

    EXPECT_THAT(request.headers[5].name, ::testing::StrEq("Connection"));
    EXPECT_THAT(request.headers[5].value, ::testing::StrEq("keep-alive"));

    EXPECT_THAT(request.headers[6].name, ::testing::StrEq("Upgrade-Insecure-Requests"));
    EXPECT_THAT(request.headers[6].value, ::testing::StrEq("1"));

    EXPECT_THAT(request.headers[7].name, ::testing::StrEq("Sec-Fetch-Dest"));
    EXPECT_THAT(request.headers[7].value, ::testing::StrEq("document"));

    EXPECT_THAT(request.headers[8].name, ::testing::StrEq("Sec-Fetch-Mode"));
    EXPECT_THAT(request.headers[8].value, ::testing::StrEq("navigate"));

    EXPECT_THAT(request.headers[9].name, ::testing::StrEq("Sec-Fetch-Site"));
    EXPECT_THAT(request.headers[9].value, ::testing::StrEq("none"));

    EXPECT_THAT(request.headers[10].name, ::testing::StrEq("Sec-Fetch-User"));
    EXPECT_THAT(request.headers[10].value, ::testing::StrEq("?1"));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
const std::string kChromeRequest = R"http(GET /foo/bar/grill.html HTTP/1.1
Host: localhost:19991
Connection: keep-alive
sec-ch-ua: " Not A;Brand";v="99", "Chromium";v="99", "Google Chrome";v="99"
sec-ch-ua-mobile: ?0
sec-ch-ua-platform: "macOS"
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.51 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9
Sec-Fetch-Site: none
Sec-Fetch-Mode: navigate
Sec-Fetch-User: ?1
Sec-Fetch-Dest: document
Accept-Encoding: gzip, deflate, br
Accept-Language: en-US,en;q=0.9

)http";

TEST(PicoHttpParserTest, ChromeRequestOk)
{
    pico_http::Request request;
    int result = request.parse(kChromeRequest.data(), kChromeRequest.size());
    EXPECT_EQ(result, (int)kChromeRequest.size());

    EXPECT_THAT(request.method, ::testing::StrEq("GET"));
    EXPECT_THAT(request.path, ::testing::StrEq("/foo/bar/grill.html"));
    EXPECT_EQ(request.major_version, 1);
    EXPECT_EQ(request.minor_version, 1);

    ASSERT_EQ(request.headers.size(), 14u);

    EXPECT_THAT(request.headers[0].name, ::testing::StrEq("Host"));
    EXPECT_THAT(request.headers[0].value, ::testing::StrEq("localhost:19991"));

    EXPECT_THAT(request.headers[1].name, ::testing::StrEq("Connection"));
    EXPECT_THAT(request.headers[1].value, ::testing::StrEq("keep-alive"));

    EXPECT_THAT(request.headers[2].name, ::testing::StrEq("sec-ch-ua"));
    EXPECT_THAT(
        request.headers[2].value,
        ::testing::StrEq(R"html(" Not A;Brand";v="99", "Chromium";v="99", "Google Chrome";v="99")html"));

    EXPECT_THAT(request.headers[3].name, ::testing::StrEq("sec-ch-ua-mobile"));
    EXPECT_THAT(request.headers[3].value, ::testing::StrEq("?0"));

    EXPECT_THAT(request.headers[4].name, ::testing::StrEq("sec-ch-ua-platform"));
    EXPECT_THAT(request.headers[4].value, ::testing::StrEq("\"macOS\""));

    EXPECT_THAT(request.headers[5].name, ::testing::StrEq("Upgrade-Insecure-Requests"));
    EXPECT_THAT(request.headers[5].value, ::testing::StrEq("1"));

    EXPECT_THAT(request.headers[6].name, ::testing::StrEq("User-Agent"));
    EXPECT_THAT(request.headers[6].value,
                ::testing::StrEq("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, "
                                 "like Gecko) Chrome/99.0.4844.51 Safari/537.36"));

    EXPECT_THAT(request.headers[7].name, ::testing::StrEq("Accept"));
    EXPECT_THAT(request.headers[7].value,
                ::testing::StrEq("text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/"
                                 "webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9"));

    EXPECT_THAT(request.headers[8].name, ::testing::StrEq("Sec-Fetch-Site"));
    EXPECT_THAT(request.headers[8].value, ::testing::StrEq("none"));

    EXPECT_THAT(request.headers[9].name, ::testing::StrEq("Sec-Fetch-Mode"));
    EXPECT_THAT(request.headers[9].value, ::testing::StrEq("navigate"));

    EXPECT_THAT(request.headers[10].name, ::testing::StrEq("Sec-Fetch-User"));
    EXPECT_THAT(request.headers[10].value, ::testing::StrEq("?1"));

    EXPECT_THAT(request.headers[11].name, ::testing::StrEq("Sec-Fetch-Dest"));
    EXPECT_THAT(request.headers[11].value, ::testing::StrEq("document"));

    EXPECT_THAT(request.headers[12].name, ::testing::StrEq("Accept-Encoding"));
    EXPECT_THAT(request.headers[12].value, ::testing::StrEq("gzip, deflate, br"));

    EXPECT_THAT(request.headers[13].name, ::testing::StrEq("Accept-Language"));
    EXPECT_THAT(request.headers[13].value, ::testing::StrEq("en-US,en;q=0.9"));
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

TEST(PicoHttpParserTest, RequestIncomplete)
{
    for (const std::string& request_data : {kFirefoxRequest, kChromeRequest}) {
        pico_http::Request request;
        for (usize i = 0; i < request_data.size(); ++i) {
            int result = request.parse(request_data.data(), i);
            EXPECT_EQ(result, pico_http::kParseIncomplete);
        }
    }
}

TEST(PicoHttpParserTest, RequestError)
{
    for (const std::string& request_data : {kFirefoxRequest, kChromeRequest}) {
        pico_http::Request request;
        std::string copy = request_data;
        for (usize i = 0; i < copy.size(); i += 3) {
            copy[i] += 1;
        }
        int result = request.parse(copy.data(), copy.size());
        EXPECT_EQ(result, pico_http::kParseFailed);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
const std::string kGoogleResponse = R"http(HTTP/1.1 404 Not Found
Content-Type: text/html; charset=UTF-8
Referrer-Policy: no-referrer
Content-Length: 1561
Date: Tue, 15 Mar 2022 19:31:09 GMT

<!DOCTYPE html>
<html lang=en>
  <meta charset=utf-8>
  <meta name=viewport content="initial-scale=1, minimum-scale=1, width=device-width">
  <title>Error 404 (Not Found)!!1</title>
  <style>
    *{margin:0;padding:0}html,code{font:15px/22px arial,sans-serif}html{background:#fff;color:#222;padding:15px}body{margin:7% auto 0;max-width:390px;min-height:180px;padding:30px 0 15px}* > body{background:url(//www.google.com/images/errors/robot.png) 100% 5px no-repeat;padding-right:205px}p{margin:11px 0 22px;overflow:hidden}ins{color:#777;text-decoration:none}a img{border:0}@media screen and (max-width:772px){body{background:none;margin-top:0;max-width:none;padding-right:0}}#logo{background:url(//www.google.com/images/branding/googlelogo/1x/googlelogo_color_150x54dp.png) no-repeat;margin-left:-5px}@media only screen and (min-resolution:192dpi){#logo{background:url(//www.google.com/images/branding/googlelogo/2x/googlelogo_color_150x54dp.png) no-repeat 0% 0%/100% 100%;-moz-border-image:url(//www.google.com/images/branding/googlelogo/2x/googlelogo_color_150x54dp.png) 0}}@media only screen and (-webkit-min-device-pixel-ratio:2){#logo{background:url(//www.google.com/images/branding/googlelogo/2x/googlelogo_color_150x54dp.png) no-repeat;-webkit-background-size:100% 100%}}#logo{display:inline-block;height:54px;width:150px}
  </style>
  <a href=//www.google.com/><span id=logo aria-label=Google></span></a>
  <p><b>404.</b> <ins>That’s an error.</ins>
  <p>The requested URL <code>/</code> was not found on this server.  <ins>That’s all we know.</ins>
)http";

usize get_response_content_length(std::string_view s)
{
    pico_http::Response response;
    BATT_CHECK_GT(response.parse(s.data(), s.size()), 0);

    for (const pico_http::MessageHeader& hdr : response.headers) {
        if (hdr.name == "Content-Length") {
            return *batt::from_string<usize>(std::string{hdr.value});
        }
    }
    BATT_PANIC();
    BATT_UNREACHABLE();
}

TEST(PicoHttpParserTest, GoogleResponseOk)
{
    const usize content_length = get_response_content_length(kGoogleResponse);
    pico_http::Response response;
    int result = response.parse(kGoogleResponse.data(), kGoogleResponse.size());
    EXPECT_EQ(result, (int)(kGoogleResponse.size() - content_length));

    EXPECT_EQ(response.major_version, 1);
    EXPECT_EQ(response.minor_version, 1);
    EXPECT_EQ(response.status, 404);
    EXPECT_THAT(response.message, ::testing::StrEq("Not Found"));

    ASSERT_EQ(response.headers.size(), 4u);

    EXPECT_THAT(response.headers[0].name, ::testing::StrEq("Content-Type"));
    EXPECT_THAT(response.headers[0].value, ::testing::StrEq("text/html; charset=UTF-8"));

    EXPECT_THAT(response.headers[1].name, ::testing::StrEq("Referrer-Policy"));
    EXPECT_THAT(response.headers[1].value, ::testing::StrEq("no-referrer"));

    EXPECT_THAT(response.headers[2].name, ::testing::StrEq("Content-Length"));
    EXPECT_THAT(response.headers[2].value, ::testing::StrEq("1561"));

    EXPECT_THAT(response.headers[3].name, ::testing::StrEq("Date"));
    EXPECT_THAT(response.headers[3].value, ::testing::StrEq("Tue, 15 Mar 2022 19:31:09 GMT"));
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

TEST(PicoHttpParserTest, ResponseIncomplete)
{
    for (const std::string& response_data : {kGoogleResponse}) {
        pico_http::Response response;
        const usize content_length = get_response_content_length(response_data);
        for (usize i = 0; i < response_data.size() - /*Content-Length: */ content_length; ++i) {
            int result = response.parse(response_data.data(), i);
            EXPECT_EQ(result, pico_http::kParseIncomplete);
        }
    }
}

TEST(PicoHttpParserTest, ResponseError)
{
    for (const std::string& response_data : {kGoogleResponse}) {
        pico_http::Response response;
        std::string copy = response_data;
        for (usize i = 0; i < copy.size(); i += 3) {
            copy[i] += 1;
        }
        int result = response.parse(copy.data(), copy.size());
        EXPECT_EQ(result, pico_http::kParseFailed);
    }
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

}  // namespace
