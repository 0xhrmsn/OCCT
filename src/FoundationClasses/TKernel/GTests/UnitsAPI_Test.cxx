// Copyright (c) 2025 OPEN CASCADE SAS
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

#include <UnitsAPI.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <Message_Printer.hxx>

#include <gtest/gtest.h>

// Test BUC60727: UnitsAPI unit conversion
// Migrated from QABugs_3.cxx
TEST(UnitsAPI_Test, BUC60727_AnyToLS_Conversion)
{
  // Set local system to MDTV (Millimeter, Degree, Ton, Velocity)
  UnitsAPI::SetLocalSystem(UnitsAPI_MDTV);

  // Test conversion: 3 mm in the MDTV system should remain 3
  // (since the base unit for length in MDTV is millimeter)
  double aResult = UnitsAPI::AnyToLS(3.0, "mm");
  EXPECT_DOUBLE_EQ(3.0, aResult);
}

// Test of fix for bug 33176: UnitsAPI::AnyToAny must not crash when one of the units is unknown.
TEST(UnitsAPI_Test, AnyToAny_UnknownUnit)
{
  // Attempt to convert using an unknown unit, should not crash and return the input value.
  const double aResult1 = UnitsAPI::AnyToAny(1.0, "unknown_unit", "mm");
  const double aResult2 = UnitsAPI::AnyToAny(1.0, "mm", "unknown_unit");
  EXPECT_DOUBLE_EQ(1.0, aResult1);
  EXPECT_DOUBLE_EQ(1.0, aResult2);
}

namespace
{
//! Counts the warnings sent while it is attached to the default messenger.
class WarningCounter : public Message_Printer
{
public:
  void send(const TCollection_AsciiString&, const Message_Gravity theGravity) const override
  {
    if (theGravity == Message_Warning)
    {
      ++myNbWarnings;
    }
  }

  mutable int myNbWarnings = 0;
};
} // namespace

// An unknown unit was converted to 0 without any message, like a real 0.
TEST(UnitsAPI_Test, AnyToSI_UnknownUnit_Warns)
{
  occ::handle<WarningCounter> aCounter = new WarningCounter();
  Message::DefaultMessenger()->AddPrinter(aCounter);
  const double aKnown    = UnitsAPI::AnyToSI(1.0, "mm");
  const int    aNbKnown  = aCounter->myNbWarnings;
  const double anUnknown = UnitsAPI::AnyToSI(1.0, "xyz");
  Message::DefaultMessenger()->RemovePrinter(aCounter);
  EXPECT_DOUBLE_EQ(aKnown, 0.001);
  EXPECT_EQ(aNbKnown, 0);
  EXPECT_EQ(anUnknown, 0.0);
  EXPECT_EQ(aCounter->myNbWarnings, 1);
}
