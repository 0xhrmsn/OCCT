// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <DE_ConfigurationContext.hxx>

#include <gtest/gtest.h>

// A line holding only whitespace (also the "\r" left by a CRLF file) threw Standard_OutOfRange.
TEST(DE_ConfigurationContextTest, LoadStr_BlankLines_Skipped)
{
  occ::handle<DE_ConfigurationContext> aContext = new DE_ConfigurationContext();
  ASSERT_TRUE(
    aContext->LoadStr("first.param : 1\n   \n\t\nsecond.param : 2\r\n\r\nthird.param : 3\n"));
  EXPECT_EQ(aContext->IntegerVal("first.param", 0), 1);
  EXPECT_EQ(aContext->IntegerVal("second.param", 0), 2);
  EXPECT_EQ(aContext->IntegerVal("third.param", 0), 3);
}