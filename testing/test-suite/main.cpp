/*
 * Copyright (c) 2025, dev4fun. All rights reserved.
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

//#include "pch.h"  
#include <gtest/gtest.h> 
#include <cstdlib>
#include "base/log.h"

extern void register_all_metaclasses();


int main(int argc, char** argv) {
  if (!std::getenv("SQLSTUDIO_DBC_LIVE_TESTS") && !std::getenv("GTEST_FILTER"))
    ::testing::GTEST_FLAG(filter) = "*-DbcMetadataIntegrationTest.*";
  testing::InitGoogleTest(&argc, argv);
  ::testing::GTEST_FLAG(death_test_style) = "fast";
  return RUN_ALL_TESTS();
}