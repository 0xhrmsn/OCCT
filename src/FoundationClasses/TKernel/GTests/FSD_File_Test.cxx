// Copyright (c) 2026 OPEN CASCADE SAS
//
// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.
//
// Alternatively, this file may be used under the terms of Open CASCADE
// commercial license or contractual agreement.

#include <FSD_File.hxx>
#include <Storage_StreamTypeMismatchError.hxx>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

// The reals of a document were read as words into a buffer of 100 bytes without a limit: a
// longer word in a damaged or crafted document was written past the end of the buffer.
TEST(FSD_FileTest, GetReal_LongWord)
{
  const std::filesystem::path aPath =
    std::filesystem::temp_directory_path() / "occt_fsd_file_long_real.txt";
  {
    std::ofstream anOut(aPath, std::ios::binary);
    anOut << "1.5 2.25 " << std::string(99, '1') << " " << std::string(300, '1') << " 3.5\n";
  }

  FSD_File aFile;
  ASSERT_EQ(aFile.Open(TCollection_AsciiString(aPath.u8string().c_str()), Storage_VSRead),
            Storage_VSOk);
  double aReal  = 0.0;
  float  aShort = 0.0f;
  aFile.GetReal(aReal);
  EXPECT_EQ(aReal, 1.5);
  aFile.GetShortReal(aShort);
  EXPECT_EQ(aShort, 2.25f);
  aFile.GetReal(aReal); // the longest word that fits
  EXPECT_GT(aReal, 1.0e98);
  EXPECT_THROW(aFile.GetReal(aReal), Storage_StreamTypeMismatchError);
  aFile.Close();
  std::filesystem::remove(aPath);
}
