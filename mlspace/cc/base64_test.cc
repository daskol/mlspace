// Copyright 2025 Daniel Bershatsky
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <mlspace/cc/base64.h>

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
