/*
 * Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2026 dev4fun. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms, as
 * designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "base/utf8string.h"
#include "base/string_utilities.h"

#include "gtest/gtest.h"

namespace {



struct LangStringDetails {
  const char *const _text = nullptr;
  size_t _length;
  size_t _bytes;
  LangStringDetails() = default;
  LangStringDetails(const LangStringDetails &) = default;
  LangStringDetails(const char *text, size_t length, size_t bytes) : _text(text), _length(length), _bytes(bytes) {
  }
};

class Utf8StringTest : public ::testing::Test {
protected:
  const std::map<std::string, LangStringDetails> languageStrings = {
    { "english", { "This is a lazy test", 19, 19 }},
    { "polish", { "zażółć", 6, 10 }},
    { "greek", { "ὕαλον ϕαγεῖν", 12, 25 }},
    { "russian", { "Я могу есть стекло", 18, 33 }},
    { "arabic", { "هذا لا يؤلمني", 13, 24 }},
    { "chinese", { "我可以吞下茶", 6, 18 }},
    { "japanese", { "私はお茶を飲み込むことができます", 16, 48 }},
    { "portuguese", { "Há açores e cães ávidos no chão", 31, 36 }}};

  std::map<std::string, const char *> substrings = {
    { "english", "his " }, { "polish", "ażół" }, { "greek", "αλον"}, { "russian", " мог" },
    { "arabic", "ذا ل" },  { "chinese", "可以吞下" }, { "japanese", "はお茶を" }, { "portuguese", "á aç" }};
};

TEST_F(Utf8StringTest, Constructor) {
  base::utf8string str1;
  EXPECT_TRUE(str1.validate());
  EXPECT_EQ(str1.length(), 0U);
  EXPECT_EQ(str1.bytes(), 0U);
  EXPECT_TRUE(str1.empty());

  base::utf8string str2 = "";
  EXPECT_TRUE(str2.validate());
  EXPECT_EQ(str2.length(), 0U);
  EXPECT_EQ(str2.bytes(), 0U);
  EXPECT_TRUE(str2.empty());
}

TEST_F(Utf8StringTest, ConstructorFromCharPointer) {
  // TODO: update cycle name
  for (auto iter : languageStrings) {
    LangStringDetails &current = iter.second;
    base::utf8string str1(current._text); //  from char *

    EXPECT_TRUE(str1.validate());
    EXPECT_EQ(str1, current._text);
    EXPECT_EQ(str1.length(), current._length);
    EXPECT_EQ(str1.bytes(), current._bytes);
    EXPECT_FALSE(str1.empty());
  }
}

TEST_F(Utf8StringTest, ConstructorFromStdString) {
  for (auto iter : languageStrings) {
    LangStringDetails &current = iter.second;
    std::string str_to_init(current._text);
    base::utf8string str2(str_to_init); //  from std::string

    EXPECT_TRUE(str2.validate());
    EXPECT_EQ(str2, current._text);
    EXPECT_EQ(str2.length(), current._length);
    EXPECT_EQ(str2.bytes(), current._bytes);
    EXPECT_FALSE(str2.empty());
  }
}

TEST_F(Utf8StringTest, CopyConstructor) {
  for (auto iter : languageStrings) {
    LangStringDetails &current = iter.second;
    base::utf8string str_to_init(current._text);
    base::utf8string str1(str_to_init); //  copy constructor

    EXPECT_TRUE(str1.validate());
    EXPECT_EQ(str1, current._text);
    EXPECT_EQ(str1.length(), current._length);
    EXPECT_EQ(str1.bytes(), current._bytes);
    EXPECT_FALSE(str1.empty());
  }
}

TEST_F(Utf8StringTest, ConstructorFromWCharPointer) {
  EXPECT_TRUE(base::utf8string(L"").empty());
  for (const auto &entry : languageStrings) {
    SCOPED_TRACE(entry.first);
    const auto wide = base::string_to_wstring(entry.second._text);
    base::utf8string value(wide.c_str());
    EXPECT_TRUE(value.validate());
    EXPECT_EQ(value, entry.second._text);
    EXPECT_EQ(value.length(), entry.second._length);
    EXPECT_EQ(value.bytes(), entry.second._bytes);
  }
  base::utf8string supplementary(L"\U0001F600");
  EXPECT_EQ(supplementary.to_string(), "\xF0\x9F\x98\x80");
  EXPECT_EQ(supplementary.length(), 1U);
  EXPECT_EQ(supplementary.bytes(), 4U);
}

TEST_F(Utf8StringTest, ConstructorFromWString) {
  EXPECT_TRUE(base::utf8string(std::wstring()).empty());
  for (const auto &entry : languageStrings) {
    SCOPED_TRACE(entry.first);
    const auto wide = base::string_to_wstring(entry.second._text);
    base::utf8string value(wide);
    EXPECT_TRUE(value.validate());
    EXPECT_EQ(value, entry.second._text);
    EXPECT_EQ(value.length(), entry.second._length);
    EXPECT_EQ(value.bytes(), entry.second._bytes);
  }
  base::utf8string supplementary(std::wstring(L"\U0001F600"));
  EXPECT_EQ(supplementary.to_string(), "\xF0\x9F\x98\x80");
  EXPECT_EQ(supplementary.length(), 1U);
  EXPECT_EQ(supplementary.bytes(), 4U);
}

TEST_F(Utf8StringTest, SubstringConstructorFromCharPointer) {
  for (auto iter : languageStrings) {
    LangStringDetails &current = iter.second;

    base::utf8string str1(current._text, 1, 4);   //  from char *
    base::utf8string str2(current._text, 500, 4); //  sub-string from invalid index
    base::utf8string str3(current._text, 1, 500); //  sub-string with huge length
    base::utf8string str4(current._text, 1, 0);   //  sub-string with zero length

    base::utf8string right_to_compare = base::utf8string(current._text).right(current._length - 1);

    EXPECT_TRUE(str1.validate());
    EXPECT_EQ(str1, substrings[iter.first]);
    EXPECT_EQ(str1.size(), 4U);
    EXPECT_FALSE(str1.empty());

    EXPECT_TRUE(str2.validate());
    EXPECT_EQ(str2.size(), 0U);
    EXPECT_EQ(str2.length(), 0U);
    EXPECT_TRUE(str2.empty());

    EXPECT_TRUE(str3.validate());
    EXPECT_EQ(str3, right_to_compare);
    EXPECT_EQ(str3.size(), current._length - 1);
    EXPECT_EQ(str3.length(), current._length - 1);
    EXPECT_FALSE(str3.empty());

    EXPECT_TRUE(str4.validate());
    EXPECT_EQ(str4.size(), 0U);
    EXPECT_EQ(str4.length(), 0U);
    EXPECT_TRUE(str4.empty());
  }
}

TEST_F(Utf8StringTest, SubstringConstructorFromStdString) {
  for (auto iter : languageStrings) {
    LangStringDetails &current = iter.second;

    std::string str_to_init(current._text);

    base::utf8string str1(str_to_init, 1, 4);   //  from std::string
    base::utf8string str2(str_to_init, 500, 4); //  sub-string from invalid index
    base::utf8string str3(str_to_init, 1, 500); //  sub-string with huge length
    base::utf8string str4(str_to_init, 1, 0);   //  sub-string with zero length

    base::utf8string right_to_compare = base::utf8string(current._text).right(current._length - 1);

    EXPECT_TRUE(str1.validate());
    EXPECT_EQ(str1, substrings[iter.first]);
    EXPECT_EQ(str1.size(), 4U);
    EXPECT_FALSE(str1.empty());

    EXPECT_TRUE(str2.validate());
    EXPECT_EQ(str2.size(), 0U);
    EXPECT_EQ(str2.length(), 0U);
    EXPECT_TRUE(str2.empty());

    EXPECT_TRUE(str3.validate());
    EXPECT_EQ(str3, right_to_compare);
    EXPECT_EQ(str3.size(), current._length - 1);
    EXPECT_EQ(str3.length(), current._length - 1);
    EXPECT_FALSE(str3.empty());

    EXPECT_TRUE(str4.validate());
    EXPECT_EQ(str4.size(), 0U);
    EXPECT_EQ(str4.length(), 0U);
    EXPECT_TRUE(str4.empty());
  }
}

TEST_F(Utf8StringTest, SubstringConstructorFromUtf8String) {
  for (auto iter : languageStrings) {
    LangStringDetails &current = iter.second;

    base::utf8string str_to_init(current._text);

    base::utf8string str1(str_to_init, 1, 4);   //  from utf8string
    base::utf8string str2(str_to_init, 500, 4); //  sub-string from invalid index
    base::utf8string str3(str_to_init, 1, 500); //  sub-string with huge length
    base::utf8string str4(str_to_init, 1, 0);   //  sub-string with zero length

    base::utf8string right_to_compare = base::utf8string(current._text).right(current._length - 1);

    EXPECT_TRUE(str1.validate());
    EXPECT_EQ(str1, substrings[iter.first]);
    EXPECT_EQ(str1.size(), 4U);
    EXPECT_FALSE(str1.empty());

    EXPECT_TRUE(str2.validate());
    EXPECT_EQ(str2.size(), 0U);
    EXPECT_EQ(str2.length(), 0U);
    EXPECT_TRUE(str2.empty());

    EXPECT_TRUE(str3.validate());
    EXPECT_EQ(str3, right_to_compare);
    EXPECT_EQ(str3.size(), current._length - 1);
    EXPECT_EQ(str3.length(), current._length - 1);
    EXPECT_FALSE(str3.empty());

    EXPECT_TRUE(str4.validate());
    EXPECT_EQ(str4.size(), 0U);
    EXPECT_EQ(str4.length(), 0U);
    EXPECT_TRUE(str4.empty());
  }
}

TEST_F(Utf8StringTest, CharacterConstructor) {
  base::utf8string str(10, 'a');

  EXPECT_TRUE(str.validate());
  EXPECT_EQ(str, "aaaaaaaaaa");
  EXPECT_EQ(str.size(), 10U);
  EXPECT_EQ(str.length(), 10U);
  EXPECT_EQ(str.bytes(), 10U);
  EXPECT_FALSE(str.empty());
}

TEST_F(Utf8StringTest, Utf8CharacterConstructorUnicode) {
  base::utf8string str(10, base::utf8string::utf8char("ł"));

  EXPECT_TRUE(str.validate());
  EXPECT_EQ(str, "łłłłłłłłłł");
  EXPECT_EQ(str.size(), 10U);
  EXPECT_EQ(str.length(), 10U);
  EXPECT_EQ(str.bytes(), 20U);
  EXPECT_FALSE(str.empty());
}

TEST_F(Utf8StringTest, Utf8CharacterConstructorNonUnicode) {
  base::utf8string str(10, base::utf8string::utf8char("a"));

  EXPECT_TRUE(str.validate());
  EXPECT_EQ(str, "aaaaaaaaaa");
  EXPECT_EQ(str.size(), 10U);
  EXPECT_EQ(str.length(), 10U);
  EXPECT_EQ(str.bytes(), 10U);
  EXPECT_FALSE(str.empty());
}

TEST_F(Utf8StringTest, IndexOperatorAndAt) {
  base::utf8string str = std::string("zażółć");
  base::utf8string::utf8char res1("ó");
  base::utf8string::utf8char res2("ć");
  EXPECT_EQ(str[3], res1);
  EXPECT_EQ(str[5], res2);

  // TODO: test utf8string::at()
}

TEST_F(Utf8StringTest, SubstrMethod) {
  base::utf8string str = std::string("zażółć");
  base::utf8string res1 = "żółć";
  base::utf8string res2 = "aż";
  EXPECT_EQ(str.substr(2), res1);
  EXPECT_EQ(str.substr(1, 2), res2);
}

TEST_F(Utf8StringTest, Operators) {
  base::utf8string str1 = std::string("zażółć");
  base::utf8string str2 = std::string("gęślą");
  base::utf8string result = std::string("zażółćgęślą");

  EXPECT_EQ(str1 + str2, result);
  str1 += str2;
  EXPECT_EQ(str1, result);
  str1 = str2;
  EXPECT_EQ(str1, str2);
  EXPECT_EQ(str1 == str2, true);
  EXPECT_EQ(str1 != result, true);
}

TEST_F(Utf8StringTest, StringConversion) {
  for (auto iter : languageStrings) {
    LangStringDetails &current = iter.second;

    base::utf8string str(current._text);
    EXPECT_EQ(strcmp(str.c_str(), current._text), 0);
    EXPECT_TRUE(str.to_string() == std::string(current._text));
    EXPECT_TRUE(str.to_wstring() == base::string_to_wstring(current._text));
  }
}

TEST_F(Utf8StringTest, MoveConstructorAndOperator) {
  //  TODO: test in all languages
  base::utf8string str1(std::string("za") + std::string("żółć"));
  EXPECT_EQ(str1.length(), 6U);
  EXPECT_EQ(str1.bytes(), 10U);
  EXPECT_FALSE(str1.empty());

  base::utf8string str2 = std::move(str1);
  EXPECT_EQ(str2.length(), 6U);
  EXPECT_EQ(str2.bytes(), 10U);
  EXPECT_FALSE(str2.empty());
}

TEST_F(Utf8StringTest, TrimFunctions) {
  //  TODO: test in all languages
  EXPECT_EQ(base::utf8string("  zażółć    ").trim_left().length(), 10U);
  EXPECT_EQ(base::utf8string("  zażółć    ").trim_left().bytes(), 14U);
  EXPECT_EQ(base::utf8string("  zażółć    ").trim_right().length(), 8U);
  EXPECT_EQ(base::utf8string("  zażółć    ").trim_right().bytes(), 12U);
  EXPECT_EQ(base::utf8string("  zażółć    ").trim().length(), 6U);
  EXPECT_EQ(base::utf8string("  zażółć    ").trim().bytes(), 10U);
}

TEST_F(Utf8StringTest, CaseConversionAndValidation) {
  EXPECT_EQ(base::utf8string("zażółć").to_upper(), base::utf8string("ZAŻÓŁĆ"));
  EXPECT_EQ(base::utf8string("ZAŻÓŁĆ").to_lower(), base::utf8string("zażółć"));

  EXPECT_EQ(base::utf8string("zAżóŁć").to_case_fold(), base::utf8string("zażółć"));
  EXPECT_TRUE(base::utf8string("grüßen").validate());
}
  
TEST_F(Utf8StringTest, TruncateSubstrLeftRight) {
  EXPECT_EQ(base::utf8string("zażółć").truncate(0), base::utf8string("..."));
  EXPECT_EQ(base::utf8string("zażółć").truncate(1), base::utf8string("z..."));
  EXPECT_EQ(base::utf8string("zażółć").truncate(2), base::utf8string("za..."));
  EXPECT_EQ(base::utf8string("zażółć").truncate(3), "zażółć");
  EXPECT_EQ(base::utf8string("zażółć").truncate(4), "zażółć");
  EXPECT_EQ(base::utf8string("zażółć").truncate(5), "zażółć");
  EXPECT_EQ(base::utf8string("zażółć").truncate(6), "zażółć");
  EXPECT_EQ(base::utf8string("zażółć").truncate(7), "zażółć");

  EXPECT_EQ(base::utf8string("zażółć").left(0), "");
  EXPECT_EQ(base::utf8string("zażółć").left(1), "z");
  EXPECT_EQ(base::utf8string("zażółć").left(2), "za");
  EXPECT_EQ(base::utf8string("zażółć").left(3), "zaż");
  EXPECT_EQ(base::utf8string("zażółć").left(4), "zażó");
  EXPECT_EQ(base::utf8string("zażółć").left(5), "zażół");
  EXPECT_EQ(base::utf8string("zażółć").left(6), "zażółć");
  EXPECT_EQ(base::utf8string("zażółć").left(7), "zażółć");

  EXPECT_EQ(base::utf8string("zażółć").right(0), "");
  EXPECT_EQ(base::utf8string("zażółć").right(1), "ć");
  EXPECT_EQ(base::utf8string("zażółć").right(2), "łć");
  EXPECT_EQ(base::utf8string("zażółć").right(3), "ółć");
  EXPECT_EQ(base::utf8string("zażółć").right(4), "żółć");
  EXPECT_EQ(base::utf8string("zażółć").right(5), "ażółć");
  EXPECT_EQ(base::utf8string("zażółć").right(6), "zażółć");
  EXPECT_EQ(base::utf8string("zażółć").right(7), "zażółć");
}

TEST_F(Utf8StringTest, StartsWithEndsWithContains) {
  base::utf8string str = std::string("zażółć");
  EXPECT_TRUE(str.starts_with("za"));
  EXPECT_FALSE(str.starts_with("kk"));
  EXPECT_FALSE(str.starts_with("toolongstring"));
  EXPECT_TRUE(str.ends_with("ółć"));
  EXPECT_FALSE(str.ends_with("ÓŁa"));
  EXPECT_FALSE(str.ends_with("toolongstring"));
  EXPECT_TRUE(str.contains("żół"));
  EXPECT_FALSE(str.contains("ŻÓŁ"));
  EXPECT_TRUE(str.contains("ŻÓŁ", false));
  EXPECT_FALSE(str.contains("", false));
}

TEST_F(Utf8StringTest, CharIndexToByteOffsetConversions) {
  base::utf8string str = std::string("zażółć");
  EXPECT_EQ(str.charIndexToByteOffset(2), 2U);
  EXPECT_EQ(str.charIndexToByteOffset(3), 4U);
  EXPECT_EQ(str.charIndexToByteOffset(4), 6U);
  EXPECT_EQ(str.byteOffsetToCharIndex(2), 2U);
  EXPECT_EQ(str.byteOffsetToCharIndex(5), 4U);
  EXPECT_EQ(str.byteOffsetToCharIndex(6), 4U);
}

TEST_F(Utf8StringTest, Iterator) {
  base::utf8string str = std::string("zażółć");
  base::utf8string::iterator iter = str.begin();
  
  EXPECT_EQ(*iter, base::utf8string::utf8char("z"));
  EXPECT_TRUE(iter == str.begin());
  EXPECT_FALSE(iter == str.end());
  ++iter;
  EXPECT_EQ(*iter, base::utf8string::utf8char("a"));
  for (size_t i = 0; i < 5; i++) {
    ++iter;
  }
  EXPECT_TRUE(iter == str.end());
  --iter;
  EXPECT_EQ(*iter, base::utf8string::utf8char("ć"));
}

}
