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

#include <gtest/gtest.h>

#include <BinLDrivers_DocumentSection.hxx>
#include <TDocStd_FormatVersion.hxx>

#include <cstdint>
#include <sstream>
#include <string>

namespace
{

//! Entry of the table of contents: the size of the name, the name and three 64-bit values.
std::string tocEntry(const int theNameSize)
{
  std::string aData(reinterpret_cast<const char*>(&theNameSize), sizeof(int));
  aData += std::string(static_cast<size_t>(theNameSize), 'N');
  aData += std::string(3 * sizeof(uint64_t), '\0');
  return aData;
}

} // namespace

// The name of a section is read into a buffer of 512 bytes. A damaged or crafted document that
// declares a name of 512 bytes had its terminating null written past the end of the buffer.
TEST(BinLDrivers_DocumentSectionTest, ReadTOC_NameSizeLimit)
{
  for (const int aSize : {508, 511, 512, 513})
  {
    std::istringstream          anIn(tocEntry(aSize));
    BinLDrivers_DocumentSection aSection;
    const bool                  isRead =
      BinLDrivers_DocumentSection::ReadTOC(aSection, anIn, TDocStd_FormatVersion_CURRENT);
    EXPECT_EQ(isRead, aSize < 512) << aSize;
    if (isRead)
    {
      EXPECT_EQ(aSection.Name().Length(), aSize) << aSize;
    }
  }
}

// Guard: a section written by OCCT, with the longest name the writer keeps, is read back.
TEST(BinLDrivers_DocumentSectionTest, WriteReadTOC_LongName)
{
  BinLDrivers_DocumentSection aWritten(TCollection_AsciiString(std::string(600, 'S').c_str()),
                                       false);
  std::ostringstream          anOut;
  aWritten.WriteTOC(anOut, TDocStd_FormatVersion_CURRENT);

  std::istringstream          anIn(anOut.str());
  BinLDrivers_DocumentSection aRead;
  ASSERT_TRUE(BinLDrivers_DocumentSection::ReadTOC(aRead, anIn, TDocStd_FormatVersion_CURRENT));
  EXPECT_EQ(aRead.Name().Length(), 507);
}
