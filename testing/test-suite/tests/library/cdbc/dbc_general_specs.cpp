/*
 * Copyright (c) 2012, 2020, Oracle and/or its affiliates.
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

#include "connector_test_fixture.h"
#include <cppconn/exception.h>

#include <string>

namespace {
using namespace testing;
using namespace sql::test;

class DbcGeneralTest : public MockConnectorFixture {};
class DbcGeneralIntegrationTest : public MockConnectorFixture {};

TEST_F(DbcGeneralIntegrationTest, ChecksInitialFunctionality) {
  EXPECT_EQ(static_cast<size_t>(INT_METACLASS_COUNT), grt::GRT::get()->get_metaclasses().size());
  auto connection = expectConnection();
  auto wrapper = manager->getConnection(connectionProperties);
  auto statement = newMock<MockStatement>();
  EXPECT_CALL(*connection, createStatement()).WillOnce(transferOwnership(statement));

  auto before = newMock<MockResultSet>();
  auto after = newMock<MockResultSet>();
  const sql::SQLString lookup("SHOW DATABASES LIKE 'sqlstudio_dbc_general'");
  {
    InSequence sequence;
    EXPECT_CALL(*statement, executeQuery(lookup)).WillOnce(transferOwnership(before));
    EXPECT_CALL(*before, rowsCount()).WillOnce(Return(0));
    EXPECT_CALL(*statement, execute(sql::SQLString("CREATE SCHEMA sqlstudio_dbc_general")))
      .WillOnce(Return(false));
    EXPECT_CALL(*statement, executeQuery(lookup)).WillOnce(transferOwnership(after));
    EXPECT_CALL(*after, rowsCount()).WillOnce(Return(1));
  }

  std::unique_ptr<sql::Statement> owned(wrapper->createStatement());
  {
    std::unique_ptr<sql::ResultSet> result(owned->executeQuery(lookup));
    EXPECT_EQ(0U, result->rowsCount());
  }
  owned->execute("CREATE SCHEMA sqlstudio_dbc_general");
  {
    std::unique_ptr<sql::ResultSet> result(owned->executeQuery(lookup));
    EXPECT_EQ(1U, result->rowsCount());
  }
}

TEST_F(DbcGeneralIntegrationTest, MetadataFetchTest) {
  auto connection = expectConnection();
  auto wrapper = manager->getConnection(connectionProperties);
  StrictMock<MockDatabaseMetaData> metadata;
  EXPECT_CALL(metadata, destroyed());
  auto schemata = newMock<MockResultSet>();
  auto objects = newMock<MockResultSet>();
  {
    InSequence sequence;
    EXPECT_CALL(*connection, getMetaData()).WillOnce(Return(&metadata));
    EXPECT_CALL(metadata, getSchemata(sql::SQLString(""))).WillOnce(transferOwnership(schemata));
    EXPECT_CALL(*schemata, next()).WillOnce(Return(true));
    EXPECT_CALL(*schemata, getString(sql::SQLString("Database")))
      .WillOnce(Return(sql::SQLString("sqlstudio_dbc_general")));
    EXPECT_CALL(metadata, getSchemaObjects(sql::SQLString(""), sql::SQLString("sqlstudio_dbc_general"),
                                           sql::SQLString(""), true, sql::SQLString(""), sql::SQLString("")))
      .WillOnce(transferOwnership(objects));
    EXPECT_CALL(*objects, next()).WillOnce(Return(true));
    EXPECT_CALL(*objects, getString(sql::SQLString("OBJECT_TYPE"))).WillOnce(Return(sql::SQLString("table")));
    EXPECT_CALL(*objects, getString(sql::SQLString("NAME"))).WillOnce(Return(sql::SQLString("product")));
    EXPECT_CALL(*objects, getString(sql::SQLString("DDL")))
      .WillOnce(Return(sql::SQLString("CREATE TABLE product (id INT)")));
    EXPECT_CALL(*objects, next()).WillOnce(Return(false));
    EXPECT_CALL(*schemata, next()).WillOnce(Return(false));
  }

  auto meta = wrapper->getMetaData();
  std::unique_ptr<sql::ResultSet> schemaRows(meta->getSchemata());
  ASSERT_TRUE(schemaRows->next());
  const auto schema = schemaRows->getString("Database");
  EXPECT_EQ(sql::SQLString("sqlstudio_dbc_general"), schema);
  std::unique_ptr<sql::ResultSet> objectRows(meta->getSchemaObjects("", schema));
  ASSERT_TRUE(objectRows->next());
  EXPECT_EQ(sql::SQLString("table"), objectRows->getString("OBJECT_TYPE"));
  EXPECT_EQ(sql::SQLString("product"), objectRows->getString("NAME"));
  EXPECT_EQ(sql::SQLString("CREATE TABLE product (id INT)"), objectRows->getString("DDL"));
  EXPECT_FALSE(objectRows->next());
  EXPECT_FALSE(schemaRows->next());
}

TEST_F(DbcGeneralIntegrationTest, TransactionTests) {
  auto connection = expectConnection();
  auto wrapper = manager->getConnection(connectionProperties);
  auto statement = newMock<MockStatement>();
  EXPECT_CALL(*connection, createStatement()).WillOnce(transferOwnership(statement));
  auto prepared = newMock<MockPreparedStatement>();
  auto before = newMock<MockResultSet>();
  auto after = newMock<MockResultSet>();
  const sql::SQLString insert("INSERT INTO product(idproduct, name) VALUES(?, ?)");
  const sql::SQLString query("SELECT idproduct, name FROM product");
  {
    InSequence sequence;
    EXPECT_CALL(*connection, setAutoCommit(false));
    EXPECT_CALL(*connection, prepareStatement(insert)).WillOnce(transferOwnership(prepared));
    EXPECT_CALL(*prepared, setInt(1, 1));
    EXPECT_CALL(*prepared, setString(2, sql::SQLString("Harry Potter")));
    EXPECT_CALL(*prepared, executeUpdate()).WillOnce(Return(1));
    EXPECT_CALL(*statement, executeQuery(query)).WillOnce(transferOwnership(before));
    EXPECT_CALL(*before, next()).WillOnce(Return(true));
    EXPECT_CALL(*before, getString(uint32_t(2))).WillOnce(Return(sql::SQLString("Harry Potter")));
    EXPECT_CALL(*before, next()).WillOnce(Return(false));
    EXPECT_CALL(*connection, rollback());
    EXPECT_CALL(*statement, executeQuery(query)).WillOnce(transferOwnership(after));
    EXPECT_CALL(*after, next()).WillOnce(Return(false));
  }

  std::unique_ptr<sql::Statement> owned(wrapper->createStatement());
  wrapper->setAutoCommit(false);
  std::unique_ptr<sql::PreparedStatement> insertStatement(wrapper->prepareStatement(insert));
  insertStatement->setInt(1, 1);
  insertStatement->setString(2, "Harry Potter");
  EXPECT_EQ(1, insertStatement->executeUpdate());
  {
    std::unique_ptr<sql::ResultSet> result(owned->executeQuery(query));
    ASSERT_TRUE(result->next());
    EXPECT_EQ(sql::SQLString("Harry Potter"), result->getString(2));
    EXPECT_FALSE(result->next());
  }
  wrapper->rollback();
  {
    std::unique_ptr<sql::ResultSet> result(owned->executeQuery(query));
    EXPECT_FALSE(result->next());
  }
}

TEST_F(DbcGeneralTest, RollbackFailurePropagates) {
  auto connection = expectConnection();
  auto wrapper = manager->getConnection(connectionProperties);
  EXPECT_CALL(*connection, rollback())
    .WillOnce(Throw(sql::SQLException("Mock rollback failed", "HY000", 2013)));
  try {
    wrapper->rollback();
    FAIL() << "Expected rollback failure";
  } catch (const sql::SQLException &error) {
    EXPECT_EQ(2013, error.getErrorCode());
    EXPECT_EQ(sql::SQLString("HY000"), error.getSQLState());
    EXPECT_STREQ("Mock rollback failed", error.what());
  }
}

} // namespace
