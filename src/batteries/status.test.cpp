#include <batteries/status.hpp>
//
#include <batteries/status.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

batt::Status foo()
{
    return batt::OkStatus();
}

enum struct MyCodes {
    OK = 0,
    NOT_REGISTERED,
    BAD,
    TERRIBLE,
    THE_WORST,
};

enum struct HttpCode {
    CONTINUE = 100,
    OK = 200,
    REDIRECT = 300,
    CLIENT_ERROR = 400,
    SERVER_ERROR,
};

class StatusTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        batt::Status::register_codes<MyCodes>({{MyCodes::OK, "it is ok!"},
                                               {MyCodes::BAD, "it is bad!"},
                                               {MyCodes::TERRIBLE, "it is terrible!"},
                                               {MyCodes::THE_WORST, "The. Worst. Ever."}});

        batt::Status::register_codes<HttpCode>({{HttpCode::OK, "HTTP Ok"},
                                                {HttpCode::CONTINUE, "HTTP Continue"},
                                                {HttpCode::REDIRECT, "HTTP Redirect"},
                                                {HttpCode::CLIENT_ERROR, "HTTP Client Error"},
                                                {HttpCode::SERVER_ERROR, "HTTP Server Error"}});
    }
};

TEST_F(StatusTest, DefaultConstruct)
{
    batt::Status s;

    EXPECT_TRUE(s.ok());
    EXPECT_EQ(s.code(), 0);
    EXPECT_THAT(s.message(), ::testing::StrEq("Ok"));
}

TEST_F(StatusTest, RegisteredEnums)
{
    batt::Status s2 = MyCodes::BAD;
    EXPECT_EQ(s2.code(), batt::Status::kGroupSize * 2 + 1);

    EXPECT_EQ(s2, MyCodes::BAD);
    EXPECT_EQ(MyCodes::BAD, s2);
    EXPECT_THAT(s2.message(), ::testing::StrEq("it is bad!"));
    EXPECT_FALSE(s2.ok());

    EXPECT_THAT(batt::Status{HttpCode::OK}.message(), ::testing::StrEq("HTTP Ok"));
    EXPECT_THAT(batt::Status{HttpCode::CONTINUE}.message(), ::testing::StrEq("HTTP Continue"));
    EXPECT_THAT(batt::Status{HttpCode::REDIRECT}.message(), ::testing::StrEq("HTTP Redirect"));
    EXPECT_THAT(batt::Status{HttpCode::CLIENT_ERROR}.message(), ::testing::StrEq("HTTP Client Error"));
    EXPECT_THAT(batt::Status{HttpCode::SERVER_ERROR}.message(), ::testing::StrEq("HTTP Server Error"));
}

TEST_F(StatusTest, AllOkCodesAreEqual)
{
    batt::Status s;
    EXPECT_EQ(s.code(), 0);

    EXPECT_EQ(s, MyCodes::OK);
    EXPECT_EQ(MyCodes::OK, s);
    EXPECT_TRUE(batt::Status{MyCodes::OK}.ok());
    EXPECT_THAT(batt::Status{MyCodes::OK}.message(), ::testing::StrEq("it is ok!"));

    EXPECT_EQ(s, HttpCode::OK);
    EXPECT_EQ(HttpCode::OK, s);
    EXPECT_TRUE(batt::Status{HttpCode::OK}.ok());
    EXPECT_FALSE(batt::Status{HttpCode::CLIENT_ERROR}.ok());
    EXPECT_THAT(batt::Status{HttpCode::OK}.message(), ::testing::StrEq("HTTP Ok"));
}

TEST_F(StatusTest, CodeGroupForType)
{
    const batt::Status::CodeGroup& type_group = batt::Status::code_group_for_type<MyCodes>();
    const batt::Status::CodeGroup& group = batt::Status{MyCodes::NOT_REGISTERED}.group();

    EXPECT_EQ(type_group.index, 2u);
    EXPECT_EQ(type_group.enum_type_index, std::type_index{typeid(MyCodes)});
    EXPECT_EQ(group.index, 2u);
    EXPECT_EQ(group.enum_type_index, std::type_index{typeid(MyCodes)});

    EXPECT_EQ(batt::Status::code_group_for_type<HttpCode>().min_enum_value, 100);
}

TEST_F(StatusTest, UnknownEnumValueMessage)
{
    EXPECT_THAT(batt::Status{MyCodes::NOT_REGISTERED}.message(),
                ::testing::StrEq("(Unknown enum value; not registered via batt::Status::register_codes)"));

    EXPECT_THAT(batt::Status{MyCodes::NOT_REGISTERED}.message(),
                ::testing::StrEq(batt::Status::unknown_enum_value_message()));
}

TEST_F(StatusTest, IgnoreErrorCompilesOk)
{
    foo().IgnoreError();
}

