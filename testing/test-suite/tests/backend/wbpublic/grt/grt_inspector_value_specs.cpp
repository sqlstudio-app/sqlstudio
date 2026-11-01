/*
 * Copyright (c) 2019, 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "grt/grt_value_inspector.h"
#include "wb_test_helpers.h"

using namespace grt;
using namespace bec;

#include "wb_test_helpers.h"
#include "grt_test_helpers.h"

#include "grt_values_test_data.h"
#include "structs.test.h"

#include "gtest/gtest.h"
#include "context.h"

#include <fstream>

namespace {

static void expectFilesEqual(const std::string &test, const std::string file, const std::string reffile) {
  std::string line, refline;
  std::ifstream ref(reffile);
  std::ifstream f(file);

  EXPECT_TRUE(ref.is_open());
  EXPECT_TRUE(f.is_open());

  while (!ref.eof() && !f.eof()) {
    getline(ref, refline);
    getline(f, line);

    EXPECT_EQ(refline, line);
  }

  EXPECT_TRUE(f.eof() && ref.eof());
}

class GRTInspectorValueTest : public ::testing::Test {
public:
  GRTInspectorValueTest *data = this;
  std::string dataDir = testing::Context::get().tmpDataDir();
  std::string outputDir = testing::Context::get().outputDir();

  void SetUp() override {
    register_structs_test_xml();
    grt::GRT::get()->load_metaclasses(testing::Context::get().tmpDataDir() + "/structs.test.xml");
    grt::GRT::get()->end_loading_metaclasses();
  }

  void TearDown() override {
    SqlStudioTester::reinitGRT();
  }
};

TEST_F(GRTInspectorValueTest, TestInspectionOfList) {
  bool flag;
  // create a test list
  BaseListRef list(create_list_with_varied_data());

  EXPECT_EQ(10, (int)list.count());
  EXPECT_EQ(ListType, list.type());


    ValueInspectorBE *vinsp = ValueInspectorBE::create(list, false, false);

    // test listing
    size_t c = vinsp->count();
    EXPECT_EQ(10U, c);

    std::string name, value;
    Type type;
    NodeId node;

    g_mkdir_with_parents("output", 0700);

    std::vector<ssize_t> columns;
    columns.push_back(ValueInspectorBE::Name);
    columns.push_back(ValueInspectorBE::Value);
    testing::dumpTreeModel(outputDir + "/grt_inspector_value_test1.txt", vinsp, columns, true);
    expectFilesEqual("list contents", outputDir + "/grt_inspector_value_test1.txt", dataDir + "/be/grt_inspector_value_test1.txt");

    try {
      node = vinsp->get_node(10);
      flag = vinsp->get_field(node, ValueInspectorBE::Name, name);
      EXPECT_FALSE(flag);
    } catch (...) {
    }

    // test get random item

    node = vinsp->get_child(vinsp->get_root(), 6);

    flag = vinsp->get_field(node, ValueInspectorBE::Name, name);
    EXPECT_TRUE(flag);
    flag = vinsp->get_field(node, ValueInspectorBE::Value, value);
    EXPECT_TRUE(flag);
    type = vinsp->get_field_type(node, ValueInspectorBE::Value);
    EXPECT_TRUE(flag);
    EXPECT_EQ("[7]", name);
    EXPECT_EQ("{item1 = 1, item2 = 2.200000, item3 = test}", value);
    EXPECT_EQ(DictType, type);

    NodeId nd;
    EXPECT_FALSE(nd.is_valid());

    node = vinsp->get_node(-1);
    EXPECT_FALSE(node.is_valid());

    node = vinsp->get_node(11);
    EXPECT_FALSE(node.is_valid());

    // test change int value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, (ssize_t)112211);
    EXPECT_TRUE(flag);

    flag = vinsp->get_field(0, ValueInspectorBE::Value, value);
    EXPECT_TRUE(flag);
    EXPECT_EQ("112211", value);
    type = vinsp->get_field_type(0, ValueInspectorBE::Value);
    EXPECT_EQ(IntegerType, type);

    flag = vinsp->set_field(9, ValueInspectorBE::Value, (ssize_t)8888);
    EXPECT_TRUE(flag);

    // test change string value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, "hello");
    EXPECT_TRUE(flag);

    flag = vinsp->set_field(9, ValueInspectorBE::Value, "world");
    EXPECT_TRUE(flag);

    // test change double value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, 1.234);
    EXPECT_TRUE(flag);

    flag = vinsp->set_field(8, ValueInspectorBE::Value, 5.1);
    EXPECT_TRUE(flag);

    // test change int value from string
    flag = vinsp->set_convert_field(1, ValueInspectorBE::Value, "112233");
    EXPECT_TRUE(flag);

    testing::dumpTreeModel(outputDir + "/grt_inspector_value_test1.1.txt", vinsp, columns, true);
    expectFilesEqual("list change", outputDir + "/grt_inspector_value_test1.1.txt",
              dataDir + "/be/grt_inspector_value_test1.1.txt");

    // item count still ok?

    EXPECT_EQ((int)list.count(), 10);

    // test add new value
    NodeId nkey;
    flag = vinsp->add_item(nkey);
    EXPECT_TRUE(flag);
    EXPECT_EQ(10U, nkey[0]);
    flag = vinsp->set_field(nkey, ValueInspectorBE::Value, "new value");
    EXPECT_TRUE(flag);

    EXPECT_EQ(list.count(), 11U);

    // test delete value
    flag = vinsp->delete_item(3);
    EXPECT_TRUE(flag);

    EXPECT_EQ(list.count(), 10U);

    testing::dumpTreeModel(outputDir + "/grt_inspector_value_test1.2.txt", vinsp, columns, true);
    expectFilesEqual("list delete", outputDir + "/grt_inspector_value_test1.2.txt",
              dataDir + "/be/grt_inspector_value_test1.2.txt");

    flag = vinsp->delete_item(10);
    EXPECT_FALSE(flag);

    delete vinsp;
}

TEST_F(GRTInspectorValueTest, TestInspectionOfStringTypedList) {
    bool flag;
    std::vector<ssize_t> columns;
    columns.push_back(ValueInspectorBE::Name);
    columns.push_back(ValueInspectorBE::Value);

    // create a test list
    BaseListRef list(create_string_list(10));

    EXPECT_EQ(list.count(), 10U);
    EXPECT_EQ(list.type(), ListType);
    EXPECT_EQ(list.content_type(), StringType);

    ValueInspectorBE *vinsp = ValueInspectorBE::create(list, false, false);

    // test listing
    size_t c = vinsp->count();
    EXPECT_EQ(10U, c);

    testing::dumpTreeModel(outputDir + "/grt_inspector_value_test2.txt", vinsp, columns, true);
    expectFilesEqual("typed list check", outputDir + "/grt_inspector_value_test2.txt",
              dataDir + "/be/grt_inspector_value_test2.txt");

    std::string name, value;

    NodeId node = vinsp->get_node(-1);
    EXPECT_FALSE(node.is_valid());

    flag = vinsp->get_field(11, ValueInspectorBE::Name, name);
    EXPECT_FALSE(flag);

    flag = vinsp->get_field(11, ValueInspectorBE::Value, value);
    EXPECT_FALSE(flag);

    // test change int value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, (ssize_t)112211);
    EXPECT_FALSE(flag);

    // test change string value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, "hello");
    EXPECT_TRUE(flag);

    // test change double value
    flag = vinsp->set_field(1, ValueInspectorBE::Value, 1.234);
    EXPECT_FALSE(flag);

    // test change int value from string
    flag = vinsp->set_convert_field(2, ValueInspectorBE::Value, "112233");
    EXPECT_TRUE(flag);

    testing::dumpTreeModel(outputDir + "/grt_inspector_value_test2.1.txt", vinsp, columns, true);
    expectFilesEqual("typed list setting", outputDir + "/grt_inspector_value_test2.1.txt",
              dataDir + "/be/grt_inspector_value_test2.1.txt");

    // item count still ok?
    delete vinsp;

    EXPECT_EQ(list.count(), 10U);
}

TEST_F(GRTInspectorValueTest, TestInspectionOfIntTypedList) {
    bool flag;
    std::vector<ssize_t> columns;
    columns.push_back(ValueInspectorBE::Name);
    columns.push_back(ValueInspectorBE::Value);

    // create a test list
    BaseListRef list(create_int_list(10));

    EXPECT_EQ(list.count(), 10U);
    EXPECT_EQ(list.type(), ListType);
    EXPECT_EQ(list.content_type(), IntegerType);

    ValueInspectorBE *vinsp = ValueInspectorBE::create(list, false, false);

    // test listing
    size_t c = vinsp->count();
    EXPECT_EQ(10U, c);

    std::string name, value;

    testing::dumpTreeModel(outputDir + "/grt_inspector_value_test3.txt", vinsp, columns, true);
    expectFilesEqual("int typed list", outputDir + "/grt_inspector_value_test3.txt", dataDir + "/be/grt_inspector_value_test3.txt");

    NodeId node;
    node = vinsp->get_node(-1);
    EXPECT_FALSE(node.is_valid());

    node = vinsp->get_node(10);
    EXPECT_FALSE(node.is_valid());

    node = vinsp->get_node(11);
    EXPECT_FALSE(node.is_valid());

    // node= vinsp->get_child(4, 0);
    // EXPECT_FALSE(node.is_valid());

    // node= vinsp->get_child(1, 1);
    // EXPECT_FALSE(node.is_valid());

    flag = vinsp->set_field(0, ValueInspectorBE::Name, (ssize_t)123);
    EXPECT_FALSE(flag);

    // test change int value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, (ssize_t)112211);
    EXPECT_TRUE(flag);

    // test change string value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, "hello");
    EXPECT_FALSE(flag);

    // test change double value
    flag = vinsp->set_field(1, ValueInspectorBE::Value, 1.234);
    EXPECT_FALSE(flag);

    // test change int value from string
    flag = vinsp->set_convert_field(2, ValueInspectorBE::Value, "112233");
    EXPECT_TRUE(flag);

    testing::dumpTreeModel(outputDir + "/grt_inspector_value_test3.1.txt", vinsp, columns, true);
    expectFilesEqual("int typed list set", outputDir + "/grt_inspector_value_test3.1.txt",
              dataDir + "/be/grt_inspector_value_test3.1.txt");

    delete vinsp;

    EXPECT_EQ(list.count(), 10U);
}

// Test Dicts
// ----------

TEST_F(GRTInspectorValueTest, TestInspectionOfDict) {
    bool flag;
    std::vector<ssize_t> columns;
    columns.push_back(ValueInspectorBE::Name);
    columns.push_back(ValueInspectorBE::Value);

    // create a test dict
    DictRef dict(create_dict_with_varied_data());

    EXPECT_EQ(dict.count(), 6U);
    EXPECT_EQ(dict.type(), DictType);

    ValueInspectorBE *vinsp = ValueInspectorBE::create(dict, false, false);

    // test listing
    size_t c = vinsp->count();
    EXPECT_EQ(6U, c);

    std::string name, value;

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test10.txt", vinsp, columns, true);
    expectFilesEqual("dict check", data->outputDir + "/grt_inspector_value_test10.txt", data->dataDir + "/be/grt_inspector_value_test10.txt");

    NodeId node;
    node = vinsp->get_node(-1);
    EXPECT_FALSE(node.is_valid());

    //  node= vinsp->get_child(1, 0);
    //  EXPECT_FALSE(node.is_valid());

    // test change int value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, (ssize_t)112211);
    EXPECT_TRUE(flag);

    flag = vinsp->set_field(4, ValueInspectorBE::Value, (ssize_t)8888);
    EXPECT_TRUE(flag);

    // test change string value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, "hello");
    EXPECT_TRUE(flag);

    // test change double value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, 1.234);
    EXPECT_TRUE(flag);

    // test change int value from string
    flag = vinsp->set_convert_field(1, ValueInspectorBE::Value, "112233");
    EXPECT_TRUE(flag);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test10.1.txt", vinsp, columns, true);
    expectFilesEqual("dict check set", data->outputDir + "/grt_inspector_value_test10.1.txt",
                      data->dataDir + "/be/grt_inspector_value_test10.1.txt");

    // item count still ok?

    EXPECT_EQ(dict.count(), 6U);

    // test add new value
    NodeId nkey;
    flag = vinsp->add_item(nkey);
    EXPECT_TRUE(flag);
    EXPECT_EQ(6U, nkey[0]);

    flag = vinsp->set_field(nkey, ValueInspectorBE::Name, "newk");
    EXPECT_TRUE(flag);
    flag = vinsp->set_field(nkey, ValueInspectorBE::Value, "new value");
    EXPECT_TRUE(flag);

    EXPECT_EQ(dict.count(), 7U);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test10.2.txt", vinsp, columns, true);
    expectFilesEqual("dict check add", data->outputDir + "/grt_inspector_value_test10.2.txt",
                      data->dataDir + "/be/grt_inspector_value_test10.2.txt");
    // test delete value
    flag = vinsp->delete_item(3);
    EXPECT_TRUE(flag);

    EXPECT_EQ(dict.count(), 6U);

    flag = vinsp->delete_item(8);
    EXPECT_FALSE(flag);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test10.3.txt", vinsp, columns, true);
    expectFilesEqual("dict check del", data->outputDir + "/grt_inspector_value_test10.3.txt",
                      data->dataDir + "/be/grt_inspector_value_test10.3.txt");

    delete vinsp;

    EXPECT_EQ(dict.count(), 6U);
}

TEST_F(GRTInspectorValueTest, TestInspectionOfTypedDict) {
    bool flag;
    std::vector<ssize_t> columns;
    columns.push_back(ValueInspectorBE::Name);
    columns.push_back(ValueInspectorBE::Value);

    // create a test dict
    DictRef dict(create_dict_with_int_data());

    EXPECT_EQ(dict.count(), 9U);
    EXPECT_EQ(dict.type(), DictType);
    EXPECT_EQ(dict.content_type(), IntegerType);

    ValueInspectorBE *vinsp = ValueInspectorBE::create(dict, false, false);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test11.txt", vinsp, columns, true);
    expectFilesEqual("object check", data->outputDir + "/grt_inspector_value_test11.txt", data->dataDir + "/be/grt_inspector_value_test11.txt");

    // test listing
    size_t c = vinsp->count();
    EXPECT_EQ(9U, c);

    std::string name, value;
    NodeId node;

    // test get random item
    node = vinsp->get_node(9);
    EXPECT_FALSE(node.is_valid());

    node = vinsp->get_node(-1);
    EXPECT_FALSE(node.is_valid());

    // test change int value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, (ssize_t)112211);
    EXPECT_TRUE(flag);

    flag = vinsp->set_field(4, ValueInspectorBE::Value, (ssize_t)8888);
    EXPECT_TRUE(flag);

    // test change string value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, "hello");
    EXPECT_FALSE(flag);

    // test change double value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, 1.234);
    EXPECT_FALSE(flag);

    // test change int value from string
    flag = vinsp->set_convert_field(1, ValueInspectorBE::Value, "112233");
    EXPECT_TRUE(flag);
    // in this case, the dict is untyped, so it should just appear as string

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test11.1.txt", vinsp, columns, true);
    expectFilesEqual("object check set", data->outputDir + "/grt_inspector_value_test11.1.txt",
                      data->dataDir + "/be/grt_inspector_value_test11.1.txt");

    // item count still ok?
    EXPECT_EQ((int)dict.count(), 9);

    // test add new value
    NodeId nkey;
    flag = vinsp->add_item(nkey);
    EXPECT_TRUE(flag);
    EXPECT_EQ(9U, nkey[0]);

    flag = vinsp->set_field(nkey, ValueInspectorBE::Value, "new value");
    EXPECT_FALSE(flag);

    flag = vinsp->set_field(nkey, ValueInspectorBE::Value, "1234");
    EXPECT_FALSE(flag);

    flag = vinsp->set_field(nkey, ValueInspectorBE::Name, "anewk");
    EXPECT_TRUE(flag);

    flag = vinsp->set_field(nkey, ValueInspectorBE::Value, (ssize_t)1234);
    EXPECT_TRUE(flag);

    EXPECT_EQ(dict.count(), 10U);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test11.2.txt", vinsp, columns, true);
    expectFilesEqual("object check add", data->outputDir + "/grt_inspector_value_test11.2.txt",
                      data->dataDir + "/be/grt_inspector_value_test11.2.txt");

    flag = vinsp->add_item(nkey);
    EXPECT_TRUE(flag);

    flag = vinsp->add_item(nkey);
    EXPECT_FALSE(flag);

    vinsp->refresh();

    flag = vinsp->add_item(nkey);
    EXPECT_TRUE(flag);

    flag = vinsp->add_item(nkey);
    EXPECT_FALSE(flag);

    flag = vinsp->set_convert_field(nkey, ValueInspectorBE::Name, "aaa");
    flag = vinsp->set_convert_field(nkey, ValueInspectorBE::Value, "22");
    EXPECT_TRUE(flag);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test11.3.txt", vinsp, columns, true);
    expectFilesEqual("object check add", data->outputDir + "/grt_inspector_value_test11.3.txt",
                      data->dataDir + "/be/grt_inspector_value_test11.3.txt");

    // test delete value
    flag = vinsp->delete_item(3);
    EXPECT_TRUE(flag);

    EXPECT_EQ(dict.count(), 10U);

    flag = vinsp->delete_item(10);
    EXPECT_FALSE(flag);

    flag = vinsp->delete_item(9);
    EXPECT_TRUE(flag);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test11.4.txt", vinsp, columns, true);
    expectFilesEqual("object check del", data->outputDir + "/grt_inspector_value_test11.4.txt",
                      data->dataDir + "/be/grt_inspector_value_test11.4.txt");

    delete vinsp;

    EXPECT_EQ(dict.count(), 9U);
}

// Test Objects
// ------------

TEST_F(GRTInspectorValueTest, TestInspectionOfObjectUngrouped) {
    std::vector<ssize_t> columns;
    columns.push_back(ValueInspectorBE::Name);
    columns.push_back(ValueInspectorBE::Value);

    test_BookRef book(grt::Initialized);

    EXPECT_TRUE(book.is_valid());

    ValueInspectorBE *vinsp = ValueInspectorBE::create(book, false, false);
    bool flag;

    // test listing
    size_t c = vinsp->count();
    EXPECT_EQ(6U, c);

    std::string name, value;

    NodeId node;
    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test20.txt", vinsp, columns, true);
    expectFilesEqual("object check", data->outputDir + "/grt_inspector_value_test20.txt", data->dataDir + "/be/grt_inspector_value_test20.txt");

    node = vinsp->get_node(9);
    EXPECT_FALSE(node.is_valid());

    node = vinsp->get_node(-1);
    EXPECT_FALSE(node.is_valid());

    // test bad change int value
    flag = vinsp->set_field(5, ValueInspectorBE::Value, (ssize_t)112211);
    EXPECT_FALSE(flag);

    flag = vinsp->set_field(5, ValueInspectorBE::Value, "The Illiad");
    EXPECT_TRUE(flag);

    flag = vinsp->get_field(5, ValueInspectorBE::Value, value);
    EXPECT_TRUE(flag);
    EXPECT_EQ("The Illiad", value);

    // test change int value
    flag = vinsp->set_field(2, ValueInspectorBE::Value, (ssize_t)123);
    EXPECT_TRUE(flag);

    // test change double value
    flag = vinsp->set_field(3, ValueInspectorBE::Value, 1.234);
    EXPECT_TRUE(flag);

    // test change int value from string
    flag = vinsp->set_convert_field(2, ValueInspectorBE::Value, "112233");
    EXPECT_TRUE(flag);
    // in this case, the dict is untyped, so it should just appear as string

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test20.1.txt", vinsp, columns, true);
    expectFilesEqual("object check set", data->outputDir + "/grt_inspector_value_test20.1.txt",
                      data->dataDir + "/be/grt_inspector_value_test20.1.txt");

    // test add new value
    NodeId nkey;
    flag = vinsp->add_item(nkey);
    EXPECT_FALSE(flag);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test20.2.txt", vinsp, columns, true);
    expectFilesEqual("object check add", data->outputDir + "/grt_inspector_value_test20.2.txt",
                      data->dataDir + "/be/grt_inspector_value_test20.2.txt");

    // test delete value
    flag = vinsp->delete_item(3);
    EXPECT_FALSE(flag);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test20.3.txt", vinsp, columns, true);
    expectFilesEqual("object check del", data->outputDir + "/grt_inspector_value_test20.3.txt",
                      data->dataDir + "/be/grt_inspector_value_test20.3.txt");

    delete vinsp;
}

TEST_F(GRTInspectorValueTest, TestInspectionOfObjectGrouped) {
    std::vector<ssize_t> columns;
    columns.push_back(ValueInspectorBE::Name);
    columns.push_back(ValueInspectorBE::Value);

    test_BookRef book(grt::Initialized);
    EXPECT_TRUE(book.is_valid());

    ValueInspectorBE *vinsp = ValueInspectorBE::create(book, true, true);

    // test listing
    size_t c = vinsp->count();
    EXPECT_EQ(3U, c);

    std::string name, value;
    bool flag;

    NodeId node, gnode;

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test21.txt", vinsp, columns, true);
    expectFilesEqual("grouped object check", data->outputDir + "/grt_inspector_value_test21.txt",
                      data->dataDir + "/be/grt_inspector_value_test21.txt");

    try {
      node = vinsp->get_child(NodeId(1), 3);
      EXPECT_FALSE(node.is_valid());
    } catch (std::range_error &) {
      // expected
    }

    // test bad change int value
    flag = vinsp->set_field(0, ValueInspectorBE::Value, (ssize_t)112211);
    EXPECT_FALSE(flag);

    node = vinsp->get_child(0, 0);

    flag = vinsp->set_field(node, ValueInspectorBE::Value, "The Illiad");
    EXPECT_TRUE(flag);

    // test change int value
    node = vinsp->get_child(1, 1);
    flag = vinsp->set_field(node, ValueInspectorBE::Value, (ssize_t)123);
    EXPECT_TRUE(flag);

    // test change double value
    node = vinsp->get_child(1, 2);
    flag = vinsp->set_field(node, ValueInspectorBE::Value, 1.234);
    EXPECT_TRUE(flag);

    // test change int value from string
    node = vinsp->get_child(1, 1);
    flag = vinsp->set_convert_field(node, ValueInspectorBE::Value, "112233");
    EXPECT_TRUE(flag);

    // test change group name
    node = vinsp->get_node(1);
    flag = vinsp->set_convert_field(node, ValueInspectorBE::Value, "HELLO");
    EXPECT_FALSE(flag);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test21.1.txt", vinsp, columns, true);
    expectFilesEqual("grouped object check set", data->outputDir + "/grt_inspector_value_test21.1.txt",
                      data->dataDir + "/be/grt_inspector_value_test21.1.txt");

    // test add new value
    NodeId nkey;
    flag = vinsp->add_item(nkey);
    EXPECT_FALSE(flag);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test21.2.txt", vinsp, columns, true);
    expectFilesEqual("grouped object check add", data->outputDir + "/grt_inspector_value_test21.2.txt",
                      data->dataDir + "/be/grt_inspector_value_test21.2.txt");

    // test delete value
    flag = vinsp->delete_item(3);
    EXPECT_FALSE(flag);

    testing::dumpTreeModel(data->outputDir + "/grt_inspector_value_test21.3.txt", vinsp, columns, true);
    expectFilesEqual("grouped object check del", data->outputDir + "/grt_inspector_value_test21.3.txt",
                      data->dataDir + "/be/grt_inspector_value_test21.3.txt");

    delete vinsp;
  
}
}

