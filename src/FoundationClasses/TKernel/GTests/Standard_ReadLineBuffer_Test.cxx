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

#include <Standard_ReadLineBuffer.hxx>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

// The last line of a stream without an end-of-line symbol was returned without the null terminator,
// so line parsers read past the buffer (ASan heap-buffer-overflow in Strtod from RWObj_Reader on a
// truncated OBJ file).
TEST(Standard_ReadLineBufferTest, LastLineWithoutEndOfLine_IsNullTerminated)
{
  std::istringstream      aStream("v 1 2 3\nvt 0.5 0.25");
  Standard_ReadLineBuffer aBuffer(64);
  size_t                  aLen  = 0;
  const char*             aLine = aBuffer.ReadLine(aStream, aLen);
  ASSERT_NE(aLine, nullptr);
  EXPECT_EQ(std::string(aLine, aLen), "v 1 2 3");
  EXPECT_EQ(aLine[aLen], '\0');

  aLine = aBuffer.ReadLine(aStream, aLen);
  ASSERT_NE(aLine, nullptr);
  ASSERT_EQ(aLen, 11u);
  EXPECT_EQ(std::string(aLine, aLen), "vt 0.5 0.25");
  EXPECT_EQ(aLine[aLen], '\0');

  EXPECT_EQ(aBuffer.ReadLine(aStream, aLen), nullptr);
}

// Same with a line split between two read chunks.
TEST(Standard_ReadLineBufferTest, LastLineAcrossChunks_IsNullTerminated)
{
  std::istringstream      aStream("vt 0.5 0.25");
  Standard_ReadLineBuffer aBuffer(4);
  size_t                  aLen  = 0;
  const char*             aLine = aBuffer.ReadLine(aStream, aLen);
  ASSERT_NE(aLine, nullptr);
  ASSERT_EQ(aLen, 11u);
  EXPECT_EQ(std::string(aLine, aLen), "vt 0.5 0.25");
  EXPECT_EQ(aLine[aLen], '\0');
}
