/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "grtdb/db_object_helpers.h"
#include "model/wb_history_tree.h"
#include "model/wb_context_model.h"
#include "grtpp_undo_manager.h"
#include "grtpp_util.h"

#include "gtest/gtest.h"
#include "wb_test_helpers.h"
#include <filesystem>
#include "grt_test_helpers.h"

using namespace bec;
using namespace wb;
using namespace grt;

namespace {
struct WbUndoOthersData {
  std::unique_ptr<SqlStudioTester> tester;
  UndoManager *um = nullptr;
  OverviewBE *overview = nullptr;
  size_t lastUndoStackSize = 0;
  size_t lastRedoStackSize = 0;

  #include "wb_undo_helpers.h"

  void checkOverviewObject(const std::string &what, const NodeId &base_node, const std::string &list_path,
                           size_t initial_count = 0) {
    resetUndoAccounting();

    // Checks Overview object handling by adding a node, renaming it and then deleting it
    // with undo/redo for each operation

    NodeId add_node(base_node);
    add_node.append(0);

    NodeId added_node(base_node);
    added_node.append((int)initial_count + 1);

    std::string name;
    {
      grt::BaseListRef list = grt::BaseListRef::cast_from(get_value_by_path(tester->getPmodel(), list_path));
      EXPECT_EQ(list.count(), initial_count) << list_path + " " + what + " initial count";
    }

    // Add diagram
    overview->get_field(add_node, OverviewBE::Label, name);
    EXPECT_EQ(name, "Add " + what) << what + " add node";
    overview->activate_node(add_node);

    checkOnlyOneUndoAdded();

    // check that it was added
    overview->refresh_node(base_node, true);
    EXPECT_EQ(overview->count_children(base_node), initial_count + 2) << what + " node count";
    {
      grt::BaseListRef list = grt::BaseListRef::cast_from(get_value_by_path(tester->getPmodel(), list_path));
      EXPECT_EQ(list.count(), initial_count + 1) << list_path + " " + what + " count";
    }
    // check undo add
    checkUndo();
    overview->refresh_node(base_node, true);
    EXPECT_EQ(overview->count_children(base_node), initial_count + 1) << what + " node count";
    {
      grt::BaseListRef list = grt::BaseListRef::cast_from(get_value_by_path(tester->getPmodel(), list_path));
      EXPECT_EQ(list.count(), initial_count) << list_path + " " + what + " count after undo";
    }

    // check redo add
    checkRedo();
    overview->refresh_node(base_node, true);
    EXPECT_EQ(overview->count_children(base_node), initial_count + 2) << "diagram node count";
    {
      grt::BaseListRef list = grt::BaseListRef::cast_from(get_value_by_path(tester->getPmodel(), list_path));
      EXPECT_EQ(list.count(), initial_count + 1) << list_path + " " + what + " count after redo";
    }

    // check Renaming
    std::string old_name;
    overview->get_field(added_node, OverviewBE::Label, old_name);

    overview->set_field(added_node, OverviewBE::Label, "new name");
    overview->refresh_node(added_node, false);
    checkOnlyOneUndoAdded();

    overview->get_field(added_node, OverviewBE::Label, name);
    EXPECT_EQ(name, "new name") << "rename " + what;

    checkUndo();
    overview->refresh_node(added_node, false);
    overview->get_field(added_node, OverviewBE::Label, name);
    EXPECT_EQ(name, old_name) << "undo rename " + what;

    checkRedo();
    overview->refresh_node(added_node, false);
    overview->get_field(added_node, OverviewBE::Label, name);
    EXPECT_EQ(name, "new name") << "redo rename " + what;

    // Delete
    overview->request_delete_object(added_node);
    checkOnlyOneUndoAdded();

    // check delete
    overview->refresh_node(base_node, true);
    EXPECT_EQ(overview->count_children(base_node), initial_count + 1) << what + " node count";
    {
      grt::BaseListRef list = grt::BaseListRef::cast_from(get_value_by_path(tester->getPmodel(), list_path));
      EXPECT_EQ(list.count(), initial_count) << list_path + " " + what + " count after delete";
    }

    // check undo delete
    checkUndo();
    overview->refresh_node(base_node, true);
    EXPECT_EQ(overview->count_children(base_node), initial_count + 2) << what + " node count";
    {
      grt::BaseListRef list = grt::BaseListRef::cast_from(get_value_by_path(tester->getPmodel(), list_path));
      EXPECT_EQ(list.count(), initial_count + 1) << list_path + " " + what + " count after undo";
    }

    // check redo delete
    checkRedo();
    overview->refresh_node(base_node, true);
    EXPECT_EQ(overview->count_children(base_node), initial_count + 1) << what + " node count";
    {
      grt::BaseListRef list = grt::BaseListRef::cast_from(get_value_by_path(tester->getPmodel(), list_path));
      EXPECT_EQ(list.count(), initial_count) << list_path + " " + what + " count after redo";
    }

    checkUndo(); // final undo delete
    checkUndo(); // final undo rename
    checkUndo(); // final undo add
  }
};

class General_Undo_RedoTest : public ::testing::Test {
protected:
  std::unique_ptr<WbUndoOthersData> data;

