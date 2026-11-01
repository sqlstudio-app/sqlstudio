/*
 * Copyright (c) 2019, 2025, Oracle and/or its affiliates. All rights reserved.
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

/*
 * Copyright (c) 2026 dev4fun. All rights reserved.
 */

#include <initializer_list>

#include "gtest/gtest.h"
#include "wb_test_helpers.h"

#include "base/string_utilities.h"

#include "grt.h"

#include "grtdb/editor_table.h"
#include "grtdb/db_object_helpers.h"

#include "sqlide/recordset_be.h"

using namespace grt;
using namespace bec;
using namespace base;

namespace {

  class TestTableColumnsListBE : public TableColumnsListBE {
  public:
    TestTableColumnsListBE(TableEditorBE *ed) : TableColumnsListBE(ed) {
    }

    virtual std::vector<std::string> get_datatype_names() {
      return std::vector<std::string>();
    }
  };

  class TestTableEditor : public TableEditorBE {
    db_TableRef _table;
    TestTableColumnsListBE _columns;
    IndexListBE _indexes;

  public:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4355)
#endif
    TestTableEditor(db_TableRef table, db_mgmt_RdbmsRef rdbms)
      : TableEditorBE(table), _table(table), _columns(this), _indexes(this) {
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    db_TableRef get_table() {
      return _table;
    }

    virtual TableColumnsListBE *get_columns() {
      return &_columns;
    }

    virtual IndexListBE *get_indexes() {
      return &_indexes;
    }

    virtual void edit_object(const ObjectRef &v) {
    }

    virtual std::vector<std::string> get_index_types() {
      std::vector<std::string> index_types;
      index_types.push_back("type1");
      index_types.push_back("type2");
      index_types.push_back("type3");
      index_types.push_back("PRIMARY");
      return index_types;
    }

    void set_table_option_by_name(const std::string &name, const std::string &value) {
      // TODO: implement
    }

    std::string get_table_option_by_name(const std::string &name) {
      // TODO: implement
      return std::string();
    }

    std::vector<std::string> get_charsets_list() {
      return std::vector<std::string>();
    }

    virtual bool check_column_referenceable_by_fk(const db_ColumnRef &column1, const db_ColumnRef &column2) {
      // TODO: implement
      return false;
    }

    virtual db_TableRef create_stub_table(const std::string &schema, const std::string &table) {
      return db_TableRef(); // TODO: implement and add tests.
    }
  };

  struct TestData {
    std::unique_ptr<SqlStudioTester> tester;
    db_TableRef table;
    std::unique_ptr<TestTableEditor> editor;

    db_IndexColumnRef findIndexColumnFor(const grt::ListRef<db_IndexColumn> &cols, const std::string &name) {
      for (size_t c = cols.count(), i = 0; i < c; i++) {
        if (cols[i]->referencedColumn()->name() == name)
          return cols[i];
      }
      return db_IndexColumnRef();
    }
  };

  class TableEditorBackendTest : public ::testing::Test {
  protected:
    TestData *data = new TestData();

    void SetUp() override {
      data->tester.reset(new SqlStudioTester());
      data->tester->initializeRuntime();
      data->tester->createNewDocument();

      EXPECT_TRUE(data->tester->wb->get_document().is_valid()) /* "document ok" */;
      EXPECT_TRUE(data->tester->wb->get_document()->physicalModels()[0]->catalog().is_valid()) /* "catalog" */;

      db_mysql_SchemaRef schema = db_mysql_SchemaRef(grt::Initialized);
      EXPECT_TRUE(schema.is_valid()) /* "schema ok" */;
      schema->owner(data->tester->wb->get_document()->physicalModels()[0]->catalog());
      data->tester->wb->get_document()->physicalModels()[0]->catalog()->schemata().insert(schema);

      data->table = db_mysql_TableRef(grt::Initialized);
      EXPECT_TRUE(data->table.is_valid()) /* "table ok" */;
      data->table->owner(schema);

      data->editor.reset(new TestTableEditor(data->table, data->tester->getRdbms()));
    }

