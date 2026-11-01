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
#include "wb_test_helpers.h"

#include "grt.h"
#include "grtsqlparser/mysql_parser_services.h"

using namespace parsers;

namespace {
struct MysqlParserModuleData {
  std::unique_ptr<SqlStudioTester> tester;
  MySQLParserServices::Ref services;
  MySQLParserContext::Ref context;
};

// Contains tests for the parser module implementing the ANTLR based parser services.
// Many of the APIs are also used in other tests.
class Parser_moduleTest : public ::testing::Test {
protected:
  static std::unique_ptr<MysqlParserModuleData> data;

  static void SetUpTestSuite() {
    data = std::make_unique<MysqlParserModuleData>();
    data->tester.reset(new SqlStudioTester(false));
    data->tester->initializeRuntime();

    data->services = MySQLParserServices::get();
    GrtVersionRef version(grt::Initialized);
    version->majorNumber(5);
    version->minorNumber(7);
    version->releaseNumber(10);
    data->context = MySQLParserServices::get()->createParserContext(data->tester->getRdbms()->characterSets(), version, "", true);
  }

  static void TearDownTestSuite() {
    data->context.reset();
    data.reset();
  }

  void checkStatement(const std::string &sql, MySQLQueryType type, const std::string &invalidSql) {
    SCOPED_TRACE(sql);
    EXPECT_EQ(data->services->determineQueryType(data->context, sql), type);
    EXPECT_EQ(data->services->checkSqlSyntax(data->context, sql.c_str(), sql.size(), MySQLParseUnit::PuGeneric), 0U);
    EXPECT_TRUE(data->context->errorsWithOffset(0).empty());

    EXPECT_GT(data->services->checkSqlSyntax(data->context, invalidSql.c_str(), invalidSql.size(),
                                           MySQLParseUnit::PuGeneric), 0U) << invalidSql;
    EXPECT_FALSE(data->context->errorsWithOffset(0).empty());

    EXPECT_EQ(data->services->checkSqlSyntax(data->context, sql.c_str(), sql.size(), MySQLParseUnit::PuGeneric), 0U);
    EXPECT_TRUE(data->context->errorsWithOffset(0).empty()) << "Previous syntax errors were not cleared";
  }

};

std::unique_ptr<MysqlParserModuleData> Parser_moduleTest::data;

TEST_F(Parser_moduleTest, alterStatement) {
  checkStatement("ALTER TABLE t ADD COLUMN c INT", QtAlterTable, "ALTER TABLE t ADD COLUMN");
  checkStatement("ALTER DATABASE db CHARACTER SET utf8", QtAlterDatabase, "ALTER DATABASE db CHARACTER SET");
}

TEST_F(Parser_moduleTest, createStatement) {
  checkStatement("CREATE TABLE t (id INT PRIMARY KEY, name VARCHAR(20))", QtCreateTable, "CREATE TABLE t (id)");
  checkStatement("CREATE VIEW v AS SELECT id FROM t", QtCreateView, "CREATE VIEW v AS SELECT");
}

TEST_F(Parser_moduleTest, dropStatement) {
  checkStatement("DROP TABLE IF EXISTS t", QtDropTable, "DROP TABLE");
  checkStatement("DROP VIEW IF EXISTS v", QtDropView, "DROP VIEW");
}

TEST_F(Parser_moduleTest, renameTableStatement) {
  checkStatement("RENAME TABLE t TO renamed, u TO other", QtRenameTable, "RENAME TABLE t TO");
}

TEST_F(Parser_moduleTest, truncateTableStatement) {
  checkStatement("TRUNCATE TABLE t", QtTruncateTable, "TRUNCATE TABLE");
}

TEST_F(Parser_moduleTest, callStatement) {
  checkStatement("CALL db.p(1, 'value')", QtCall, "CALL db.p(");
}

TEST_F(Parser_moduleTest, deleteStatement) {
  checkStatement("DELETE FROM t WHERE id = 1", QtDelete, "DELETE FROM t WHERE");
  checkStatement("DELETE t FROM t JOIN u ON t.id = u.id", QtDelete, "DELETE t FROM t JOIN");
}

TEST_F(Parser_moduleTest, doStatement) {
  checkStatement("DO 1 + 2, CONCAT('a', 'b')", QtDo, "DO");
}

TEST_F(Parser_moduleTest, handlerStatement) {
  checkStatement("HANDLER t OPEN AS h", QtHandler, "HANDLER t OPEN AS");
  checkStatement("HANDLER h READ FIRST LIMIT 1", QtHandler, "HANDLER h READ");
  checkStatement("HANDLER h CLOSE", QtHandler, "HANDLER");
}

TEST_F(Parser_moduleTest, insertStatement) {
  checkStatement("INSERT INTO t (id) VALUES (1), (2)", QtInsert, "INSERT INTO t (id) VALUES");
  checkStatement("INSERT INTO t SELECT id FROM u", QtInsert, "INSERT INTO t SELECT");
}

TEST_F(Parser_moduleTest, loadStatement) {
  checkStatement("LOAD DATA LOCAL INFILE 'rows.csv' INTO TABLE t FIELDS TERMINATED BY ','",
                 QtLoadData, "LOAD DATA INFILE 'rows.csv' INTO TABLE");
  checkStatement("LOAD XML INFILE 'rows.xml' INTO TABLE t ROWS IDENTIFIED BY '<row>'",
                 QtLoadXML, "LOAD XML INFILE 'rows.xml' INTO TABLE");
}

TEST_F(Parser_moduleTest, replaceStatement) {
  checkStatement("REPLACE INTO t (id) VALUES (1)", QtReplace, "REPLACE INTO t VALUES");
}

TEST_F(Parser_moduleTest, selectStatement) {
  checkStatement("SELECT t.id, COUNT(*) FROM t GROUP BY t.id HAVING COUNT(*) > 1 ORDER BY t.id",
                 QtSelect, "SELECT * FROM");
  checkStatement("SELECT 1 UNION ALL SELECT 2", QtSelect, "SELECT 1 UNION ALL");
}

TEST_F(Parser_moduleTest, updateStatement) {
  checkStatement("UPDATE t SET name = 'updated' WHERE id = 1", QtUpdate, "UPDATE t SET name =");
}

TEST_F(Parser_moduleTest, partitioning) {
  checkStatement("CREATE TABLE t (id INT) PARTITION BY RANGE (id) "
                 "(PARTITION p0 VALUES LESS THAN (10), PARTITION p1 VALUES LESS THAN MAXVALUE)",
                 QtCreateTable, "CREATE TABLE t (id INT) PARTITION BY RANGE");
  checkStatement("ALTER TABLE t ADD PARTITION (PARTITION p2 VALUES LESS THAN (100))",
                 QtAlterTable, "ALTER TABLE t ADD PARTITION (PARTITION)");
}

TEST_F(Parser_moduleTest, transactionOrLockingStatement) {
  checkStatement("START TRANSACTION WITH CONSISTENT SNAPSHOT", QtStartTransaction, "START TRANSACTION WITH");
  checkStatement("COMMIT AND NO CHAIN", QtCommit, "COMMIT AND");
  checkStatement("ROLLBACK TO SAVEPOINT s", QtRollbackSavepoint, "ROLLBACK TO SAVEPOINT (");
  checkStatement("SAVEPOINT s", QtSavepoint, "SAVEPOINT");
  checkStatement("RELEASE SAVEPOINT s", QtReleaseSavepoint, "RELEASE SAVEPOINT");
  checkStatement("LOCK TABLES t READ, u WRITE", QtLock, "LOCK TABLES t");
  checkStatement("UNLOCK TABLES", QtUnlock, "UNLOCK");
}

TEST_F(Parser_moduleTest, replicationStatement) {
  checkStatement("CHANGE MASTER TO MASTER_HOST = 'localhost', MASTER_PORT = 3306",
                 QtChangeMaster, "CHANGE MASTER TO MASTER_HOST =");
  checkStatement("START SLAVE", QtStartSlave, "START SLAVE UNTIL");
  checkStatement("STOP SLAVE", QtStopSlave, "STOP SLAVE FOR");
  checkStatement("PURGE BINARY LOGS TO 'mysql-bin.000010'", QtPurge, "PURGE BINARY LOGS TO");
}

TEST_F(Parser_moduleTest, preparedStatement) {
  checkStatement("PREPARE s FROM 'SELECT ?'", QtPrepare, "PREPARE s FROM");
  checkStatement("EXECUTE s USING @id", QtExecute, "EXECUTE s USING");
  checkStatement("DEALLOCATE PREPARE s", QtDeallocate, "DEALLOCATE PREPARE");
}

TEST_F(Parser_moduleTest, accountManagementStatement) {
  // ----- Grant statements.
  grt::DictRef result = data->services->parseStatement(data->context, "grant all privileges on table a to current_user");
  grt::StringListRef privileges = grt::StringListRef::cast_from(result["privileges"]);
  EXPECT_TRUE(privileges.is_valid()) << "95.1";
  EXPECT_EQ(privileges.get_index("all privileges"), 0U) << "95.2";
  EXPECT_EQ(*grt::StringRef::cast_from(result["target"]), "table a") << "95.3";

  grt::DictRef users = grt::DictRef::cast_from(result["users"]);
  EXPECT_TRUE(users.is_valid()) << "95.4";
  EXPECT_TRUE(users.has_key("current_user")) << "95.5";
  grt::DictRef user = grt::DictRef::cast_from(users["current_user"]);
  EXPECT_TRUE(user.has_key("user")) << "95.6";
  EXPECT_EQ(*grt::StringRef::cast_from(user["user"]), "current_user") << "95.7";

  result = data->services->parseStatement(data->context, "grant all privileges on table *.* to CURRENT_USER() identified by password 'blah'");
  privileges = grt::StringListRef::cast_from(result["privileges"]);
  EXPECT_TRUE(privileges.is_valid()) << "95.8";
  EXPECT_EQ(*grt::StringRef::cast_from(privileges[0]), "all privileges") << "95.9";
  EXPECT_EQ(*grt::StringRef::cast_from(result["target"]), "table *.*") << "95.10";

  users = grt::DictRef::cast_from(result["users"]);
  EXPECT_TRUE(users.is_valid()) << "95.11";
  user = grt::DictRef::cast_from(users["CURRENT_USER"]);
  EXPECT_EQ(*grt::StringRef::cast_from(user["user"]), "CURRENT_USER") << "95.12";
  EXPECT_EQ(*grt::StringRef::cast_from(user["id_method"]), "PASSWORD") << "95.13";
  EXPECT_EQ(*grt::StringRef::cast_from(user["id_string"]), "blah") << "95.14";

  result = data->services->parseStatement(data->context, "grant all privileges on x.* to mike identified with 'blah' by 'blubb'");
  privileges = grt::StringListRef::cast_from(result["privileges"]);
  EXPECT_TRUE(privileges.is_valid()) << "95.15";
  EXPECT_EQ(*grt::StringRef::cast_from(privileges[0]), "all privileges") << "95.1";
  EXPECT_EQ(*grt::StringRef::cast_from(result["target"]), "x.*") << "95.17";

  users = grt::DictRef::cast_from(result["users"]);
  EXPECT_TRUE(users.is_valid()) << "95.18";
  user = grt::DictRef::cast_from(users["mike"]);
  EXPECT_TRUE(user.is_valid()) << "95.1";
  EXPECT_EQ(*grt::StringRef::cast_from(user["user"]), "mike") << "95.20";
  EXPECT_EQ(*grt::StringRef::cast_from(user["id_method"]), "blah") << "95.21";
  EXPECT_EQ(*grt::StringRef::cast_from(user["id_string"]), "blubb") << "95.22";

  result = data->services->parseStatement(data->context, "grant all privileges on function x.y to mike\t@\nhome");
  privileges = grt::StringListRef::cast_from(result["privileges"]);
  EXPECT_TRUE(privileges.is_valid()) << "95.23";
  EXPECT_EQ(*grt::StringRef::cast_from(privileges[0]), "all privileges") << "95.24";
  EXPECT_EQ(*grt::StringRef::cast_from(result["target"]), "function x.y") << "95.25";

  users = grt::DictRef::cast_from(result["users"]);
  EXPECT_TRUE(users.is_valid()) << "95.26";
  user = grt::DictRef::cast_from(users["mike"]);
  EXPECT_TRUE(user.is_valid()) << "95.27";
  EXPECT_EQ(*grt::StringRef::cast_from(user["user"]), "mike") << "95.28";
  EXPECT_FALSE(user["id_method"].is_valid()) << "95.29";
  EXPECT_FALSE(user["id_string"].is_valid()) << "95.30";
  EXPECT_EQ(*grt::StringRef::cast_from(user["host"]), "home") << "95.31";

  result = data->services->parseStatement(data->context, "grant select on pizza to me require X509 with grant option");
  privileges = grt::StringListRef::cast_from(result["privileges"]);
  EXPECT_TRUE(privileges.is_valid()) << "95.32";
  EXPECT_EQ(*grt::StringRef::cast_from(privileges[0]), "select") << "95.33";
  EXPECT_EQ(*grt::StringRef::cast_from(result["target"]), "pizza") << "95.34";

  users = grt::DictRef::cast_from(result["users"]);
  EXPECT_TRUE(users.is_valid()) << "95.35";
  user = grt::DictRef::cast_from(users["me"]);
  EXPECT_TRUE(user.is_valid()) << "95.36";
  EXPECT_EQ(*grt::StringRef::cast_from(user["user"]), "me") << "95.37";
  EXPECT_EQ(user.count(), 1U) << "95.38";

  // Everything possible in a grant statements.
  std::string sql = base::wstring_to_string(
    L"grant insert (a), insert (b), insert(c), update(a), "
    L"alter routine, create routine, "
    L"create tablespace, create\t\t\t temporary      tables, create user, create view, delete, drop, event, "
    L"execute, file, grant option, index, insert, insert (a, b, c, d, ⌚️, ♨️), lock tables, process, proxy, "
    L"references (a, b, c, d, ⌚️, ♨️), reload, replication client, select, select (a, b, c, d, ⌚️, ♨️), "
    L"show databases, show view, shutdown, super, trigger, update, update (a, b, c, d, ⌚️, ♨️), usage "
    L"on *.* to current_user, CURRENT_USER() identified by password 'blah', mike identified with 'blah' by 'blubb', "
    L"mike@home require cipher 'abc' and cipher 'xyz' issuer 'a' subject 'b' and issuer '⌚️' with "
    L"grant option max_queries_per_hour 1 max_updates_per_hour 2 max_connections_per_hour 3 "
    L"max_user_connections 4 max_queries_per_hour 111 max_queries_per_hour 111 max_queries_per_hour 222");
  result = data->services->parseStatement(data->context, sql);
  privileges = grt::StringListRef::cast_from(result["privileges"]);
  EXPECT_TRUE(privileges.is_valid()) << "95.39";
  EXPECT_EQ(*grt::StringRef::cast_from(privileges[0]), "insert (a)") << "95.40";
  EXPECT_EQ(*grt::StringRef::cast_from(privileges[7]), "create\t\t\t temporary      tables") << "95.41";
  EXPECT_EQ(*grt::StringRef::cast_from(privileges[11]), "drop") << "95.42";
  EXPECT_EQ(*grt::StringRef::cast_from(privileges[15]), "grant option") << "95.43";
  EXPECT_EQ(*grt::StringRef::cast_from(privileges[18]), base::wstring_to_string(L"insert (a, b, c, d, ⌚️, ♨️)")) << "95.44";
  EXPECT_EQ(*grt::StringRef::cast_from(privileges[27]), "show databases") << "95.45";

  EXPECT_EQ(*grt::StringRef::cast_from(result["target"]), "*.*") << "95.46";

  users = grt::DictRef::cast_from(result["users"]);
  EXPECT_TRUE(users.is_valid()) << "95.48";
  EXPECT_EQ(users.count(), 3U) << "95.49";
  EXPECT_TRUE(users["current_user"].is_valid()) << "95.40";
  EXPECT_TRUE(users["CURRENT_USER"].is_valid()) << "95.50";
  EXPECT_TRUE(users["mike"].is_valid()) << "95.51";

  user = grt::DictRef::cast_from(users["CURRENT_USER"]);
  EXPECT_EQ(*grt::StringRef::cast_from(user["user"]), "CURRENT_USER") << "95.52";
  EXPECT_EQ(user.count(), 3U) << "95.53";

  grt::DictRef options = grt::DictRef::cast_from(result["options"]);
  EXPECT_TRUE(options.is_valid()) << "95.54";
  EXPECT_EQ(options.count(), 5U) << "95.55";
  EXPECT_EQ(*grt::StringRef::cast_from(options["grant"]), "") << "95.56";
  EXPECT_EQ(*grt::StringRef::cast_from(options["max_queries_per_hour"]), "222") << "95.57";

  grt::DictRef requirements = grt::DictRef::cast_from(result["requirements"]);
  EXPECT_TRUE(requirements.is_valid()) << "95.58";
  EXPECT_EQ(requirements.count(), 3U) << "95.59";
  EXPECT_EQ(*grt::StringRef::cast_from(requirements["cipher"]), "xyz") << "95.60";
  EXPECT_EQ(*grt::StringRef::cast_from(requirements["issuer"]), base::wstring_to_string(L"⌚️")) << "95.61";
}

TEST_F(Parser_moduleTest, table_administrationStatement) {
  checkStatement("ANALYZE TABLE t", QtAnalyzeTable, "ANALYZE TABLE");
  checkStatement("CHECK TABLE t QUICK", QtCheckTable, "CHECK TABLE");
  checkStatement("CHECKSUM TABLE t", QtChecksumTable, "CHECKSUM TABLE");
  checkStatement("OPTIMIZE TABLE t", QtOptimizeTable, "OPTIMIZE TABLE");
  checkStatement("REPAIR TABLE t", QtRepairTable, "REPAIR TABLE");
}

TEST_F(Parser_moduleTest, install_uninstall_statment) {
  checkStatement("INSTALL PLUGIN p SONAME 'plugin.dll'", QtInstallPlugin, "INSTALL PLUGIN p SONAME");
  checkStatement("UNINSTALL PLUGIN p", QtUninstallPlugin, "UNINSTALL PLUGIN");
}

TEST_F(Parser_moduleTest, setStatement) {
  checkStatement("SET @x = 1, @y = 2", QtSet, "SET @x =");
  checkStatement("SET autocommit = 0", QtSetAutoCommit, "SET autocommit =");
  checkStatement("SET TRANSACTION ISOLATION LEVEL READ COMMITTED", QtSetTransaction,
                 "SET TRANSACTION ISOLATION LEVEL");
}

TEST_F(Parser_moduleTest, showStatement) {
  checkStatement("SHOW TABLES FROM db LIKE 't%'", QtShowTables, "SHOW TABLES FROM");
  checkStatement("SHOW CREATE TABLE t", QtShowCreateTable, "SHOW CREATE TABLE");
  checkStatement("SHOW VARIABLES LIKE 'sql_mode'", QtShowVariables, "SHOW VARIABLES LIKE");
}

TEST_F(Parser_moduleTest, other_administrativeStatement) {
  checkStatement("FLUSH TABLES", QtFlush, "FLUSH TABLES WITH");
  checkStatement("KILL QUERY 123", QtKill, "KILL QUERY (");
  checkStatement("CACHE INDEX t IN default", QtCacheIndex, "CACHE INDEX t IN");
  checkStatement("LOAD INDEX INTO CACHE t", QtLoadIndex, "LOAD INDEX INTO CACHE");
}

TEST_F(Parser_moduleTest, utilityStatement) {
  checkStatement("EXPLAIN SELECT * FROM t WHERE id = 1", QtExplainStatement, "EXPLAIN SELECT * FROM");
  checkStatement("EXPLAIN t", QtExplainTable, "EXPLAIN (");
  checkStatement("DESCRIBE t", QtExplainTable, "DESCRIBE");
  checkStatement("HELP 'SELECT'", QtHelp, "HELP");
  checkStatement("USE db", QtUse, "USE");
}


}