  void SetUp() override {
    data = std::make_unique<WbUndoOthersData>();
    data->tester.reset(new SqlStudioTester());
    data->tester->initializeRuntime();
    data->um = grt::GRT::get()->get_undo_manager();

    bool flag = data->tester->wb->open_document(std::filesystem::absolute("data/studio/undo_test_model1.mwb").u8string());
    ASSERT_TRUE(flag) << "open_document";

    data->overview = wb::WBContextUI::get()->get_physical_overview();
    wb::WBContextUI::get()->set_active_form(data->overview);

    EXPECT_EQ(data->tester->getCatalog()->schemata().count(), 1U) << "schemas";

    db_SchemaRef schema(data->tester->getCatalog()->schemata()[0]);

    // make sure the loaded model contains expected number of things
    EXPECT_EQ(schema->tables().count(), 4U) << "tables";
    EXPECT_EQ(schema->views().count(), 1U) << "views";
    EXPECT_EQ(schema->routineGroups().count(), 1U) << "groups";

    EXPECT_EQ(data->tester->getPmodel()->diagrams().count(), 1U) << "diagrams";
    model_DiagramRef view(data->tester->getPmodel()->diagrams()[0]);

    EXPECT_EQ(view->figures().count(), 5U) << "figures";

    EXPECT_EQ(data->um->get_undo_stack().size(), 0U) << "undo stack is empty";
  }

