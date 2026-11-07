// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <LDOMParser.hxx>
#include <LDOM_Element.hxx>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace
{
std::string nested(const int theDepth)
{
  std::string aXml = "<?xml version=\"1.0\"?>";
  for (int i = 0; i < theDepth; ++i)
  {
    aXml += "<n>";
  }
  for (int i = 0; i < theDepth; ++i)
  {
    aXml += "</n>";
  }
  return aXml;
}
} // namespace

TEST(LDOMParserTest, Parse_NestingAtLimit_Succeeds)
{
  std::istringstream aStream(nested(1000));
  LDOMParser         aParser;
  EXPECT_FALSE(aParser.parse(aStream));
}

// Each nesting level used to be one unbounded recursive call: a crafted file overflowed the stack.
TEST(LDOMParserTest, Parse_NestingTooDeep_FailsCleanly)
{
  for (const int aDepth : {1001, 100000})
  {
    std::istringstream aStream(nested(aDepth));
    LDOMParser         aParser;
    EXPECT_TRUE(aParser.parse(aStream)) << "depth " << aDepth;
    TCollection_AsciiString aData;
    EXPECT_STREQ(aParser.GetError(aData).ToCString(), "Elements nested too deeply");
  }
}

TEST(LDOMParserTest, Parse_CharReferences_Decoded)
{
  std::istringstream aStream(
    "<?xml version=\"1.0\"?><a x=\"A&#66;&amp;\">C&#x44;&lt;<!--E&#70;--></a>");
  LDOMParser aParser;
  ASSERT_FALSE(aParser.parse(aStream));
  const LDOM_Element anElem = aParser.getDocument().getDocumentElement();
  EXPECT_STREQ(anElem.getAttribute("x").GetString(), "AB&");
  const LDOM_Node aText = anElem.getFirstChild();
  EXPECT_STREQ(aText.getNodeValue().GetString(), "CD<");
  EXPECT_STREQ(aText.getNextSibling().getNodeValue().GetString(), "EF");
}

// A character reference that cannot be decoded used to give a null string, which was then read.
TEST(LDOMParserTest, Parse_InvalidCharReference_FailsCleanly)
{
  for (const char* aXml : {"<?xml version=\"1.0\"?><a>&#xFFFFFFFF;</a>",
                           "<?xml version=\"1.0\"?><a>x&#0;</a>",
                           "<?xml version=\"1.0\"?><a>&#65</a>",
                           "<?xml version=\"1.0\"?><a><!--&#300;--></a>",
                           "<?xml version=\"1.0\"?><a x=\"&#0;\"/>",
                           "<?xml version=\"1.0\"?><a x=\"1&#xFFFF;\"></a>"})
  {
    std::istringstream aStream(aXml);
    LDOMParser         aParser;
    EXPECT_TRUE(aParser.parse(aStream)) << aXml;
    TCollection_AsciiString aData;
    EXPECT_STREQ(aParser.GetError(aData).ToCString(), "Invalid character reference") << aXml;
  }
}