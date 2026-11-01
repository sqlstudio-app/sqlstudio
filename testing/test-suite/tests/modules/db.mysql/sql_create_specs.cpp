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

#include "gtest/gtest.h"
#include "context.h"
#include "wb_connection_helpers.h"
#include "base/file_utilities.h"

#include <cstdlib>

#include "grt/grt_manager.h"
#include "grt.h"
#include "grtdb/diff_dbobjectmatch.h"
#include "interfaces/sqlgenerator.h"
#include "backend/db_mysql_sql_export.h"

#include "db_mysql_diffsqlgen.h"

#include "grtsqlparser/mysql_parser_services.h"

#include "model_mockup.h"
#include "wb_model_file.h"

using namespace grt;

namespace {
struct SqlCreateData {
  std::unique_ptr<SqlStudioTester> tester;
  SQLGeneratorInterfaceImpl *diffsqlModule = nullptr;
  sql::ConnectionWrapper connection;
  parsers::MySQLParserServices::Ref services = nullptr;
  parsers::MySQLParserContext::Ref parserContext;
  std::unique_ptr<wb::ModelFile> modelFile;

  std::string dataDir;

  db_mysql_CatalogRef loadCatalog(const std::string &filename) {
    modelFile = std::make_unique<wb::ModelFile>(testing::Context::get().outputDir());
    modelFile->open(filename);
    auto document = modelFile->retrieve_document();
    return db_mysql_CatalogRef::cast_from(document->physicalModels().get(0)->catalog());
  }

  void checkExportScript(const std::string &script, bool executeOnServer = true) {
    ASSERT_FALSE(script.empty());
    std::vector<parsers::StatementRange> ranges;
    services->determineStatementRanges(script.c_str(), script.size(), ";", ranges);
    ASSERT_FALSE(ranges.empty());
    for (const auto &range : ranges) {
      std::string statement(script, range.start, range.length);
      SCOPED_TRACE(statement);
      EXPECT_EQ(services->checkSqlSyntax(parserContext, statement.c_str(), statement.size(),
                                        MySQLParseUnit::PuGeneric), 0U);
      for (const auto &error : parserContext->errorsWithOffset(0))
        ADD_FAILURE() << error.message;
    }
    if (executeOnServer && connection.get()) {
      std::unique_ptr<sql::Statement> stmt(connection->createStatement());
      tester->executeScript(stmt.get(), script);
    }
  }

  void compareExportScript(const std::string &script, const std::string &expectedFile) {
    ASSERT_TRUE(base::file_exists(expectedFile)) << "Expected SQL file not found: " << expectedFile;
    EXPECT_EQ(base::replaceString(base::getTextFileContent(expectedFile), "\r\n", "\n"),
              base::replaceString(script, "\r\n", "\n"));
    checkExportScript(script, false);
  }

  std::string strrange(const std::string &s, const std::string &start, const std::string &end) {
    try {
      std::string res = s.substr(s.find(start));
      if (end.empty())
        return res;
      return res.substr(0, res.find(end));
    } catch (std::exception &e) {
      throw std::runtime_error(s + " does not contain " + start + " or " + end + ":" + e.what());
    }
  }
};

class SQL_code_generationTest : public ::testing::Test {
protected:
  std::unique_ptr<SqlCreateData> data;

  void SetUp() override {
    data = std::make_unique<SqlCreateData>();
    data->dataDir = testing::Context::get().baseDir() + "/test-suite/data";
    data->tester.reset(new SqlStudioTester());
    data->tester->initializeRuntime();
    bec::GRTManager::get()->set_app_option("DefaultTargetMySQLVersion", grt::StringRef("8.0.0"));
    ASSERT_TRUE(data->tester->closeDocument());
    data->tester->wb->close_document_finish();

    // Load modules.
    data->diffsqlModule = dynamic_cast<SQLGeneratorInterfaceImpl *>(grt::GRT::get()->get_module("DbMySQL"));
    ASSERT_NE(data->diffsqlModule, nullptr);

    data->services = parsers::MySQLParserServices::get();
    data->parserContext = data->services->createParserContext(data->tester->getRdbms()->characterSets(),
                                                            bec::parse_version("8.0.0"), "", true);
    const char *liveTests = std::getenv("SQLSTUDIO_SQL_GENERATION_LIVE_TESTS");
    if (liveTests != nullptr) {
      ASSERT_TRUE(std::string(liveTests) == "0" || std::string(liveTests) == "1")
        << "SQLSTUDIO_SQL_GENERATION_LIVE_TESTS must be 0 or 1";
      if (std::string(liveTests) == "1") {
        data->connection = createConnectionForImport();
        std::unique_ptr<sql::Statement> stmt(data->connection->createStatement());
        stmt->execute("DROP SCHEMA IF EXISTS `A`;");
        stmt->execute("DROP SCHEMA IF EXISTS `B`;");
      }
    }
  }