  void TearDown() override {
    data.reset();
  }

};



TEST_F(General_Undo_RedoTest, Overview_manipulations_Diagram) {
  data->checkOverviewObject("Diagram", NodeId("0"), "/diagrams", 1);
  EXPECT_EQ(data->um->get_undo_stack().size(), 0U) << "undo stack size";
}

TEST_F(General_Undo_RedoTest, Overview_manipulations_Schema) {
  std::string s;

  EXPECT_EQ(data->overview->count_children(NodeId("1")), 1U) << "schema count";

  data->overview->request_add_object(NodeId("1"));
  data->checkOnlyOneUndoAdded();

  data->overview->refresh_node(NodeId("1"), true);
  EXPECT_EQ(data->overview->count_children(NodeId("1")), 2U) << "schema count";

  data->checkUndo();
  data->overview->refresh_node(NodeId("1"), true);
  EXPECT_EQ(data->overview->count_children(NodeId("1")), 1U) << "schema count";

  data->checkRedo();
  data->overview->refresh_node(NodeId("1"), true);
  EXPECT_EQ(data->overview->count_children(NodeId("1")), 2U) << "schema count";

  data->overview->activate_node(NodeId("1.1"));

  data->overview->refresh_node(NodeId("1"), true);
  data->overview->get_field(NodeId("1.1"), 0, s);
  EXPECT_EQ(s, "new_schema1") << "overview original name";

  /* cant rename schema directly atm
   bool flag= overview->set_field(NodeId("1.1"), 0, "sakila");
   EXPECT_TRUE(flag) << "rename";
   data->checkOnlyOneUndoAdded();

   overview->refresh_node(NodeId("1"), true);
   overview->get_field(NodeId("1.1"), 0, s);
   EXPECT_EQ(s, "sakila") << "overview rename";

   data->checkUndo();
   overview->refresh_node(NodeId("1"), true);
   overview->get_field(NodeId("1.1"), 0, s);
   EXPECT_EQ(s, "new_schema1") << "overview original name";

   data->checkRedo();
   overview->set_field(NodeId("1.1"), 0, "sakila");
   data->checkOnlyOneUndoAdded();
   */
  data->overview->request_delete_object(NodeId("1.1"));
  data->checkOnlyOneUndoAdded();

  data->overview->refresh_node(NodeId("1"), true);
  EXPECT_EQ(data->overview->count_children(NodeId("1")), 1U) << "schema count";

  data->checkUndo();
  data->overview->refresh_node(NodeId("1"), true);
  EXPECT_EQ(data->overview->count_children(NodeId("1")), 2U) << "schema count";

  data->checkRedo();
  data->overview->refresh_node(NodeId("1"), true);
  EXPECT_EQ(data->overview->count_children(NodeId("1")), 1U) << "schema count";

  data->checkUndo();
  data->checkUndo(); // final undo schema add

  EXPECT_EQ(data->um->get_undo_stack().size(), 0U) << "undo stack size";
}

TEST_F(General_Undo_RedoTest, Overview_manipulations_Table) {
  data->checkOverviewObject("Table", NodeId("1.0.0"), "/catalog/schemata/0/tables", 4);
}

TEST_F(General_Undo_RedoTest, Overview_manipulations_View) {
  data->checkOverviewObject("View", NodeId("1.0.1"), "/catalog/schemata/0/views", 1);
}

TEST_F(General_Undo_RedoTest, Overview_manipulations_Routine) {
  data->checkOverviewObject("Routine", NodeId("1.0.2"), "/catalog/schemata/0/routines", 0);
}

TEST_F(General_Undo_RedoTest, Overview_manipulations_Group) {
  data->checkOverviewObject("Group", NodeId("1.0.3"), "/catalog/schemata/0/routineGroups", 1);
}

TEST_F(General_Undo_RedoTest, Overview_manipulations_User) {
  data->checkOverviewObject("User", NodeId("2.0"), "/catalog/users");
}

TEST_F(General_Undo_RedoTest, Overview_manipulations_Role) {
  data->checkOverviewObject("Role", NodeId("2.1"), "/catalog/roles");
}

TEST_F(General_Undo_RedoTest, Overview_manipulations_Script) {
  data->checkOverviewObject("Script", NodeId("3"), "/scripts");
}

TEST_F(General_Undo_RedoTest, Overview_manipulations_Note) {
  data->checkOverviewObject("Note", NodeId("4"), "/notes");
}

TEST_F(General_Undo_RedoTest, Sidebar) {
  ASSERT_TRUE(data->tester->closeDocument());
  data->tester->wb->close_document_finish();

  // reinitialize
  bool flag = data->tester->wb->open_document(std::filesystem::absolute("data/studio/undo_test_model1.mwb").u8string());
  ASSERT_TRUE(flag) << "open_document";

  data->overview = wb::WBContextUI::get()->get_physical_overview();
  wb::WBContextUI::get()->set_active_form(data->overview);

  bec::NodeId node(0);
  node.append(1);
  data->overview->activate_node(node);
}

TEST_F(General_Undo_RedoTest, Property) {
  data->tester->openAllDiagrams();
  data->tester->syncView();
  data->resetUndoAccounting();
  ModelDiagramForm *view = data->tester->wb->get_model_context()->get_diagram_form_for_diagram_id(data->tester->getPview().id());
  ASSERT_NE(view, nullptr) << "viewform";

  model_FigureRef table(find_named_object_in_list(data->tester->getPview()->figures(), "table2"));

  data->tester->getPview()->selectObject(table);

  EXPECT_EQ(view->get_selection().count(), 1U) << "selection";

  std::vector<std::string> items;

  ValueInspectorBE *insp = wb::WBContextUI::get()->create_inspector_for_selection(view, items);
  EXPECT_NE(insp, nullptr) << "prop inspector created";
  EXPECT_EQ(items.size(), 1U) << "items";
  EXPECT_EQ(items[0], "table2: Table") << "item0";

  EXPECT_EQ(*table->name(), "table2") << "table name";

  std::string s;

  insp->get_field(NodeId(7), ValueInspectorBE::Name, s);
  EXPECT_EQ(s, "name") << "node for name";

  bool flag = insp->set_field(NodeId(7), ValueInspectorBE::Value, "hello");
  EXPECT_TRUE(flag) << "rename value";
  data->checkOnlyOneUndoAdded();

  EXPECT_EQ(*table->name(), "hello") << "table renamed";
  data->checkUndo();
  EXPECT_EQ(*table->name(), "table2") << "table renamed back";
  data->checkRedo();
  EXPECT_EQ(*table->name(), "hello") << "table renamed";

  data->checkUndo();

  delete insp;
}

TEST_F(General_Undo_RedoTest, Description) {
  // select table in overview
  data->overview->refresh_node(NodeId("1.0.0"), true);
  data->overview->begin_selection_marking();
  data->overview->select_node(NodeId("1.0.0.1"));
  data->overview->end_selection_marking();

  std::vector<std::string> items;
  grt::ListRef<GrtObject> new_object_list;
  std::string description, old_description;

  old_description = wb::WBContextUI::get()->get_description_for_selection(new_object_list, items);

  EXPECT_EQ(items.size(), 1U) << "selection count";

  wb::WBContextUI::get()->set_description_for_selection(new_object_list, "test description");
  data->checkOnlyOneUndoAdded();

  EXPECT_EQ(*data->tester->getCatalog()->schemata()[0]->tables()[0]->comment(), "test description") << "description";

  data->checkUndo();

  EXPECT_EQ(*data->tester->getCatalog()->schemata()[0]->tables()[0]->comment(), "") << "description";

  data->checkRedo();

  EXPECT_EQ(*data->tester->getCatalog()->schemata()[0]->tables()[0]->comment(), "test description") << "description";

  // undo change description
  data->checkUndo();
}

TEST_F(General_Undo_RedoTest, Configuration_general_settings) {
  grt::DictRef options = data->tester->wb->get_wb_options();
  const std::string key = "test.general.undo";
  EXPECT_FALSE(options.has_key(key));

  data->resetUndoAccounting();
  {
    grt::AutoUndo undo;
    options.gset(key, 17);
    undo.end("Change general setting");
  }
  data->checkOnlyOneUndoAdded();
  EXPECT_EQ(17, options.get_int(key));
  data->checkUndo();
  EXPECT_FALSE(options.has_key(key));
  data->checkRedo();
  EXPECT_EQ(17, options.get_int(key));
  data->checkUndo();
}

TEST_F(General_Undo_RedoTest, Configuration_model_settings) {
  grt::DictRef options = data->tester->getPmodel()->options();
  const std::string key = "test.model.undo";
  EXPECT_FALSE(options.has_key(key));

  data->resetUndoAccounting();
  {
    grt::AutoUndo undo;
    options.gset(key, "model value");
    undo.end("Change model setting");
  }
  data->checkOnlyOneUndoAdded();
  EXPECT_EQ("model value", options.get_string(key));
  data->checkUndo();
  EXPECT_FALSE(options.has_key(key));
  data->checkRedo();
  EXPECT_EQ("model value", options.get_string(key));
  data->checkUndo();
}

TEST_F(General_Undo_RedoTest, Configuration_diagram_settings) {
  grt::DictRef options = data->tester->getPview()->options();
  const std::string key = "test.diagram.undo";
  EXPECT_FALSE(options.has_key(key));

  data->resetUndoAccounting();
  {
    grt::AutoUndo undo;
    options.gset(key, 1);
    undo.end("Change diagram setting");
  }
  data->checkOnlyOneUndoAdded();
  EXPECT_EQ(1, options.get_int(key));
  data->checkUndo();
  EXPECT_FALSE(options.has_key(key));
  data->checkRedo();
  EXPECT_EQ(1, options.get_int(key));
  data->checkUndo();
}

TEST_F(General_Undo_RedoTest, Configuration_page_settings) {
  app_PageSettingsRef settings = data->tester->wb->get_document()->pageSettings();
  ASSERT_TRUE(settings.is_valid());
  const std::string original = settings->orientation();
  const std::string changed = original == "landscape" ? "portrait" : "landscape";

  data->resetUndoAccounting();
  {
    grt::AutoUndo undo;
    settings->orientation(changed);
    undo.end("Change page setting");
  }
  data->checkOnlyOneUndoAdded();
  EXPECT_EQ(changed, *settings->orientation());
  data->checkUndo();
  EXPECT_EQ(original, *settings->orientation());
  data->checkRedo();
  EXPECT_EQ(changed, *settings->orientation());
  data->checkUndo();
}

TEST_F(General_Undo_RedoTest, Plugin_registration) {
  app_PluginRef plugin = data->tester->wb->get_plugin_manager()->get_plugin("wb.edit.toggleGrid");
  ASSERT_TRUE(plugin.is_valid());
  EXPECT_EQ("wb.edit.toggleGrid", *plugin->name());
  EXPECT_EQ("Toggle Grid", *plugin->caption());
  EXPECT_EQ("toggleGrid", *plugin->moduleFunctionName());
}


}
