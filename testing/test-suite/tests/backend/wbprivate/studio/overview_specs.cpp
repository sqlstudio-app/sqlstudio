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

#include "grt.h"
#include "wb_test_helpers.h"
#include "grt_test_helpers.h"

#include "studio/wb_overview.h"
#include "model/wb_overview_physical.h"
#include "grts/structs.studio.h"
#include "grts/structs.studio.logical.h"
#include "grts/structs.studio.physical.h"

#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>

namespace {

using namespace grt;
using namespace wb;
using namespace bec;

static void ensure_files_equal(const std::string &test, const char *file, const char *reffile) {
  std::string line, refline;
  std::ifstream ref(reffile);
  std::ifstream f(file);

  ASSERT_TRUE(ref.is_open()) << reffile;
  ASSERT_TRUE(f.is_open()) << file;

  while (!ref.eof() && !f.eof()) {
    getline(ref, refline);
    getline(f, line);

    EXPECT_EQ(refline, line);
  }

  EXPECT_TRUE(f.eof() && ref.eof());
}

struct TestData {
  std::unique_ptr<SqlStudioTester> tester;
  std::string dataDir;
};

class WBOverviewTest : public ::testing::Test {
protected:
  TestData *data = new TestData();

  void SetUp() override {
    data->dataDir = std::filesystem::absolute("data").lexically_normal().u8string();
    data->tester.reset(new SqlStudioTester());
    data->tester->initializeRuntime();
  }

  void TearDown() override {
    delete data;
  }
};

TEST_F(WBOverviewTest, OpenDocument) {
  ASSERT_TRUE(data->tester->wb->open_document(data->dataDir + "/studio/test_model1.mwb"));
}

TEST_F(WBOverviewTest, DumpTreeModel) {
  ASSERT_TRUE(data->tester->wb->open_document(data->dataDir + "/studio/test_model1.mwb"));
  auto *overview = data->tester->wbui->get_physical_overview();
  ASSERT_NE(overview, nullptr);
  ASSERT_TRUE(overview->get_model().is_valid());

  const std::vector<ssize_t> columns = {
    wb::OverviewBE::Label, wb::OverviewBE::NodeType, wb::OverviewBE::Expanded,
    wb::OverviewBE::Height, wb::OverviewBE::DisplayMode
  };
  std::filesystem::create_directories("output");
  testing::dumpTreeModel("output/overview_test2.txt", overview, columns);

  ensure_files_equal("initial overview state ", "output/overview_test2.txt",
                     (data->dataDir + "/be/overview_test2.txt").c_str());
}
}
