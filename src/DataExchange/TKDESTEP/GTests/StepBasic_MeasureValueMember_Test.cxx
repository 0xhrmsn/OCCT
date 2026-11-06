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

#include <StepBasic_MeasureValueMember.hxx>

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

// SetName() checked one character at a fixed index (up to 9) before strcmp(), so a shorter name was
// read past its end (ASan heap-buffer-overflow when writing a material density named "density" to
// STEP; the name also comes from STEP files through StepData_SelectType). The buffers are
// heap-allocated with the exact size so that ASan sees an overrun.
TEST(StepBasic_MeasureValueMemberTest, SetName_ShortUnknownName_NoOverrun)
{
  occ::handle<StepBasic_MeasureValueMember> aMember = new StepBasic_MeasureValueMember();
  for (const char* aName : {"density", "PO", "POSITIVE", "AB", "TI"})
  {
    const std::vector<char> aText(aName, aName + std::strlen(aName) + 1);
    EXPECT_FALSE(aMember->SetName(aText.data())) << aName;
  }
}

TEST(StepBasic_MeasureValueMemberTest, SetName_KnownNames_RoundTrip)
{
  occ::handle<StepBasic_MeasureValueMember> aMember = new StepBasic_MeasureValueMember();
  for (const char* aName : {"LENGTH_MEASURE",
                            "TIME_MEASURE",
                            "PLANE_ANGLE_MEASURE",
                            "SOLID_ANGLE_MEASURE",
                            "RATIO_MEASURE",
                            "PARAMETER_VALUE",
                            "CONTEXT_DEPENDANT_MEASURE",
                            "POSITIVE_LENGTH_MEASURE",
                            "POSITIVE_PLANE_ANGLE_MEASURE",
                            "POSITIVE_RATIO_MEASURE",
                            "AREA_MEASURE",
                            "VOLUME_MEASURE",
                            "MASS_MEASURE",
                            "THERMODYNAMIC_TEMPERATURE_MEASURE",
                            "COUNT_MEASURE",
                            "NUMERIC_MEASURE"})
  {
    const std::vector<char> aText(aName, aName + std::strlen(aName) + 1);
    ASSERT_TRUE(aMember->SetName(aText.data())) << aName;
    EXPECT_EQ(std::string(aMember->Name()), aName);
  }
}
