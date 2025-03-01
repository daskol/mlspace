#include <gtest/gtest.h>

#include <base64.h>

using mlspace::Base64;

TEST(Base64, Init) {
    Base64 base64;
    EXPECT_EQ(base64.bits[0], 0xff);
    EXPECT_EQ(base64.bits[0x40], 0xff);
    EXPECT_EQ(base64.bits[0x41], 0x00);
    EXPECT_EQ(base64.bits[0x42], 0x01);
    EXPECT_EQ(base64.bits[0x43], 0x02);
}

TEST(Base64, Decode) {
    auto result = Base64().Decode("TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu");
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, "Many hands make light work.") << *result;
}

TEST(Base64, DecodeWithPadding1) {
    auto result = Base64().Decode("ay4=");
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, "k.");
}

TEST(Base64, DecodeWithPadding2) {
    auto result = Base64().Decode("aw==");
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, "k");
}

TEST(Base64, DecodeWithPadding3) {
    auto result = Base64().Decode("a===");
    ASSERT_FALSE(result);
}

TEST(Base64, DecodeWithoutPadding1) {
    auto result = Base64().Decode("ay4");
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, "k.");
}

TEST(Base64, DecodeWithoutPadding2) {
    auto result = Base64().Decode("aw");
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, "k");
}

TEST(Base64, DecodeWithoutPadding3) {
    auto result = Base64().Decode("a");
    ASSERT_FALSE(result);
}

TEST(Base64, Encode) {
    auto actual = Base64().Encode("Many hands make light work.");
    ASSERT_EQ(actual, "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu");
}

TEST(Base64, EncodeWithPadding1) {
    auto result = Base64().Encode("rk");
    ASSERT_EQ(result, "cms=");
}

TEST(Base64, EncodeWithPadding2) {
    auto result = Base64().Encode("r");
    ASSERT_EQ(result, "cg==");
}