TEST_F(StatusTest, ConvertErrnoToStatus)
{
    for (int e : {0, EPERM, ENOENT, ESRCH, EINTR, EIO, EAGAIN}) {
        EXPECT_EQ(batt::status_from_errno(e).ok(), (e == 0));
        EXPECT_THAT(batt::status_from_errno(e).message(), ::testing::StrEq(std::strerror(e)));
    }
}

#ifdef BATT_STATUS_CUSTOM_MESSSAGES
TEST_F(StatusTest, CustomMessage)
{
    batt::Status s1{MyCodes::BAD};
    batt::Status s2{MyCodes::BAD, "maybe it is not so bad after all"};

    EXPECT_EQ(s1.code(), s2.code());
    EXPECT_EQ(s1.ok(), s2.ok());
    EXPECT_EQ(s1, s2);

    EXPECT_THAT(s1.message(), ::testing::StrEq("it is bad!"));
    EXPECT_THAT(s2.message(), ::testing::StrEq("maybe it is not so bad after all"));
}
#endif

TEST_F(StatusTest, MessageFromCode)
{
    EXPECT_THAT(batt::Status{MyCodes::TERRIBLE}.message(), ::testing::StrEq("it is terrible!"));
    EXPECT_THAT(batt::Status::message_from_code(batt::Status{MyCodes::TERRIBLE}.code()),
                ::testing::StrEq("it is terrible!"));
}

//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++

TEST(StatusOrTest, DefaultConstruct)
{
    batt::StatusOr<std::string> s;
    batt::StatusOr<int> i;

    EXPECT_FALSE(s.ok());
    EXPECT_FALSE(i.ok());

    EXPECT_EQ(s.status(), batt::StatusCode::kUnknown);
    EXPECT_EQ(i.status(), batt::StatusCode::kUnknown);
}

TEST(StatusOrTest, CopyConstruct)
{
    batt::StatusOr<std::string> s{"some foo"};
    batt::StatusOr<std::string> s2 = s;

    EXPECT_TRUE(s.ok());
    EXPECT_EQ(*s, std::string("some foo"));

    EXPECT_TRUE(s2.ok());
    EXPECT_EQ(*s2, std::string("some foo"));

    const std::string c = "some bar";
    batt::StatusOr<std::string> s3{c};

    EXPECT_TRUE(s3.ok());
    EXPECT_EQ(*s3, c);
}

TEST(StatusOrTest, MoveConstruct)
{
    batt::StatusOr<std::unique_ptr<int>> i{std::make_unique<int>(42)};
    batt::StatusOr<std::unique_ptr<int>> i2 = std::move(i);

    EXPECT_FALSE(i.ok());
    EXPECT_TRUE(*i == nullptr);

    EXPECT_TRUE(i2.ok());
    EXPECT_TRUE(*i2 != nullptr);
    EXPECT_EQ(**i2, 42);
}

TEST(StatusOrTest, ConstructFromStatus)
{
    batt::Status s = batt::StatusCode::kNotFound;
    batt::StatusOr<std::string> t{s};

    EXPECT_FALSE(t.ok());
    EXPECT_EQ(s, t.status());
    EXPECT_EQ(t.status(), batt::StatusCode::kNotFound);
    EXPECT_NE(t.status(), batt::StatusCode::kCancelled);

    batt::Status s2 = std::move(t).status();
    EXPECT_EQ(s, s2);
}

TEST(StatusOrTest, CopyAssign)
{
    batt::StatusOr<std::string> s{"some foo"};
    batt::StatusOr<std::string> s2;
    auto* retval = &(s2 = s);

    EXPECT_EQ(retval, &s2);
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(*s, std::string("some foo"));

    EXPECT_TRUE(s2.ok());
    EXPECT_EQ(*s2, std::string("some foo"));
}

TEST(StatusOrTest, CopyAssignFromValue)
{
    const char* s = "some foo";
    batt::StatusOr<std::string> s2;
    auto* retval = &(s2 = s);

    EXPECT_EQ(retval, &s2);
    EXPECT_THAT(s, ::testing::StrEq("some foo"));

    EXPECT_TRUE(s2.ok());
    EXPECT_EQ(*s2, std::string("some foo"));

    const batt::StatusOr<std::string> s3 = s2;

    EXPECT_EQ(*s3, *s2);
    EXPECT_THAT(s3->c_str(), ::testing::StrEq(*s2));
}

TEST(StatusOrTest, CopyAssignFromValue2)
{
    const std::string s = "some foo";
    batt::StatusOr<std::string> s2;
    auto* retval = &(s2 = s);

    EXPECT_EQ(retval, &s2);
    EXPECT_THAT(s, ::testing::StrEq("some foo"));

    EXPECT_TRUE(s2.ok());
    EXPECT_EQ(*s2, std::string("some foo"));

    const batt::StatusOr<std::string> s3 = s2;

    EXPECT_EQ(*s3, *s2);
    EXPECT_THAT(s3->c_str(), ::testing::StrEq(*s2));
}