    void createColumns(std::initializer_list<const char *> names) {
      for (const char *name : names)
        data->editor->add_column(name);

      auto *columns = data->editor->get_columns();
      ASSERT_TRUE(columns->set_field(0, TableColumnsListBE::Type, "INT"));
      ASSERT_TRUE(columns->set_field(0, TableColumnsListBE::IsPK, 1));
    }

    void TearDown() override {
      delete data;
    }
  };

  TEST_F(TableEditorBackendTest, SetNameAndComment) {
    EXPECT_EQ("", *data->table->name()) /* "initial table name" */;
    EXPECT_EQ("", data->editor->get_name()) /* "get table name" */;

    data->editor->set_name("employee");
    EXPECT_EQ("employee", data->editor->get_name()) /* "table name" */;
    EXPECT_EQ("", data->editor->get_comment()) /* "table comment" */;

    data->editor->set_comment("this is a test table");
    EXPECT_EQ("employee", data->editor->get_name()) /* "table name" */;
    EXPECT_EQ("this is a test table", data->editor->get_comment()) /* "table comment" */;
  }

  TEST_F(TableEditorBackendTest, EditAndInspectColumns) {
    TableColumnsListBE *clist = data->editor->get_columns();
    NodeId node;
    std::string name;
    std::string type;
    ssize_t ispk;
    bool notnull;
    std::string flags;
    std::string defvalue;
    // int defnull;
    bool flag;

    // count is always +1 because of placeholder
    EXPECT_EQ(1U, clist->count()) /* "initial column count" */;

    // create column
    node = data->editor->add_column("id");
    EXPECT_EQ(0U, node[0]) /* "new column id" */;
    EXPECT_EQ(2U, clist->count()) /* "column count" */;

    EXPECT_TRUE(data->table->columns().get(0).is_valid()) /* "new column" */;

    flag = clist->set_field(node, TableColumnsListBE::IsPK, 1);
    EXPECT_TRUE(flag) /* "change id pk" */;

    flag = clist->set_field(node, TableColumnsListBE::Type, "INT");
    EXPECT_TRUE(flag) /* "change id type" */;

    // create another column
    node = data->editor->add_column("name");
    EXPECT_EQ(1U, node[0]) /* "new column id" */;
    EXPECT_EQ(3U, clist->count()) /* "column count" */;

    flag = clist->get_field(node, TableColumnsListBE::Name, name);
    EXPECT_TRUE(flag) /* "name column name get" */;
    EXPECT_EQ("name", name) /* "name column name" */;

    flag = clist->get_field(node, TableColumnsListBE::Type, type);
    EXPECT_TRUE(flag) /* "name column type get" */;
    EXPECT_EQ("", type) /* "name column type" */;

    flag = clist->get_field(node, TableColumnsListBE::IsPK, ispk);
    EXPECT_TRUE(flag) /* "name column pk get" */;
    EXPECT_EQ(0U, ispk) /* "name column pk" */;

    flag = clist->get_field(node, TableColumnsListBE::IsNotNull, notnull);
    EXPECT_TRUE(flag) /* "name column notnull get" */;
    EXPECT_FALSE(notnull) /* "name column notnull" */;

    flag = clist->get_field(node, TableColumnsListBE::Flags, flags);
    EXPECT_TRUE(flag) /* "name column flags get" */;
    EXPECT_EQ("", flags) /* "name column flags" */;

    flag = clist->get_field(node, TableColumnsListBE::Default, defvalue);
    EXPECT_TRUE(flag) /* "name column default get" */;
    EXPECT_EQ("", defvalue) /* "name column default" */;

    flag = clist->set_field(node, TableColumnsListBE::IsNotNull, 1);
    EXPECT_TRUE(flag) /* "set name notnull" */;

    flag = clist->set_field(node, TableColumnsListBE::Default, "new");
    EXPECT_TRUE(flag) /* "change name column default" */;

    // create column
    node = data->editor->add_column("salary");
    flag = clist->set_field(node, TableColumnsListBE::IsNotNull, 0);
    EXPECT_TRUE(flag);

    // create column
    node = data->editor->add_column("department");

    // create column
    node = data->editor->add_column("email");

    EXPECT_EQ(6U, clist->count()) /* "column count" */;

    /// full column listing

    bool ispkb;
    bool notnullb;
    bool isunique;
    bool isbin;
    bool isunsigned;
    bool iszerofill;

    std::string charset;
    std::string collation;
    std::string comment;

    flag = clist->get_row(0, name, type, ispkb, notnullb, isunique, isbin, isunsigned, iszerofill, flags, defvalue,
                          charset, collation, comment);
    EXPECT_TRUE(flag);
    EXPECT_EQ("id", name);
    EXPECT_TRUE(ispkb);
    EXPECT_TRUE(notnullb);
    EXPECT_EQ("", flags);
    EXPECT_EQ("", defvalue);

    flag = clist->get_row(1, name, type, ispkb, notnullb, isunique, isbin, isunsigned, iszerofill, flags, defvalue,
                          charset, collation, comment);
    EXPECT_TRUE(flag);
    EXPECT_EQ("name", name);
    EXPECT_FALSE(ispkb);
    EXPECT_TRUE(notnullb);
    EXPECT_EQ("", flags);
    EXPECT_EQ("new", defvalue);
    EXPECT_EQ("", charset);
    EXPECT_EQ("", collation);

    flag = clist->get_row(2, name, type, ispkb, notnullb, isunique, isbin, isunsigned, iszerofill, flags, defvalue,
                          charset, collation, comment);
    EXPECT_TRUE(flag);
    EXPECT_EQ("salary", name);
    EXPECT_FALSE(ispkb);
    EXPECT_FALSE(notnullb);
    EXPECT_EQ("", flags);
    EXPECT_EQ("", defvalue);

    flag = clist->get_row(3, name, type, ispkb, notnullb, isunique, isbin, isunsigned, iszerofill, flags, defvalue,
                          charset, collation, comment);
    EXPECT_TRUE(flag);
    EXPECT_EQ("department", name);
    EXPECT_FALSE(ispkb);
    EXPECT_FALSE(notnullb);
    EXPECT_EQ("", flags);
    EXPECT_EQ("", defvalue);
    // EXPECT_FALSE(defnullb);

    flag = clist->get_row(4, name, type, ispkb, notnullb, isunique, isbin, isunsigned, iszerofill, flags, defvalue,
                          charset, collation, comment);
    EXPECT_TRUE(flag);
    EXPECT_EQ("email", name);
    EXPECT_FALSE(ispkb);
    EXPECT_FALSE(notnullb);
    EXPECT_EQ("", flags);
    EXPECT_EQ("", defvalue);

    flag = clist->get_row(5, name, type, ispkb, notnullb, isunique, isbin, isunsigned, iszerofill, flags, defvalue,
                          charset, collation, comment);
    EXPECT_FALSE(flag);

    // unset pk
    flag = clist->get_field(0, TableColumnsListBE::IsPK, ispk);
    EXPECT_TRUE(flag);
    EXPECT_NE(0U, ispk);

    flag = clist->set_field(0, TableColumnsListBE::IsPK, 0);
    EXPECT_TRUE(flag);

    flag = clist->get_field(0, TableColumnsListBE::IsPK, ispk);
    EXPECT_TRUE(flag);
    EXPECT_EQ(0U, ispk);

    // unset again
    flag = clist->set_field(0, TableColumnsListBE::IsPK, 0);
    EXPECT_TRUE(flag);

    flag = clist->get_field(0, TableColumnsListBE::IsPK, ispk);
    EXPECT_TRUE(flag);
    EXPECT_EQ(0U, ispk);

    // set back
    flag = clist->set_field(0, TableColumnsListBE::IsPK, 1);
    EXPECT_TRUE(flag);

    flag = clist->get_field(0, TableColumnsListBE::IsPK, ispk);
    EXPECT_TRUE(flag);
    EXPECT_NE(0U, ispk);

    // and again
    flag = clist->set_field(0, TableColumnsListBE::IsPK, 1);
    EXPECT_TRUE(flag);

    flag = clist->get_field(0, TableColumnsListBE::IsPK, ispk);
    EXPECT_TRUE(flag);
    EXPECT_NE(0U, ispk);

    name = data->editor->get_column_with_name("name")->name();
    EXPECT_EQ("name", name);
  }

