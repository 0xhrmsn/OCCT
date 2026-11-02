// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.
#include <GC_MakeArcOfCircle2d.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Precision.hxx>
#include <gp_Pnt2d.hxx>

#include <gtest/gtest.h>

TEST(GC_MakeArcOfCircle2dTest, ThreePoints_Collinear_ColinearPoints)
{
  GC_MakeArcOfCircle2d aMaker(gp_Pnt2d(0.0, 0.0), gp_Pnt2d(1.0, 0.0), gp_Pnt2d(2.0, 0.0));
  EXPECT_FALSE(aMaker.IsDone());
  EXPECT_EQ(aMaker.Status(), gce_ColinearPoints);
}

TEST(GC_MakeArcOfCircle2dTest, ThreePoints_AllCoincident_ConfusedPoints)
{
  const gp_Pnt2d aP(1.0, 1.0);

  GC_MakeArcOfCircle2d aMaker(aP, aP, aP);
  EXPECT_FALSE(aMaker.IsDone());
  EXPECT_EQ(aMaker.Status(), gce_ConfusedPoints);
}

TEST(GC_MakeArcOfCircle2dTest, ThreePoints_TwoCoincident_ConfusedPoints)
{
  GC_MakeArcOfCircle2d aMaker(gp_Pnt2d(0.0, 0.0), gp_Pnt2d(0.0, 0.0), gp_Pnt2d(1.0, 0.0));
  EXPECT_FALSE(aMaker.IsDone());
  EXPECT_EQ(aMaker.Status(), gce_ConfusedPoints);
}

TEST(GC_MakeArcOfCircle2dTest, ThreePoints_OnUnitCircle_Done)
{
  GC_MakeArcOfCircle2d aMaker(gp_Pnt2d(1.0, 0.0), gp_Pnt2d(0.0, 1.0), gp_Pnt2d(-1.0, 0.0));
  ASSERT_TRUE(aMaker.IsDone());
  const occ::handle<Geom2d_Circle> aCirc =
    occ::down_cast<Geom2d_Circle>(aMaker.Value()->BasisCurve());
  ASSERT_FALSE(aCirc.IsNull());
  EXPECT_NEAR(aCirc->Radius(), 1.0, Precision::Confusion());
}