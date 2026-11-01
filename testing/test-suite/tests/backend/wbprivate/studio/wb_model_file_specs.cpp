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

#include "studio/wb_model_file.h"

#include "wb_test_helpers.h"
#include "context.h"

#include "base/file_utilities.h"
#include "base/utf8string.h"
#include "stub/stub_utilities.h"

#include "gtest/gtest.h"

using namespace wb;

namespace {

struct WbModelFileData {
  std::unique_ptr<SqlStudioTester> tester;
  std::string tmpDataDir;
  std::string outputDir;

  const base::utf8string BaseModelFile = "/studio/test_model1.mwb";
  const base::utf8string UnicodeDirectory = "/studio/pqŃńдфصض◒◓";
  const base::utf8string UnicodeBaseModelFile = "/studio/pqŃńдфصض◒◓/☀☁☂☘_model.mwb";

 void testModelSavingAndLoading(const base::utf8string &modelFile) {
    wb::ModelFile mf(outputDir);

    base::utf8string tempPath = base::strip_extension(modelFile) + "_tmp" + base::extension(modelFile);
    EXPECT_FALSE(base::file_exists(tempPath)) << "Model file left-over found";

    // Open, save copy, reopen from copy.
    EXPECT_NO_THROW([&]() { mf.open(modelFile); });
    EXPECT_NO_THROW([&]() { mf.save_to(tempPath); mf.cleanup(); });
    EXPECT_NO_THROW([&]() { mf.open(tempPath); mf.cleanup(); });
  }
};

} // anonymous namespace

class Tests_for_WB_model_fileTest : public ::testing::Test {
protected:
  static std::unique_ptr<WbModelFileData> data;

  static void SetUpTestSuite() {
    data = std::make_unique<WbModelFileData>();
    data->tmpDataDir = testing::Context::get().tmpDataDir();
    data->outputDir = testing::Context::get().outputDir();
    data->tester.reset(new SqlStudioTester());
  }

  static void TearDownTestSuite() {
    data.reset();
  }

};

std::unique_ptr<WbModelFileData> Tests_for_WB_model_fileTest::data;

TEST_F(Tests_for_WB_model_fileTest, Model_file_creation_Plus_rename) {
  ModelFile mf(data->outputDir);
  studio_DocumentRef doc(grt::Initialized);

  // Create a test file, change it and then save_as.
  mf.create();
  doc->name("t1");

  studio_physical_ModelRef pmodel(grt::Initialized);
  pmodel->owner(doc);
  db_Catalog catalog;
  pmodel->catalog(&catalog);
  doc->physicalModels().insert(pmodel);

  mf.store_document(doc);
  mf.save_to(data->outputDir + "/t1.mwb");

  doc->name("t2");
  mf.store_document(doc);
  mf.save_to(data->outputDir + "/t2.mwb");

  ModelFile mf1(data->outputDir);
  ModelFile mf2(data->outputDir);

  mf1.open(data->outputDir + "/t1.mwb");
  mf2.open(data->outputDir + "/t2.mwb");

  studio_DocumentRef d1, d2;

  d1 = mf1.retrieve_document();
  d2 = mf2.retrieve_document();

  EXPECT_EQ(*d1->name(), "t1");
  EXPECT_EQ(*d2->name(), "t2");
}

TEST_F(Tests_for_WB_model_fileTest, Open_file_locking_test) {
  const auto path = data->outputDir + "/locking-test.mwb";
  base::copyFile(data->tmpDataDir + data->BaseModelFile, path);
  ModelFile first(data->outputDir);
  ModelFile second(data->outputDir);
  first.open(path);
  const auto originalContents = first.get_file_contents(MAIN_DOCUMENT_NAME);
  ASSERT_FALSE(originalContents.empty());
  int warnings = 0;
  mforms::stub::UtilitiesWrapper::set_message_callback([&] {
    ++warnings;
    return mforms::ResultOk;
  });
  EXPECT_NO_THROW(second.open(path));
  mforms::stub::UtilitiesWrapper::set_message_callback({});
  EXPECT_EQ(warnings, 1);
  EXPECT_TRUE(second.get_tempdir_path().empty());
  EXPECT_EQ(first.get_file_contents(MAIN_DOCUMENT_NAME), originalContents);

  first.cleanup();
  EXPECT_NO_THROW(second.open(path));
  EXPECT_FALSE(second.get_tempdir_path().empty());
  EXPECT_EQ(second.get_file_contents(MAIN_DOCUMENT_NAME), originalContents);
  second.cleanup();
  EXPECT_TRUE(base::remove(path));
}

TEST_F(Tests_for_WB_model_fileTest, Reading_comment_test) {
  ModelFile mf(data->outputDir);
  std::string comment = mf.read_comment(data->tmpDataDir + "/studio/empty_file.sql");
  EXPECT_TRUE(comment.empty());
  comment = mf.read_comment(data->tmpDataDir + "/studio/empty_model_with_comment.mwb");
  EXPECT_TRUE(comment == "mydb");
}

TEST_F(Tests_for_WB_model_fileTest, Test_if_opened_model_can_be_saved) {
  // read the file - the file should be properly closed after reading
  ModelFile mf(data->outputDir);
  EXPECT_NO_THROW([&]() { mf.open(data->tmpDataDir + data->BaseModelFile); });

  // Try to write to the file - if the file wasn't closed this will fail.
  EXPECT_NO_THROW([&]() { mf.save_to(data->tmpDataDir + data->BaseModelFile); });
}

TEST_F(Tests_for_WB_model_fileTest, Test_model_loading_and_saving_with_ANSI_Plus_full_Unicode_paths_names) {
  data->testModelSavingAndLoading(data->tmpDataDir + data->BaseModelFile);

  // We have to prepare the directory
  {
    base::create_directory(data->tmpDataDir + data->UnicodeDirectory, 0777);

    base::copyFile(data->tmpDataDir + data->BaseModelFile,
             data->tmpDataDir + data->UnicodeBaseModelFile);
  }
  data->testModelSavingAndLoading(data->tmpDataDir + data->UnicodeBaseModelFile);
}