  TEST_F(TableEditorBackendTest, RemoveColumnKeepsOrder) {
    ASSERT_NO_FATAL_FAILURE(createColumns({"id", "name", "salary", "department", "email"}));
    TableColumnsListBE *clist = data->editor->get_columns();
    bool flag;

    ASSERT_EQ(6U, clist->count()) << "column count";

    data->editor->remove_column(3);

    EXPECT_EQ(5U, clist->count()) << "new column count";

    std::string name;

    flag = clist->get_field(2, TableColumnsListBE::Name, name);
    EXPECT_TRUE(flag);
    EXPECT_EQ("salary", name);

    flag = clist->get_field(3, TableColumnsListBE::Name, name);
    EXPECT_TRUE(flag);
    EXPECT_EQ("email", name);

    // Placeholder row, doesn't return a value, so the previous one stays.
    flag = clist->get_field(4, TableColumnsListBE::Name, name);
    EXPECT_FALSE(flag);
    EXPECT_EQ("email", name);
  }

  TEST_F(TableEditorBackendTest, ReorderColumnsPreservesPrimaryKeyOrder) {
    ASSERT_NO_FATAL_FAILURE(createColumns({"id", "name", "email"}));
    auto *columns = data->editor->get_columns();
    ASSERT_TRUE(columns->set_field(2, TableColumnsListBE::Type, "INT"));
    ASSERT_TRUE(columns->set_field(2, TableColumnsListBE::IsPK, 1));

    const db_ColumnRef id = data->table->columns()[0];
    const db_ColumnRef name = data->table->columns()[1];
    const db_ColumnRef email = data->table->columns()[2];
    const db_IndexRef primaryKey = data->table->primaryKey();
    ASSERT_TRUE(primaryKey.is_valid());
    ASSERT_EQ(2U, primaryKey->columns().count());

    columns->reorder(NodeId(2), 0);

    ASSERT_EQ(4U, columns->count()); // Includes the placeholder row.
    EXPECT_EQ(email, data->table->columns()[0]);
    EXPECT_EQ(id, data->table->columns()[1]);
    EXPECT_EQ(name, data->table->columns()[2]);
    ASSERT_EQ(2U, primaryKey->columns().count());
    EXPECT_EQ(email, primaryKey->columns()[0]->referencedColumn());
    EXPECT_EQ(id, primaryKey->columns()[1]->referencedColumn());

    columns->reorder(NodeId(0), 2);

    EXPECT_EQ(id, data->table->columns()[0]);
    EXPECT_EQ(name, data->table->columns()[1]);
    EXPECT_EQ(email, data->table->columns()[2]);
    ASSERT_EQ(2U, primaryKey->columns().count());
    EXPECT_EQ(id, primaryKey->columns()[0]->referencedColumn());
    EXPECT_EQ(email, primaryKey->columns()[1]->referencedColumn());
  }

