// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <LDOM_OSStream.hxx>
#include <LDOMParser.hxx>

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

namespace
{
std::string content(const LDOM_OSStream& theStream)
{
  const std::unique_ptr<const char[]> aStr(theStream.str());
  return std::string(aStr.get());
}
} // namespace

// Single characters reach LDOM_SBuffer::overflow(), which passes one char on the stack to xsputn();
// xsputn() copied n + 1 bytes, reading one byte past that char (visible under AddressSanitizer).
// The buffer size is small so that the characters also cross element boundaries.
TEST(LDOM_OSStreamTest, SingleCharacters_AcrossElements)
{
  LDOM_OSStream aStream(8);
  std::string   anExpected;
  for (int i = 0; i < 100; ++i)
  {
    const char aChar = static_cast<char>('a' + i % 26);
    aStream << aChar;
    anExpected += aChar;
  }
  EXPECT_EQ(aStream.Length(), 100);
  EXPECT_EQ(content(aStream), anExpected);
}

// A string that is not null-terminated must be copied by its length only.
TEST(LDOM_OSStreamTest, Write_NotNullTerminated)
{
  const char    aData[6] = {'x', 'y', 'z', 'u', 'v', 'w'};
  LDOM_OSStream aStream(4);
  aStream.write(aData, 3);
  aStream.write(aData + 3, 3);
  EXPECT_EQ(content(aStream), "xyzuvw");
}

// A length below zero stores nothing. LDOM_XmlReader passes -1 for an empty DOCTYPE declaration.
TEST(LDOM_OSStreamTest, NegativeLength_StoresNothing)
{
  LDOM_OSStream aStream(8);
  aStream << "ab";
  aStream.rdbuf()->sputn("xyz", -1);
  aStream << "cd";
  EXPECT_EQ(aStream.Length(), 4);
  EXPECT_EQ(content(aStream), "abcd");
}

// The declaration comes from the file, so this path is reachable from an untrusted document.
TEST(LDOM_OSStreamTest, Parse_EmptyDoctype)
{
  for (const bool isTagPerStep : {false, true})
  {
    std::istringstream anInput("<!DOCTYPE ><a/>");
    LDOMParser         aParser;
    EXPECT_FALSE(aParser.parse(anInput, isTagPerStep)) << "tag per step: " << isTagPerStep;
  }
}
