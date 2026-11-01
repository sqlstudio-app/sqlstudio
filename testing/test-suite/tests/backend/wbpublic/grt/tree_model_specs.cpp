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

#include "grt/tree_model.h"

#include "gtest/gtest.h"
#include "wb_test_helpers.h"

using namespace grt;
using namespace bec;

namespace {

TEST(GrtTreeModelBase, BaseTests) {
  NodeId node, node2;

  EXPECT_FALSE(node.is_valid()) << "clean node";

  EXPECT_EQ(0U, node.depth()) << "clean node depth";

  node = NodeId(5);
  EXPECT_TRUE(node.is_valid()) << "node(5)";
  EXPECT_EQ(1U, node.depth()) << "node(5).depth()";
  EXPECT_EQ(5U, node[0]) << "node(5)[0]";

  node2 = node.append(7);
  EXPECT_EQ(2U, node.depth()) << "node append";
  EXPECT_EQ(5U, node[0]) << "node append[0]";
  EXPECT_EQ(7U, node[1]) << "node append[1]";

  EXPECT_EQ(2U, node2.depth()) << "node append ret";
  EXPECT_EQ(5U, node2[0]) << "node append ret[0]";
  EXPECT_EQ(7U, node2[1]) << "node append ret[1]";

  EXPECT_EQ(node2, node) << "node compare";

  node2 = NodeId(5);
  EXPECT_FALSE(node2 == node) << "node compare";

  node2.append(7);
  node2.append(11);

  EXPECT_FALSE(node2 == node) << "node compare";

  node = node2;
  EXPECT_EQ(node2, node) << "node assign/compare";
}

TEST(GrtTreeModelBase, Serialization) {
  // serialization
  NodeId node;
  std::string s;

  s = node.toString();
  EXPECT_EQ("", s) << "() toString";
  EXPECT_EQ(node, NodeId(s)) << "() parse";

  node.append(3);
  s = node.toString();
  EXPECT_EQ(NodeId(3), node) << "(3) check";
  EXPECT_EQ("3", s) << "(3) toString";
  EXPECT_EQ(node.toString(), NodeId(s).toString()) << "(3) parse";

  node.append(0);
  s = node.toString();
  EXPECT_EQ("3.0", s) << "(3,0) toString";
  EXPECT_EQ(node.toString(), NodeId(s).toString()) << "(3,0) parse";

  node.append(1);
  s = node.toString();
  EXPECT_EQ("3.0.1", s) << "(3,0,1) toString";
  EXPECT_EQ(node.toString(), NodeId(s).toString()) << "(3,0,1) parse";
}

TEST(GrtTreeModelBase, CommonTreeModelMethods) {
  class TestTreeModel : public TreeModel {
  public:
    size_t count_children(const NodeId &parent) override {
      if (!parent.is_valid())
        return 2;
      return parent == NodeId(0) ? 2 : 0;
    }
    void refresh() override {}
  } model;

  EXPECT_FALSE(model.get_root().is_valid());
  EXPECT_EQ(model.count(), 2U);
  EXPECT_EQ(model.get_node(0), NodeId(0));
  EXPECT_EQ(model.get_node(1), NodeId(1));
  const auto child = model.get_child(NodeId(0), 0);
  EXPECT_EQ(child, NodeId("0.0"));
  EXPECT_EQ(model.get_parent(child), NodeId(0));
  EXPECT_EQ(model.get_node_depth(child), 2U);
  EXPECT_TRUE(model.has_next(NodeId(0)));
  EXPECT_FALSE(model.has_next(NodeId(1)));
  EXPECT_EQ(model.get_next(NodeId(0)), NodeId(1));
  EXPECT_THROW(model.get_next(NodeId(1)), std::out_of_range);
  EXPECT_TRUE(model.has_next(child));
  EXPECT_EQ(model.get_next(child), NodeId("0.1"));
  EXPECT_FALSE(model.has_next(NodeId("0.1")));
  EXPECT_THROW(model.get_next(NodeId("0.1")), std::out_of_range);
  EXPECT_TRUE(model.is_expandable(NodeId(0)));
  EXPECT_FALSE(model.is_expandable(child));
  EXPECT_FALSE(model.expand_node(NodeId(0)));
  model.collapse_node(NodeId(0));
  EXPECT_FALSE(model.is_expanded(NodeId(0)));

  const auto uid = model.nodeid_to_uid(child);
  EXPECT_EQ(uid, model.nodeid_path_to_uid("0.0"));
  EXPECT_EQ(model.nodeuid_to_path(uid), "0.0");
  EXPECT_EQ(model.nodeuid_to_path(nullptr), "");
  int notifications = 0;
  auto connection = model.tree_changed_signal()->connect([&](NodeId parent, int previousCount) {
    EXPECT_EQ(parent, NodeId(0));
    EXPECT_EQ(previousCount, 2);
    ++notifications;
  });
  model.tree_changed(2, NodeId(0));
  EXPECT_EQ(notifications, 1);
  const auto refreshedUid = model.nodeid_to_uid(child);
  EXPECT_EQ(model.nodeuid_to_path(refreshedUid), "0.0");
  connection.disconnect();
}

}