  TEST_F(TableEditorBackendTest, DuplicateColumnPreservesSettingsAndUsesUniqueNames) {
    ASSERT_NO_FATAL_FAILURE(createColumns({"id", "salary"}));
    auto *columns = data->editor->get_columns();
    ASSERT_TRUE(columns->set_field(1, TableColumnsListBE::Type, "INT"));
    ASSERT_TRUE(columns->set_field(1, TableColumnsListBE::IsNotNull, 1));
    ASSERT_TRUE(columns->set_field(1, TableColumnsListBE::Default, "100"));
    const db_ColumnRef original = data->table->columns()[1];
    original->comment("Annual salary");
    original->oldName("previous_salary");

    const NodeId first = data->editor->duplicate_column(original);
    ASSERT_EQ(2U, first[0]);
    ASSERT_EQ(3U, data->table->columns().count());
    const db_ColumnRef copy = data->table->columns()[first[0]];
    EXPECT_NE(original.id(), copy.id());
    EXPECT_EQ("salary_copy1", *copy->name());
    EXPECT_EQ("", *copy->oldName());
    EXPECT_EQ(data->table, copy->owner());
    EXPECT_EQ(original->simpleType(), copy->simpleType());
    EXPECT_EQ(1, *copy->isNotNull());
    EXPECT_EQ("100", *copy->defaultValue());
    EXPECT_EQ("Annual salary", *copy->comment());

    const NodeId second = data->editor->duplicate_column(original);
    ASSERT_EQ(3U, second[0]);
    ASSERT_EQ(5U, columns->count()); // Includes the placeholder row.
    EXPECT_EQ("salary_copy2", *data->table->columns()[second[0]]->name());
    EXPECT_NE(copy.id(), data->table->columns()[second[0]].id());

    copy->comment("Changed copy");
    EXPECT_EQ("salary", *original->name());
    EXPECT_EQ("previous_salary", *original->oldName());
    EXPECT_EQ("Annual salary", *original->comment());
    ASSERT_EQ(1U, data->table->primaryKey()->columns().count());
    EXPECT_EQ(data->table->columns()[0], data->table->primaryKey()->columns()[0]->referencedColumn());
  }

TEST_F(TableEditorBackendTest, AddAndConfigureIndexes) {
  ASSERT_NO_FATAL_FAILURE(createColumns({"id", "name", "salary", "email"}));
  IndexListBE *index = data->editor->get_indexes();
  IndexColumnsListBE *icolumns = index->get_columns();
  std::string name, type, comment;
  bool flag;

  ASSERT_EQ(5U, data->editor->get_columns()->count()) << "column count";
  ASSERT_EQ(2U, data->editor->get_indexes()->count()) << "index count";

  NodeId node = data->editor->add_index("idx1");

  index->select_index(node);
  //  index->set_field(0, IndexListBE::Name).toEqual("id");

  EXPECT_EQ(3U, index->count()) << "index count";

  flag = index->get_field(1, IndexListBE::Name, name);
  EXPECT_TRUE(flag);
  EXPECT_EQ("idx1", name);
  flag = index->get_field(1, IndexListBE::Type, type);
  EXPECT_TRUE(flag);
  EXPECT_EQ("type1", type);
  flag = index->get_field(1, IndexListBE::Comment, comment);
  EXPECT_TRUE(flag);
  EXPECT_EQ("", comment);

  // change

  flag = index->set_field(1, IndexListBE::Name, "index1");
  EXPECT_TRUE(flag);
  flag = index->set_field(1, IndexListBE::Type, "bleqw");
  EXPECT_FALSE(flag);
  flag = index->set_field(1, IndexListBE::Type, "type2");
  EXPECT_TRUE(flag);
  flag = index->set_field(1, IndexListBE::Comment, "test index");
  EXPECT_TRUE(flag);

  index->select_index(1);

  // add column 1 (name) to index
  icolumns->set_column_enabled(1, true);

  // change column stuff
  flag = icolumns->set_field(1, IndexColumnsListBE::Length, 20);
  EXPECT_TRUE(flag);
  flag = icolumns->set_field(1, IndexColumnsListBE::Descending, 1);
  EXPECT_TRUE(flag);
  std::string x;
  flag = icolumns->get_field(1, IndexColumnsListBE::Descending, x);
  EXPECT_EQ("1", x);

  EXPECT_EQ(data->editor->get_columns()->count() - 1, icolumns->count()) << "index column count";

  // list indexes and columns

  // ** the pk index
  flag = index->get_field(0, IndexListBE::Name, name);
  EXPECT_TRUE(flag);
  EXPECT_EQ("PRIMARY", name);
  flag = index->get_field(0, IndexListBE::Type, type);
  EXPECT_TRUE(flag);
  EXPECT_EQ("PRIMARY", type);
  flag = index->get_field(0, IndexListBE::Comment, comment);
  EXPECT_TRUE(flag);
  EXPECT_EQ("", comment);

  std::string buf;

  index->select_index(0);
  EXPECT_EQ(data->editor->get_columns()->count() - 1, icolumns->count()) << "index column count";

  // check if list of index columns matches list of table columns
  db_TableRef table(data->editor->get_table());
  for (size_t i = 0; i < table->columns().count(); i++) {
    flag = icolumns->get_field(i, IndexColumnsListBE::Name, name);
    EXPECT_TRUE(flag) << strfmt("index column[%lu] name", i);
    EXPECT_EQ(*data->table->columns()[i]->name(), name) << strfmt("index column[%lu] name", i);
    flag = icolumns->get_field(i, IndexColumnsListBE::Descending, buf);
    EXPECT_TRUE(flag) << strfmt("index column[%lu] desc", i);
    EXPECT_EQ("0", buf) << strfmt("index column[%lu] desc", i);
    flag = icolumns->get_field(i, IndexColumnsListBE::Length, buf);
    EXPECT_TRUE(flag) << strfmt("index column[%lu] length", i);
    EXPECT_EQ("0", buf) << strfmt("index column[%lu] length", i);
    flag = icolumns->get_column_enabled(i);
    if (name == "id") {
      EXPECT_TRUE(flag) << strfmt("index column[%lu] enabled", i);
    } else {
      EXPECT_FALSE(flag) << strfmt("index column[%lu] enabled", i);
    }
  }

    // ** the idx we added
    flag = index->get_field(1, IndexListBE::Name, name);
    EXPECT_TRUE(flag);
    EXPECT_EQ("index1", name);
    flag = index->get_field(1, IndexListBE::Type, type);
    EXPECT_TRUE(flag);
    EXPECT_EQ("type2", type);
    flag = index->get_field(1, IndexListBE::Comment, comment);
    EXPECT_TRUE(flag);
    EXPECT_EQ("test index", comment);

    index->select_index(1);

    for (size_t i = 0; i < table->columns().count(); i++) {
      flag = icolumns->get_field(i, IndexColumnsListBE::Name, name);
      EXPECT_TRUE(flag) << strfmt("index column[%lu] name", i);
      EXPECT_EQ(*data->table->columns()[i]->name(), name) << strfmt("index column[%lu] name", i);
      if (name == "name") {
        flag = icolumns->get_field(i, IndexColumnsListBE::Descending, buf);
        EXPECT_TRUE(flag) << strfmt("index column[%lu] desc", i);
        EXPECT_EQ("1", buf) << strfmt("index column[%lu] desc", i);
        flag = icolumns->get_field(i, IndexColumnsListBE::Length, buf);
        EXPECT_TRUE(flag) << strfmt("index column[%lu] length", i);
        EXPECT_EQ("20", buf) << strfmt("index column[%lu] length", i);
        flag = icolumns->get_column_enabled(i);
        EXPECT_TRUE(flag) << strfmt("index column[%lu] enabled", i);
      } else {
        flag = icolumns->get_field(i, IndexColumnsListBE::Descending, buf);
        EXPECT_TRUE(flag) << strfmt("index column[%lu] desc", i);
        EXPECT_EQ("0", buf) << strfmt("index column[%lu] desc", i);
        flag = icolumns->get_field(i, IndexColumnsListBE::Length, buf);
        EXPECT_TRUE(flag) << strfmt("index column[%lu] length", i);
        EXPECT_EQ("0", buf) << strfmt("index column[%lu] length", i);
        flag = icolumns->get_column_enabled(i);
        EXPECT_FALSE(flag) << strfmt("index column[%lu] enabled", i);
      }
    }

    // remove index
    data->editor->remove_index(1, false);

    EXPECT_EQ(2U, index->count()) << "index count";

    // add new one with convenience func
    std::vector<NodeId> columns;
    columns.push_back(NodeId(1)); // name
    columns.push_back(NodeId(3)); // email
    node = data->editor->add_index_with_columns(columns);

    EXPECT_EQ(1U, node[0]);

    EXPECT_EQ(3U, index->count()) << "index count";

    index->set_field(1, IndexListBE::Name, "namemail_index");

    flag = index->get_field(1, IndexListBE::Name, name);
    EXPECT_TRUE(flag);
    EXPECT_EQ("namemail_index", name);
    flag = index->get_field(1, IndexListBE::Type, type);
    EXPECT_TRUE(flag);
    EXPECT_EQ("type1", type);
    flag = index->get_field(1, IndexListBE::Comment, comment);
    EXPECT_TRUE(flag);
    EXPECT_EQ("", comment);

    index->select_index(1);

    for (size_t i = 0; i < (size_t)icolumns->count(); i++) {
      flag = icolumns->get_field(i, IndexColumnsListBE::Name, name);
      EXPECT_TRUE(flag) << strfmt("index column[%lu] name", i);
      flag = icolumns->get_field(i, IndexColumnsListBE::Descending, buf);
      EXPECT_TRUE(flag) << strfmt("index column[%lu] desc", i);
      EXPECT_EQ("0", buf) << strfmt("index column[%lu] desc", i);
      flag = icolumns->get_field(i, IndexColumnsListBE::Length, buf);
      EXPECT_TRUE(flag) << strfmt("index column[%lu] length", i);
      EXPECT_EQ("0", buf) << strfmt("index column[%lu] length", i);

      if (name == "name" || name == "email") {
        flag = icolumns->get_column_enabled(i);
        EXPECT_TRUE(flag) << strfmt("index column[%lu] enabled", i);
      } else {
        flag = icolumns->get_column_enabled(i);
        EXPECT_FALSE(flag) << strfmt("index column[%lu] enabled", i);
      }
    }
}

TEST_F(TableEditorBackendTest, IndexesStayConsistentAfterColumnRemoval) {
  ASSERT_NO_FATAL_FAILURE(createColumns({"id", "name", "salary", "email"}));
  const NodeId nameEmailIndex = data->editor->add_index_with_columns({NodeId(1), NodeId(3)});
  ASSERT_TRUE(data->editor->get_indexes()->set_field(nameEmailIndex, IndexListBE::Name, "namemail_index"));
  ASSERT_EQ(2U, data->table->indices().count());
  IndexListBE *index = data->editor->get_indexes();
  // IndexColumnsListBE *icolumns= index->get_columns();
  std::vector<NodeId> columns;
  columns.push_back(NodeId(3)); // email
  data->editor->add_index_with_columns(columns);

  EXPECT_EQ(index->count(), 4U) << "index count";
  EXPECT_EQ(data->editor->get_columns()->count(), 5U) << "column count";

  std::string name;
  bool flag;

  flag = index->get_field(1, IndexListBE::Name, name);
  EXPECT_TRUE(flag);
  EXPECT_EQ("namemail_index", name);
  index->select_index(1);
  EXPECT_EQ(data->table->indices().get(1)->columns().count(), 2U) << "get index[1] column.count()";

  data->editor->remove_column(3); // delete column email

  EXPECT_EQ(data->table->indices().get(1)->columns().count(), 1U) << "column count";
  EXPECT_EQ(index->count(), 3U) << "index count";

  flag = index->get_field(0, IndexListBE::Name, name);
  EXPECT_TRUE(flag);
  EXPECT_EQ("PRIMARY", name);
  index->select_index(0);
  EXPECT_EQ(data->table->indices().get(0)->columns().count(), 1U) << "get index[0] column.count()";

  flag = index->get_field(1, IndexListBE::Name, name);
  EXPECT_TRUE(flag);
  EXPECT_EQ("namemail_index", name);
  index->select_index(1);
  EXPECT_EQ(data->table->indices().get(1)->columns().count(), 1U) << "get index[1] column.count()";

  flag = index->get_field(2, IndexListBE::Name, name);
  EXPECT_TRUE(flag);
  EXPECT_EQ("", name);
}