  void TearDown() override {
    if (data && data->tester) {
      data->tester->closeDocument();
      data->tester->wb->close_document_finish();
    }
    data.reset();
  }

};

TEST_F(SQL_code_generationTest, Validate_mockup_data) {
  ValueRef e;
  NormalizedComparer cmp;
  DbObjectMatchAlterOmf omf;

  testing::SyntheticMySQLModel model;
  db_mysql_ViewRef view(grt::Initialized);
  model.schema->views().insert(view);
  view->owner(model.schema);
  view->name("v2");
  view->sqlDefinition("create view v2 as SELECT if(t1.id > 2, 'active, very active', 'inactive, very very very inactive'), "
    "if(t1.id > 4, 'active, very active', 'inactive, very very very inactive') FROM t1");

  db_mysql_CatalogRef catalog = model.catalog;
  cmp.init_omf(&omf);

  std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);
  std::shared_ptr<DiffChange> drop_change = diff_make(catalog, e, &omf);

  DictRef create_map(true);
  DictRef drop_map(true);
  grt::DictRef options(true);
  options.set("UseFilteredLists", grt::IntegerRef(0));
  options.set("OutputContainer", create_map);
  options.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
  options.set("GenerateSchemaDrops", grt::IntegerRef(1));
  options.set("GenerateDrops", grt::IntegerRef(1));
  data->diffsqlModule->generateSQL(catalog, options, create_change);

  options.set("OutputContainer", drop_map);
  data->diffsqlModule->generateSQL(catalog, options, drop_change);

  data->diffsqlModule->makeSQLExportScript(catalog, options, create_map, drop_map);
  std::string export_sql_script = options.get_string("OutputScript");
  EXPECT_NE(export_sql_script.find("DROP TABLE IF EXISTS `test_schema`.`t1`"), std::string::npos) << "DROP TABLE not found";

  data->checkExportScript(export_sql_script);
}

TEST_F(SQL_code_generationTest, Forward_engineering_model_w_o_qualifying_schema) {
  grt::ValueRef e;
  NormalizedComparer cmp;
  DbObjectMatchAlterOmf omf;

  testing::SyntheticMySQLModel model;
  db_mysql_CatalogRef catalog = model.catalog;
  cmp.init_omf(&omf);

  std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);
  std::shared_ptr<DiffChange> drop_change = diff_make(catalog, e, &omf);

  DictRef create_map(true);
  DictRef drop_map(true);
  DictRef options(true);
  options.set("UseFilteredLists", grt::IntegerRef(0));
  options.set("OutputContainer", create_map);
  options.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
  options.set("GenerateSchemaDrops", grt::IntegerRef(1));
  options.set("OmitSchemas", grt::IntegerRef(1));
  options.set("GenerateUse", grt::IntegerRef(1));
  options.set("GenerateDrops", grt::IntegerRef(1));
  data->diffsqlModule->generateSQL(catalog, options, create_change);

  options.set("OutputContainer", drop_map);
  data->diffsqlModule->generateSQL(catalog, options, drop_change);

  data->diffsqlModule->makeSQLExportScript(catalog, options, create_map, drop_map);
  std::string export_sql_script = options.get_string("OutputScript");
  EXPECT_NE(export_sql_script.find("USE `test_schema`"), std::string::npos);
  EXPECT_EQ(export_sql_script.find("CREATE TABLE IF NOT EXISTS `test_schema`."), std::string::npos);
  data->checkExportScript(export_sql_script);
}

