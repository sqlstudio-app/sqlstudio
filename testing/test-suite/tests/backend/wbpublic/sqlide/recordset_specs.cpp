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

#include "sqlide/recordset_cdbc_storage.h"
#include "sqlide/recordset_be.h"
#include "cppdbc.h"

#include "gtest/gtest.h"
#include "wb_test_helpers.h"
#include "library/cdbc/connector_mocks.h"
#include <sstream>

namespace {

using namespace testing;
using namespace sql::test;

class RecordsetTest : public ::testing::Test {
protected:
  std::unique_ptr<SqlStudioTester> tester;
  sql::Dbc_connection_handler::Ref connection;

  void SetUp() override {
    tester.reset(new SqlStudioTester());
    tester->initializeRuntime();

    connection = std::make_shared<sql::Dbc_connection_handler>();
    auto mockConnection = std::make_shared<StrictMock<MockConnection>>();
    EXPECT_CALL(*mockConnection, destroyed());
    connection->ref = sql::ConnectionWrapper(mockConnection, nullptr);
  }
};

TEST_F(RecordsetTest, RecordsetStorage) {
  Recordset_cdbc_storage::Ref data_storage(Recordset_cdbc_storage::create());

  base::RecMutex _connLock;
  data_storage->setUserConnectionGetter(
    [&](sql::Dbc_connection_handler::Ref &conn, bool LockOnly = false) -> base::RecMutexLock {
      base::RecMutexLock lock(_connLock, false);
      conn = connection;
      return lock;
    }
  );

  Recordset::Ref rs = Recordset::create();
  rs->data_storage(data_storage);

  NiceMock<MockResultSetMetaData> metadata;
  EXPECT_CALL(metadata, getColumnCount()).WillRepeatedly(Return(2));
  EXPECT_CALL(metadata, getColumnTypeName(_)).WillRepeatedly(Return(sql::SQLString("VARBINARY")));
  EXPECT_CALL(metadata, getColumnType(_)).WillRepeatedly(Return(sql::DataType::VARBINARY));
  EXPECT_CALL(metadata, isNumeric(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(metadata, getColumnLabel(1)).WillRepeatedly(Return(sql::SQLString("empty_blob")));
  EXPECT_CALL(metadata, getColumnLabel(2)).WillRepeatedly(Return(sql::SQLString("null_blob")));

  auto result = std::make_shared<StrictMock<MockResultSet>>();
  EXPECT_CALL(*result, destroyed());
  EXPECT_CALL(*result, getMetaData()).WillOnce(Return(&metadata));
  EXPECT_CALL(*result, next()).WillOnce(Return(true)).WillOnce(Return(false));
  EXPECT_CALL(*result, isNull(uint32_t(1))).WillOnce(Return(false));
  EXPECT_CALL(*result, isNull(uint32_t(2))).WillOnce(Return(true));
  EXPECT_CALL(*result, getBlob(uint32_t(1))).WillOnce([] { return new std::istringstream(""); });
  std::shared_ptr<sql::ResultSet> rset = result;
  data_storage->dbc_resultset(rset);

  rs->reset(true);

  EXPECT_FALSE(rs->is_field_null(0, 0)) << "empty blob string is not NULL";
  EXPECT_TRUE(rs->is_field_null(0, 1)) << "NULL blob is NULL";
}

}