  TEST_F(TableEditorBackendTest, ToggleIndexColumns) {
    db_TableRef table = db_mysql_TableRef(grt::Initialized);
    table->owner(data->tester->getSchema());

    EXPECT_TRUE(table.is_valid()) << "table ok";

    db_ColumnRef column(grt::Initialized);
    column->owner(table);
    column->name("col1");
    table->columns().insert(column);

    column = db_ColumnRef(grt::Initialized);
    column->owner(table);
    column->name("col2");
    table->columns().insert(column);

    column = db_ColumnRef(grt::Initialized);
    column->owner(table);
    column->name("col3");
    table->columns().insert(column);

    TestTableEditor ed(table, data->tester->getRdbms());

    EXPECT_EQ(ed.get_columns()->count(), 4U) << "column count";

    ed.add_index("hello");

    EXPECT_EQ(ed.get_indexes()->count(), 2U) << "index count";

    ed.get_indexes()->select_index(0);

    EXPECT_EQ(ed.get_indexes()->get_columns()->count(), 3U) << "index column item count";

    IndexColumnsListBE *ic = ed.get_indexes()->get_columns();
    std::string name;

    ic->get_field(0, IndexColumnsListBE::Name, name);
    EXPECT_EQ("col1", name);
    EXPECT_FALSE(ic->get_column_enabled(0)) << "col1 disabled";

    ic->get_field(1, IndexColumnsListBE::Name, name);
    EXPECT_EQ("col2", name);
    EXPECT_FALSE(ic->get_column_enabled(1)) << "col2 disabled";

    ic->get_field(2, IndexColumnsListBE::Name, name);
    EXPECT_EQ("col3", name);
    EXPECT_FALSE(ic->get_column_enabled(2)) << "col3 disabled";

    // enable 2 columns
    ic->set_column_enabled(1, true);
    EXPECT_TRUE(data->findIndexColumnFor(table->indices().get(0)->columns(), "col2").is_valid()) << "col2 in list";

    ic->set_column_enabled(0, true);
    EXPECT_TRUE(data->findIndexColumnFor(table->indices().get(0)->columns(), "col1").is_valid()) << "col1 in list";

    // disable one of them and make sure the right column is removed
    ic->set_column_enabled(0, false);
    EXPECT_TRUE(data->findIndexColumnFor(table->indices().get(0)->columns(), "col2").is_valid()) << "col2 in list";
    EXPECT_FALSE(data->findIndexColumnFor(table->indices().get(0)->columns(), "col1").is_valid()) << "col1 not in list";

    // disable all
    ic->set_column_enabled(1, false);

    // toggle last (will crash if buggy)
    ic->set_column_enabled(2, true);
    ic->set_column_enabled(2, false);
  }