TEST_F(SQL_code_generationTest, Bug_Nr11926862_NO_WAY_TO_SORT_SCHEMAS_ON_EXPORT) {
  ValueRef e;
  NormalizedComparer cmp;
  DbObjectMatchAlterOmf omf;

  db_mysql_CatalogRef catalog = data->loadCatalog(data->dataDir + "/studio/11926862.mwb");

  cmp.init_omf(&omf);

  std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);
  std::shared_ptr<DiffChange> drop_change = diff_make(catalog, e, &omf);

  DictRef create_map(true);
  DictRef drop_map(true);
  DictRef options(true);
  options.set("UseFilteredLists", grt::IntegerRef(0));
  options.set("OutputContainer", create_map);
  options.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
  options.set("GenerateSchemaDrops", grt::IntegerRef(1));
  options.set("OmitSchemas", grt::IntegerRef(1));
  options.set("GenerateUse", grt::IntegerRef(1));
  options.set("GenerateDrops", grt::IntegerRef(1));
  data->diffsqlModule->generateSQL(catalog, options, create_change);

  options.set("OutputContainer", drop_map);
  data->diffsqlModule->generateSQL(catalog, options, drop_change);

  data->diffsqlModule->makeSQLExportScript(catalog, options, create_map, drop_map);
  std::string export_sql_script = options.get_string("OutputScript");
  const auto schemaA = export_sql_script.find("CREATE SCHEMA IF NOT EXISTS `A`");
  const auto schemaB = export_sql_script.find("CREATE SCHEMA IF NOT EXISTS `B`");
  ASSERT_NE(schemaA, std::string::npos);
  ASSERT_NE(schemaB, std::string::npos);
  EXPECT_LT(schemaA, schemaB);
  data->checkExportScript(export_sql_script);
}

TEST_F(SQL_code_generationTest, Bug_Nr14278043_DB_SYNCRONIZE_MODEL_GENERATES_INCORRECT_COLLATION) {
  // If somehow the collation doesn't correspond to the charset it should be skipped during
  // sql generation to avoid creating invalid DDL.
  ValueRef e;
  NormalizedComparer cmp;
  DbObjectMatchAlterOmf omf;

  testing::SyntheticMySQLModel model;

  model.schema->defaultCharacterSetName("utf8");
  model.schema->defaultCollationName("latin_1_swedish_ci");
  model.table->defaultCharacterSetName("utf8");
  model.table->defaultCollationName("latin_1_swedish_ci");
  model.columnText->characterSetName("utf8");
  model.columnText->collationName("latin_1_swedish_ci");

  db_mysql_CatalogRef catalog = model.catalog;

  cmp.init_omf(&omf);

  std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);
  std::shared_ptr<DiffChange> drop_change = diff_make(catalog, e, &omf);

  DictRef create_map(true);
  DictRef drop_map(true);
  DictRef options(true);
  options.set("UseFilteredLists", grt::IntegerRef(0));
  options.set("OutputContainer", create_map);
  options.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
  options.set("GenerateSchemaDrops", grt::IntegerRef(1));
  options.set("GenerateDrops", grt::IntegerRef(1));
  data->diffsqlModule->generateSQL(catalog, options, create_change);

  options.set("OutputContainer", drop_map);
  data->diffsqlModule->generateSQL(catalog, options, drop_change);

  data->diffsqlModule->makeSQLExportScript(catalog, options, create_map, drop_map);
  std::string export_sql_script = options.get_string("OutputScript");
  EXPECT_EQ(export_sql_script.find("latin_1_swedish_ci"), std::string::npos);
  data->checkExportScript(export_sql_script);
}

