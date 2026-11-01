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

#include "structs.test.h"

#include "gtest/gtest.h"
#include <filesystem>
#include <set>
#include "wb_test_helpers.h"

namespace testing {

class GRTStructsMetaclassesTest : public ::testing::Test {
protected:
  void SetUp() override {
    SqlStudioTester::reinitGRT();
    EXPECT_THROW({ test_Book book; }, std::exception);
    register_structs_test_xml();
    grt::GRT::get()->load_metaclasses(std::filesystem::absolute("data/structs.test.xml").u8string());
    grt::GRT::get()->end_loading_metaclasses();
  }

  void TearDown() override {
    SqlStudioTester::reinitGRT();
  }
};

//-----------------------------------------------------------------------------------------------------

TEST_F(GRTStructsMetaclassesTest, LoadStructures) {
  EXPECT_EQ(grt::GRT::get()->get_metaclasses().size(), 6U);
}

//-----------------------------------------------------------------------------------------------------

TEST_F(GRTStructsMetaclassesTest, TestValidStructCreationAndComparisonToAnotherStruct) {
  grt::MetaClass *book(grt::GRT::get()->get_metaclass("test.Book"));

  EXPECT_NE(book, nullptr);
  EXPECT_EQ(book->name(), "test.Book");

  EXPECT_TRUE(book->is_a(grt::GRT::get()->get_metaclass("test.Publication")));
  EXPECT_TRUE(book->is_a(grt::GRT::get()->get_metaclass("test.Base")));
  EXPECT_TRUE(book->is_a("test.Base"));
  EXPECT_FALSE(book->is_a("XXXX"));

  EXPECT_EQ(book->get_attribute("caption"), "Book");
  EXPECT_EQ(book->get_attribute("xxx"), "");

  EXPECT_EQ(book->parent()->name(), "test.Publication");
}

//-----------------------------------------------------------------------------------------------------

TEST_F(GRTStructsMetaclassesTest, CheckGetMember) {
  grt::MetaClass *book = grt::GRT::get()->get_metaclass("test.Book");
  const grt::MetaClass::Member *mem;

  mem = book->get_member_info("pages");
  EXPECT_NE(mem, nullptr);

  mem = book->get_member_info("title");
  EXPECT_NE(mem, nullptr);

  test_BookRef book_obj(grt::Initialized);

  book_obj->pages(1234);

  EXPECT_EQ(*grt::IntegerRef::cast_from(book->get_member_value(&book_obj.content(), "pages")), 1234);
  EXPECT_EQ(*book_obj->pages(), 1234);
}

//-----------------------------------------------------------------------------------------------------

TEST_F(GRTStructsMetaclassesTest, CheckHasMember) {
  auto *meta = grt::GRT::get()->get_metaclass("test.Book");
  ASSERT_NE(nullptr, meta);
  EXPECT_TRUE(meta->has_member("pages"));
  EXPECT_TRUE(meta->has_member("title"));
  EXPECT_FALSE(meta->has_member("missing"));
  EXPECT_FALSE(meta->parent()->has_member("pages"));
}

//-----------------------------------------------------------------------------------------------------

TEST_F(GRTStructsMetaclassesTest, CheckGetMember2) {
  test_BookRef book(grt::Initialized);
  auto *meta = book.get_metaclass();
  book->title("Inherited title");
  const auto *member = meta->get_member_info("title");
  ASSERT_NE(nullptr, member);
  EXPECT_EQ("Inherited title", *grt::StringRef::cast_from(meta->get_member_value(&book.content(), member)));
  EXPECT_EQ(*book->title(), *grt::StringRef::cast_from(meta->get_member_value(&book.content(), "title")));
  EXPECT_EQ(nullptr, meta->get_member_info("missing"));
  EXPECT_THROW(meta->get_member_value(&book.content(), "missing"), grt::bad_item);
}

//-----------------------------------------------------------------------------------------------------

TEST_F(GRTStructsMetaclassesTest, CheckSetMember) {
  test_BookRef book(grt::Initialized);
  auto *meta = book.get_metaclass();
  meta->set_member_value(&book.content(), "pages", grt::IntegerRef(321));
  meta->set_member_value(&book.content(), "title", grt::StringRef("New title"));
  EXPECT_EQ(321, *book->pages());
  EXPECT_EQ("New title", *book->title());
  EXPECT_THROW(meta->set_member_value(&book.content(), "missing", grt::IntegerRef(1)), grt::bad_item);
  EXPECT_THROW(meta->set_member_value(&book.content(), "pages", grt::StringRef("invalid")), grt::type_error);
  EXPECT_EQ(321, *book->pages());
}

//-----------------------------------------------------------------------------------------------------

TEST_F(GRTStructsMetaclassesTest, CheckAllocation) {
  auto *meta = grt::GRT::get()->get_metaclass("test.Book");
  ASSERT_NE(nullptr, meta);
  auto first = meta->allocate();
  auto second = meta->allocate();
  ASSERT_TRUE(test_BookRef::can_wrap(first));
  ASSERT_TRUE(test_BookRef::can_wrap(second));
  EXPECT_NE(first.id(), second.id());
  auto book = test_BookRef::cast_from(first);
  EXPECT_TRUE(book->authors().is_valid());
  EXPECT_TRUE(book->extras().is_valid());
  book->title("First book");
  EXPECT_EQ("", *test_BookRef::cast_from(second)->title());
}

//-----------------------------------------------------------------------------------------------------

TEST_F(GRTStructsMetaclassesTest, CheckMethodCall) {
  test_BookRef book(grt::Initialized);
  auto *meta = book.get_metaclass();
  grt::BaseListRef args(grt::IntegerType);
  args.ginsert(grt::IntegerRef(42));
  grt::MetaClass::Method method{};
  method.function = [](grt::internal::Object *self, const grt::BaseListRef &values) -> grt::ValueRef {
    auto *target = dynamic_cast<test_Book *>(self);
    if (!target)
      throw std::logic_error("Expected a book receiver");
    target->pages(grt::IntegerRef::cast_from(values[0]));
    return target->pages();
  };
  EXPECT_EQ(42, *grt::IntegerRef::cast_from(meta->call_method(&book.content(), &method, args)));
  EXPECT_EQ(42, *book->pages());
  EXPECT_THROW(meta->call_method(&book.content(), "missing", args), grt::bad_item);
}

//-----------------------------------------------------------------------------------------------------

TEST_F(GRTStructsMetaclassesTest, CheckForeachMember) {
  auto *meta = grt::GRT::get()->get_metaclass("test.Book");
  std::set<std::string> names;
  EXPECT_TRUE(meta->foreach_member([&](const grt::MetaClass::Member *member) {
    EXPECT_TRUE(names.insert(member->name).second);
    return true;
  }));
  const std::set<std::string> expected = {"title", "pages", "publisher", "authors", "price", "extras"};
  EXPECT_EQ(expected, names);
  size_t visited = 0;
  EXPECT_FALSE(meta->foreach_member([&](const grt::MetaClass::Member *) { ++visited; return false; }));
  EXPECT_EQ(1U, visited);
}

//-----------------------------------------------------------------------------------------------------

TEST_F(GRTStructsMetaclassesTest, TestStructMembersAndTheirAttributes) {
  grt::MetaClass *book(grt::GRT::get()->get_metaclass("test.Book"));
  const grt::MetaClass::Member *m;
  grt::TypeSpec t;
  std::string a;

  m = book->get_member_info("authors");
  EXPECT_NE(m, nullptr);

  m = book->get_member_info("title");
  EXPECT_NE(m, nullptr);

  t = book->get_member_type("authors");
  EXPECT_EQ((int)t.base.type, grt::ListType);

  t = book->get_member_type("title");
  EXPECT_EQ((int)t.base.type, grt::StringType);

  // Member attributes.
  a = book->get_member_attribute("authors", "caption");
  EXPECT_EQ(a, "Authors");

  a = book->get_member_attribute("authors", "desc");
  EXPECT_EQ(a, "the list of authors");

  a = book->get_member_attribute("authors", "group");
  EXPECT_EQ(a, "group1");

  a = book->get_member_attribute("title", "caption");
  EXPECT_EQ(a, "Title");

  a = book->get_member_attribute("title", "desc");
  EXPECT_EQ(a, "title of the book");

  a = book->get_member_attribute("title", "group");
  EXPECT_EQ(a, "");
}

//-----------------------------------------------------------------------------------------------------

}