  TEST_F(TableEditorBackendTest, AutoAddsPkIndexAndFkEntries) {
    db_mysql_TableRef table(grt::Initialized);

    table->owner(data->tester->getSchema());
    table->name("table");

    TestTableEditor editor(table, data->tester->getRdbms());

    editor.get_columns()->set_field(0, 0, "newcol");

    // auto-adds a PK
    editor.get_columns()->set_field(0, 1, "int(11)");
    ASSERT_EQ(table->indices().count(), 1U) << "autoadd PK index";
    EXPECT_EQ(*table->indices()[0]->name(), "PRIMARY") << "autoadd PK index";

    EXPECT_EQ(table->columns().count(), 1U) << "add column";

    EXPECT_EQ(table->indices().count(), 1U) << "add index";
    editor.get_indexes()->set_field(1, 0, "index");
    EXPECT_EQ(table->indices().count(), 2U) << "add index";

    EXPECT_EQ(table->foreignKeys().count(), 0U) << "add fk";
    editor.get_fks()->set_field(0, 0, "newfk");
    EXPECT_EQ(table->foreignKeys().count(), 1U) << "add fk";
  }

  TEST_F(TableEditorBackendTest, CanAccessCatalogUserTypes) {
    db_mysql_TableRef table(grt::Initialized);
    EXPECT_TRUE(table.is_valid()) << "table ok";

    table->name("table");
    table->owner(data->tester->getSchema());
    data->tester->getSchema()->tables().insert(data->table);

    TestTableEditor editor(table, data->tester->getRdbms());

    EXPECT_TRUE(editor.get_catalog().is_valid()) << "editor catalog is not ok";

    grt::ListRef<db_UserDatatype> userTypes(editor.get_catalog()->userDatatypes());

    db_mysql_ColumnRef column(grt::Initialized);
    column->owner(table);
    column->name("id");
    column->setParseType("int", data->tester->getRdbms()->simpleDatatypes());
    table->columns().insert(column);

    column = db_mysql_ColumnRef(grt::Initialized);
    column->owner(table);
    column->name("fk");
    column->setParseType("int", data->tester->getRdbms()->simpleDatatypes());
    table->columns().insert(column);

    EXPECT_EQ(editor.get_columns()->count(), 3U) << "columns list ok";

    std::vector<bec::NodeId> columns;
    columns.push_back(bec::NodeId(1));
    editor.add_fk_with_columns(columns);
    EXPECT_EQ(editor.get_fks()->count(), 2U) << "fk added";

    editor.get_fks()->select_fk(bec::NodeId(0));

    editor.get_fks()->set_field(0, bec::FKConstraintListBE::RefTable, "table");

    EXPECT_EQ(editor.get_fks()->get_columns()->count(), 2U) << "columns in fk";
  }
}