TEST_F(SQL_code_generationTest, Generation_of_comments_with_support_for_truncation) {
  std::string comment_60 = "012345678901234567890123456789012345678901234567890123456789";
  std::string comment_255 = comment_60 + comment_60 + comment_60 + comment_60 + "012345678912345";

  ValueRef e;
  NormalizedComparer cmp;
  DbObjectMatchAlterOmf omf;

  testing::SyntheticMySQLModel model;
  db_mysql_CatalogRef catalog = model.catalog;
  db_mysql_TableRef table = catalog->schemata()[0]->tables()[0];
  cmp.init_omf(&omf);

  // before 5.5.3, column comments <= 255, table <= 60
  {
    DictRef dbsett(data->diffsqlModule->getTraitsForServerVersion(5, 5, 2));
    dbsett.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
    table->comment(comment_60);
    table->columns()[1]->comment(comment_255);
    std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);

    DictRef create_map(true);
    DictRef drop_map(true);
    DictRef options(true);
    options.set("UseFilteredLists", grt::IntegerRef(0));
    options.set("OutputContainer", create_map);
    options.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
    options.set("GenerateSchemaDrops", grt::IntegerRef(1));
    options.set("GenerateDrops", grt::IntegerRef(1));
    options.set("DBSettings", dbsett);
    data->diffsqlModule->generateSQL(catalog, options, create_change);

    std::string s = create_map.get_string("db.mysql.Table::`test_schema`.`t1`::t1");
    EXPECT_EQ(data->strrange(s, "`parent_id`", ","), "`parent_id` TINYINT NULL COMMENT '" + comment_255 + "'");
    EXPECT_EQ(data->strrange(s, "COMMENT =", ""), "COMMENT = '" + comment_60 + "'");
  }

  {
    grt::DictRef dbsett(data->diffsqlModule->getTraitsForServerVersion(5, 5, 2));
    dbsett.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
    table->comment(comment_60 + "###");
    table->columns()[1]->comment(comment_255 + "###");
    std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);

    DictRef create_map(true);
    DictRef drop_map(true);
    DictRef options(true);
    options.set("UseFilteredLists", grt::IntegerRef(0));
    options.set("OutputContainer", create_map);
    options.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
    options.set("GenerateSchemaDrops", grt::IntegerRef(1));
    options.set("GenerateDrops", grt::IntegerRef(1));
    options.set("DBSettings", dbsett);
    data->diffsqlModule->generateSQL(catalog, options, create_change);

    std::string s = create_map.get_string("db.mysql.Table::`test_schema`.`t1`::t1");

    EXPECT_EQ(data->strrange(s, "`parent_id`", ","),
      "`parent_id` TINYINT NULL COMMENT '" + comment_255 + "' /* comment truncated */ /*###*/");
    EXPECT_EQ(data->strrange(s, "COMMENT =", ""),
      "COMMENT = '" + comment_60 + "' /* comment truncated */ /*###*/");
  }

  // after 5.5.3
  std::string comment_2048(std::string(2048, 'X'));
  std::string comment_1024(std::string(1024, 'X'));

  DictRef dbsett(data->diffsqlModule->getTraitsForServerVersion(5, 5, 4));
  dbsett.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));

  {
    table->comment(comment_2048);
    table->columns()[1]->comment(comment_1024);
    std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);

    DictRef create_map(true);
    DictRef drop_map(true);
    DictRef options(true);
    options.set("UseFilteredLists", grt::IntegerRef(0));
    options.set("OutputContainer", create_map);
    options.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
    options.set("GenerateSchemaDrops", grt::IntegerRef(1));
    options.set("GenerateDrops", grt::IntegerRef(1));
    options.set("DBSettings", dbsett);
    data->diffsqlModule->generateSQL(catalog, options, create_change);

    std::string s = create_map.get_string("db.mysql.Table::`test_schema`.`t1`::t1", "ERROR");
    EXPECT_EQ(data->strrange(s, "`parent_id`", ","), "`parent_id` TINYINT NULL COMMENT '" + comment_1024 + "'");
    EXPECT_EQ(data->strrange(s, "COMMENT =", ""), "COMMENT = '" + comment_2048 + "'");
  }

  {
    table->comment(comment_2048 + "###");
    table->columns()[1]->comment(comment_1024 + "###");
    std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);

    DictRef create_map(true);
    DictRef drop_map(true);
    DictRef options(true);
    options.set("UseFilteredLists", grt::IntegerRef(0));
    options.set("OutputContainer", create_map);
    options.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
    options.set("GenerateSchemaDrops", grt::IntegerRef(1));
    options.set("GenerateDrops", grt::IntegerRef(1));
    options.set("DBSettings", dbsett);
    data->diffsqlModule->generateSQL(catalog, options, create_change);

    std::string s = create_map.get_string("db.mysql.Table::`test_schema`.`t1`::t1");
    EXPECT_EQ(data->strrange(s, "`parent_id`", ","),
      "`parent_id` TINYINT NULL COMMENT '" + comment_1024 + "' /* comment truncated */ /*###*/");
    EXPECT_EQ(data->strrange(s, "COMMENT =", ""),
      "COMMENT = '" + comment_2048 + "' /* comment truncated */ /*###*/");
  }
}

