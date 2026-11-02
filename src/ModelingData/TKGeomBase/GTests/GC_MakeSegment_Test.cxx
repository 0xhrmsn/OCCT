// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.
#include <GC_MakeSegment.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Precision.hxx>
#include <gp.hxx>
#include <gp_Pnt.hxx>

#include <gtest/gtest.h>

TEST(GC_MakeSegmentTest, FromPoints_Coincident_ConfusedPoints)
{
  const gp_Pnt aP(1.0, 2.0, 3.0);

  GC_MakeSegment aMaker(aP, aP);
  EXPECT_FALSE(aMaker.IsDone());
  EXPECT_EQ(aMaker.Status(), gce_ConfusedPoints);
}

TEST(GC_MakeSegmentTest, FromPoints_AtResolution_ConfusedPoints)
{
  GC_MakeSegment aMaker(gp_Pnt(0.0, 0.0, 0.0), gp_Pnt(gp::Resolution(), 0.0, 0.0));
  EXPECT_FALSE(aMaker.IsDone());
  EXPECT_EQ(aMaker.Status(), gce_ConfusedPoints);
}

TEST(GC_MakeSegmentTest, FromPoints_Distinct_Done)
{
  GC_MakeSegment aMaker(gp_Pnt(0.0, 0.0, 0.0), gp_Pnt(0.0, 3.0, 4.0));
  ASSERT_TRUE(aMaker.IsDone());
  EXPECT_EQ(aMaker.Status(), gce_Done);
  ASSERT_FALSE(aMaker.Value().IsNull());
  EXPECT_NEAR(aMaker.Value()->LastParameter() - aMaker.Value()->FirstParameter(),
              5.0,
              Precision::Confusion());
}