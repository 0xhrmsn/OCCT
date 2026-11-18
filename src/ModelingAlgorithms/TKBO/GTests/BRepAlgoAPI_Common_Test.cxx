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

#include "BOPTest_Utilities.pxx"

#include <BRepGProp.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GeomAPI_PointsToBSplineSurface.hxx>
#include <Geom_BSplineSurface.hxx>
#include <NCollection_Array2.hxx>
#include <TopLoc_Location.hxx>

#include <cmath>

//=================================================================================================
// BOPCommon Simple Tests - migrating from /tests/boolean/bopcommon_simple/
//=================================================================================================

class BOPCommonSimpleTest : public BRepAlgoAPI_TestBase
{
};

// Test bopcommon_simple/A1: box b1 0 0 0 1 1 1; box b2 0 0 0 1 1 1; bop b1 b2; bopcommon result;
// checkprops result -s 6
TEST_F(BOPCommonSimpleTest, IdenticalBoxes_A1)
{
  const TopoDS_Shape aBox1 = BOPTest_Utilities::CreateBox(gp_Pnt(0, 0, 0), 1.0, 1.0, 1.0);
  const TopoDS_Shape aBox2 = BOPTest_Utilities::CreateBox(gp_Pnt(0, 0, 0), 1.0, 1.0, 1.0);

  const TopoDS_Shape aResult = PerformCommon(aBox1, aBox2);
  ValidateResult(aResult, 6.0);
}

// A ball end mill (radius 3, tip at the pole of its spherical face) sunk 0.1 into a wavy B-spline
// face: the intersection passes within 0.003 of the pole. The approximated curve deviated by up
// to 0.15 from the sphere, the loop broke and the common part was empty.
TEST(BOPCommonBallTest, BallTipNearPole_CommonFaceNotEmpty)
{
  NCollection_Array2<gp_Pnt> aPnts(1, 81, 1, 81);
  for (int i = 1; i <= 81; ++i)
  {
    for (int j = 1; j <= 81; ++j)
    {
      const double x = i - 1.0, y = j - 1.0;
      aPnts(i, j) = gp_Pnt(x, y, 2.0 * std::sin(2 * M_PI * x / 40) * std::cos(2 * M_PI * y / 40));
    }
  }
  GeomAPI_PointsToBSplineSurface anInterp;
  anInterp.Interpolate(aPnts);
  const occ::handle<Geom_BSplineSurface> aSurf = anInterp.Surface();
  const TopoDS_Face aWave = BRepBuilderAPI_MakeFace(aSurf, 1e-7).Face();

  // tool: profile in XZ revolved about Z, tip at the origin
  const double            R = 3.0, H = 20.0;
  BRepBuilderAPI_MakeWire aWire;
  aWire.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(gp_Pnt(0, 0, 0),
                                                       gp_Pnt(R * std::sin(M_PI / 4),
                                                              0,
                                                              R - R * std::cos(M_PI / 4)),
                                                       gp_Pnt(R, 0, R))
                                      .Value())
              .Edge());
  aWire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(R, 0, R), gp_Pnt(R, 0, H)).Edge());
  aWire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(R, 0, H), gp_Pnt(0, 0, H)).Edge());
  aWire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, H), gp_Pnt(0, 0, 0)).Edge());
  const TopoDS_Shape aTool =
    BRepPrimAPI_MakeRevol(BRepBuilderAPI_MakeFace(aWire.Wire(), true).Face(),
                          gp_Ax1(gp::Origin(), gp::DZ()))
      .Shape();

  // tip positions where the common part was empty
  for (const gp_Pnt& aTip : {gp_Pnt(63.4375, 37.839506172839506, -0.96517729893143811),
                             gp_Pnt(56.796875, 63.353909465020571, -0.83961555875486249)})
  {
    gp_Trsf aTrsf;
    aTrsf.SetTranslation(gp_Vec(aTip.XYZ()));
    const TopoDS_Shape aPlaced = aTool.Located(TopLoc_Location(aTrsf));

    BRepAlgoAPI_Common aCommon(aPlaced, aWave);
    ASSERT_TRUE(aCommon.IsDone());
    GProp_GProps aProps;
    BRepGProp::SurfaceProperties(aCommon.Shape(), aProps);

    // reference: area of the wave inside the ball, by a fine grid on the B-spline near the tip
    const gp_Pnt aCenter(aTip.X(), aTip.Y(), aTip.Z() + R);
    const int    aN  = 400;
    const double aU1 = (aTip.X() - 1.5) / 80.0, aU2 = (aTip.X() + 1.5) / 80.0;
    const double aV1 = (aTip.Y() - 1.5) / 80.0, aV2 = (aTip.Y() + 1.5) / 80.0;
    const double aDU = (aU2 - aU1) / aN, aDV = (aV2 - aV1) / aN;
    double       anArea = 0.0;
    for (int i = 0; i < aN; ++i)
    {
      for (int j = 0; j < aN; ++j)
      {
        gp_Pnt aP;
        gp_Vec aDu, aDv;
        aSurf->D1(aU1 + (i + 0.5) * aDU, aV1 + (j + 0.5) * aDV, aP, aDu, aDv);
        if (aP.Distance(aCenter) < R)
        {
          anArea += aDu.Crossed(aDv).Magnitude() * aDU * aDV;
        }
      }
    }
    ASSERT_GT(anArea, 1.0);
    EXPECT_NEAR(aProps.Mass(), anArea, 0.01 * anArea)
      << "tip " << aTip.X() << " " << aTip.Y() << " " << aTip.Z();
  }
}