TEST_F(SQL_code_generationTest, bug_11765994_fwd_view_can_be_sometimes_problematic_with_case_sensitivity) {
  {
    ValueRef e;
    NormalizedComparer cmp;
    DbObjectMatchAlterOmf omf;

    std::string modelfile = data->dataDir + "/forward_engineer/view_mixed_case.mwb";
    db_mysql_CatalogRef catalog = data->loadCatalog(modelfile);

    cmp.init_omf(&omf);

    std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);
    std::shared_ptr<DiffChange> drop_change = diff_make(catalog, e, &omf);

    DictRef create_map(true);
    DictRef drop_map(true);
    DictRef db_opts(true);
    DictRef options(true);
    options.set("UseFilteredLists", grt::IntegerRef(0));
    options.set("OutputContainer", create_map);
    options.set("CaseSensitive", grt::IntegerRef(1));
    options.set("GenerateSchemaDrops", grt::IntegerRef(1));
    options.set("OmitSchemas", grt::IntegerRef(1));
    options.set("GenerateUse", grt::IntegerRef(1));
    options.set("GenerateDrops", grt::IntegerRef(1));
    options.set("GenerateDocumentProperties", grt::IntegerRef(0));
    data->diffsqlModule->generateSQL(catalog, options, create_change);

    // Case sensitive in db set to true
    db_opts.set("CaseSensitive", grt::IntegerRef(1));
    options.set("DBSettings", db_opts);

    options.set("OutputContainer", drop_map);
    data->diffsqlModule->generateSQL(catalog, options, drop_change);

    data->diffsqlModule->makeSQLExportScript(catalog, options, create_map, drop_map);
    std::string export_sql_script = options.get_string("OutputScript");

    std::string expected_sql = data->dataDir + "/forward_engineer/view_mixed_case.expected.broken.sql";

    data->compareExportScript(export_sql_script, expected_sql);

  }

  {
    ValueRef e;
    NormalizedComparer cmp;
    DbObjectMatchAlterOmf omf;

    std::string modelfile = data->dataDir + "/forward_engineer/view_mixed_case.mwb";
    db_mysql_CatalogRef catalog = data->loadCatalog(modelfile);

    cmp.init_omf(&omf);

    std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);
    std::shared_ptr<DiffChange> drop_change = diff_make(catalog, e, &omf);

    DictRef create_map(true);
    DictRef drop_map(true);
    DictRef db_opts(true);
    grt::DictRef options(true);
    options.set("UseFilteredLists", grt::IntegerRef(0));
    options.set("OutputContainer", create_map);
    options.set("CaseSensitive", grt::IntegerRef(1));
    options.set("GenerateSchemaDrops", grt::IntegerRef(1));
    options.set("OmitSchemas", grt::IntegerRef(1));
    options.set("GenerateUse", grt::IntegerRef(1));
    options.set("GenerateDrops", grt::IntegerRef(1));
    options.set("GenerateDocumentProperties", grt::IntegerRef(0));
    data->diffsqlModule->generateSQL(catalog, options, create_change);

    // Case sensitive in db set to false
    db_opts.set("CaseSensitive", grt::IntegerRef(0));
    options.set("DBSettings", db_opts);

    options.set("OutputContainer", drop_map);
    data->diffsqlModule->generateSQL(catalog, options, drop_change);

    data->diffsqlModule->makeSQLExportScript(catalog, options, create_map, drop_map);
    std::string export_sql_script = options.get_string("OutputScript");

    std::string expected_sql = data->dataDir + "/forward_engineer/view_mixed_case.expected.good.sql";

    data->compareExportScript(export_sql_script, expected_sql);

  }
}

TEST_F(SQL_code_generationTest, Forward_engineering_after_renaming_a_schema) {
  ValueRef e;
  NormalizedComparer cmp;
  DbObjectMatchAlterOmf omf;

  std::string modelfile = data->dataDir + "/forward_engineer/sakila_full.mwb";
  db_mysql_CatalogRef catalog = data->loadCatalog(modelfile);

  catalog->schemata()[0]->name("sakila_test");

  parsers::MySQLParserServices::Ref services = parsers::MySQLParserServices::get();
  GrtVersionRef version = bec::parse_version("5.6.0");
  parsers::MySQLParserContext::Ref context =
    services->createParserContext(data->tester->getRdbms()->characterSets(), version, "", false);
  services->renameSchemaReferences(context, catalog, "sakila", "sakila_test");

  cmp.init_omf(&omf);

  std::shared_ptr<DiffChange> create_change = diff_make(e, catalog, &omf);
  std::shared_ptr<DiffChange> drop_change = diff_make(catalog, e, &omf);

  DictRef create_map(true);
  DictRef drop_map(true);

  grt::DictRef options = DictRef::cast_from(grt::GRT::get()->unserialize(data->dataDir + "/forward_engineer/rename_opts.dict"));
  options.set("GenerateDocumentProperties", grt::IntegerRef(0));

  create_map = data->diffsqlModule->generateSQLForDifferences(GrtNamedObjectRef(), catalog, options);

  grt::DictRef dbsett(data->diffsqlModule->getTraitsForServerVersion(5, 6, 0));
  dbsett.set("CaseSensitive", grt::IntegerRef(omf.case_sensitive));
  options.set("DBSettings", dbsett);
  data->diffsqlModule->makeSQLExportScript(catalog, options, create_map, drop_map);

  std::string export_sql_script = options.get_string("OutputScriptHeader") + options.get_string("OutputScript");

  std::string expected_sql = data->dataDir + "/forward_engineer/sakila_name_changed.expected.sql";

  data->compareExportScript(export_sql_script, expected_sql);

}


}