TEST(StatusOrTest, MoveAssign)
{
    batt::StatusOr<std::unique_ptr<int>> i{std::make_unique<int>(42)};

    EXPECT_TRUE(i.ok());
    EXPECT_TRUE(*i != nullptr);
    EXPECT_EQ(**i, 42);

    int* ptr = i->get();

    batt::StatusOr<std::unique_ptr<int>> i2;

    auto* retval = &(i2 = std::move(i));

    EXPECT_EQ(retval, &i2);
    EXPECT_TRUE(i.ok());
    EXPECT_TRUE(*i == nullptr);

    EXPECT_TRUE(i2.ok());
    EXPECT_TRUE(*i2 != nullptr);
    EXPECT_EQ(**i2, 42);
    EXPECT_EQ(i2->get(), ptr);
}

TEST(StatusOrTest, MoveAssignFromValue)
{
    std::unique_ptr<int> i = std::make_unique<int>(42);

    EXPECT_TRUE(i != nullptr);
    EXPECT_EQ(*i, 42);

    int* ptr = i.get();

    batt::StatusOr<std::unique_ptr<int>> i2;

    auto* retval = &(i2 = std::move(i));

    EXPECT_EQ(retval, &i2);
    EXPECT_TRUE(i == nullptr);

    EXPECT_TRUE(i2.ok());
    EXPECT_TRUE(*i2 != nullptr);
    EXPECT_EQ(**i2, 42);
    EXPECT_EQ(i2->get(), ptr);
}

TEST(StatusOrTest, AssignFromStatus)
{
    batt::Status s = batt::StatusCode::kNotFound;
    batt::StatusOr<std::string> t;

    auto* retval = &(t = s);

    EXPECT_EQ(retval, &t);
    EXPECT_FALSE(t.ok());
    EXPECT_EQ(s, t.status());
    EXPECT_EQ(t.status(), batt::StatusCode::kNotFound);
    EXPECT_NE(t.status(), batt::StatusCode::kCancelled);
}

batt::StatusOr<int> get_positive_int(int i)
{
    if (i <= 0) {
        return batt::Status{batt::StatusCode::kInvalidArgument};
    }
    return i;
}

batt::Status do_maths()
{
    auto a = get_positive_int(42);
    BATT_REQUIRE_OK(a);

    auto b = get_positive_int(-4);
    BATT_REQUIRE_OK(b);

    return batt::OkStatus();
}

TEST(StatusOrTest, RequireOkMacro)
{
    batt::Status s = do_maths();

    EXPECT_FALSE(s.ok());
    EXPECT_EQ(s, batt::StatusCode::kInvalidArgument);
}

struct TheBase {
};
struct TheDerived : TheBase {
};

TEST(StatusOrTest, ConstructFromDerivedClass)
{
    {
        std::unique_ptr<TheDerived> p_dev = std::make_unique<TheDerived>();
        void* ptr = p_dev.get();

        std::unique_ptr<TheBase> p_base = std::move(p_dev);
        EXPECT_EQ(ptr, p_base.get());

        batt::StatusOr<std::unique_ptr<TheDerived>> s{std::make_unique<TheDerived>()};
        ptr = s->get();

        batt::StatusOr<std::unique_ptr<TheDerived>> s2{std::move(s)};
        EXPECT_EQ(s2->get(), ptr);

        batt::StatusOr<std::unique_ptr<TheBase>> s3{std::move(s2)};
        EXPECT_EQ(s3->get(), ptr);
    }
    {
        std::shared_ptr<TheDerived> p_dev = std::make_shared<TheDerived>();
        void* ptr1 = p_dev.get();

        std::shared_ptr<TheBase> p_base = p_dev;
        EXPECT_EQ(ptr1, p_base.get());

        std::shared_ptr<TheBase> p_base2 = std::move(p_dev);
        EXPECT_EQ(ptr1, p_base2.get());

        batt::StatusOr<std::shared_ptr<TheDerived>> s{std::make_shared<TheDerived>()};
        void* ptr2 = s->get();

        batt::StatusOr<std::shared_ptr<TheDerived>> s2{std::move(s)};
        EXPECT_EQ(ptr2, s2->get());

        batt::StatusOr<std::shared_ptr<TheBase>> s3{std::move(s2)};
        EXPECT_EQ(ptr2, s3->get());

        batt::StatusOr<std::shared_ptr<TheBase>> s4{p_base2};
        EXPECT_EQ(ptr1, s4->get());
    }
}

}  // namespace
