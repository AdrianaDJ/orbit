// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/strings/str_format.h>
#include <gtest/gtest.h>

#include <filesystem>

#include "ElfUtils/LinuxMap.h"
#include "OrbitBase/ExecutablePath.h"

TEST(LinuxMap, ReadModules) {
  const auto result = orbit_elf_utils::ReadModules(getpid());
  EXPECT_TRUE(result) << result.error().message();
}

TEST(LinuxMap, ParseMaps) {
  using orbit_elf_utils::ParseMaps;
  using orbit_grpc_protos::ModuleInfo;

  {
    // Empty data
    const auto result = ParseMaps(std::string_view{""});
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_TRUE(result.value().empty());
  }

  const std::filesystem::path test_path = orbit_base::GetExecutableDir() / "testdata";
  const std::filesystem::path hello_world_path = test_path / "hello_world_elf";
  const std::filesystem::path text_file = test_path / "textfile.txt";

  {
    // Testing correct size of result. The entry with dev/zero is ignored due to the path starting
    // with /dev/. The last entry has a valid path, but the executable flag is not set.
    const std::string data{absl::StrFormat(
        "7f687428f000-7f6874290000 r-xp 00009000 fe:01 661216                     "
        "/not/a/valid/file/path\n"
        "7f6874290000-7f6874297000 r-xp 00000000 fe:01 661214                     %s\n"
        "7f6874290000-7f6874297000 r-xp 00000000 fe:01 661214                     /dev/zero\n"
        "7f6874290001-7f6874297002 r-dp 00000000 fe:01 661214                     %s\n",
        hello_world_path, text_file)};
    const auto result = ParseMaps(data);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(result.value().size(), 1);
  }

  const std::filesystem::path no_symbols_path = test_path / "no_symbols_elf";
  {
    // Example data
    const std::string data{absl::StrFormat(
        "7f6874285000-7f6874288000 r--p 00000000 fe:01 661216                     %s\n"
        "7f6874288000-7f687428c000 r-xp 00003000 fe:01 661216                     %s\n"
        "7f687428c000-7f687428e000 r--p 00007000 fe:01 661216                     %s\n"
        "7f687428e000-7f687428f000 r--p 00008000 fe:01 661216                     %s\n"
        "7f687428f000-7f6874290000 rw-p 00009000 fe:01 661216                     %s\n"
        "0-1000 r-xp 00009000 fe:01 661216                     %s\n",
        hello_world_path, hello_world_path, hello_world_path, hello_world_path, hello_world_path,
        no_symbols_path)};

    const auto result = ParseMaps(data);
    ASSERT_TRUE(result) << result.error().message();
    ASSERT_EQ(result.value().size(), 2);

    const ModuleInfo* hello_module_info;
    const ModuleInfo* no_symbols_module_info;
    if (result.value()[0].name() == "hello_world_elf") {
      hello_module_info = &result.value()[0];
      no_symbols_module_info = &result.value()[1];
    } else {
      hello_module_info = &result.value()[1];
      no_symbols_module_info = &result.value()[0];
    }

    EXPECT_EQ(hello_module_info->name(), "hello_world_elf");
    EXPECT_EQ(hello_module_info->file_path(), hello_world_path);
    EXPECT_EQ(hello_module_info->file_size(), 16616);
    EXPECT_EQ(hello_module_info->address_start(), 0x7f6874285000);
    EXPECT_EQ(hello_module_info->address_end(), 0x7f6874290000);
    EXPECT_EQ(hello_module_info->build_id(), "d12d54bc5b72ccce54a408bdeda65e2530740ac8");
    EXPECT_EQ(hello_module_info->load_bias(), 0x0);

    EXPECT_EQ(no_symbols_module_info->name(), "no_symbols_elf");
    EXPECT_EQ(no_symbols_module_info->file_path(), no_symbols_path);
    EXPECT_EQ(no_symbols_module_info->file_size(), 18768);
    EXPECT_EQ(no_symbols_module_info->address_start(), 0x0);
    EXPECT_EQ(no_symbols_module_info->address_end(), 0x1000);
    EXPECT_EQ(no_symbols_module_info->build_id(), "b5413574bbacec6eacb3b89b1012d0e2cd92ec6b");
    EXPECT_EQ(no_symbols_module_info->load_bias(), 0x400000);
  }
}
