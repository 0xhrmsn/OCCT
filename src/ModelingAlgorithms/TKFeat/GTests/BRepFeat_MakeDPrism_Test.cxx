// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFeat_MakeDPrism.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <gp.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

#include <gtest/gtest.h>

#include <cmath>

namespace
{
constexpr double THE_HEIGHT = 15.0;

double volumeOf(const TopoDS_Shape& theShape)
{
  GProp_GProps aProps;
  BRepGProp::VolumeProperties(theShape, aProps);
  return aProps.Mass();
}

// Box 100 x 100 x 20; theTop receives its top face (z = 20).
TopoDS_Shape box(TopoDS_Face& theTop)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(gp_Pnt(-50.0, -50.0, 0.0), 100.0, 100.0, 20.0);
  for (TopExp_Explorer anExp(aBox, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    GProp_GProps aProps;
    BRepGProp::SurfaceProperties(anExp.Current(), aProps);
    if (std::abs(aProps.CentreOfMass().Z() - 20.0) < 1.0e-9)
    {
      theTop = TopoDS::Face(anExp.Current());
    }
  }
  return aBox;
}

// Rectangle 40 x 30 at z = 20, optionally with a hole of radius 5 at its centre.
TopoDS_Face profile(const bool theWithHole)
{
  BRepBuilderAPI_MakePolygon aPoly(gp_Pnt(-20.0, -15.0, 20.0),
                                   gp_Pnt(20.0, -15.0, 20.0),
                                   gp_Pnt(20.0, 15.0, 20.0),
                                   gp_Pnt(-20.0, 15.0, 20.0),
                                   true);
  BRepBuilderAPI_MakeFace    aMakeFace(aPoly.Wire(), true);
  if (theWithHole)
  {
    TopoDS_Wire aHole = BRepBuilderAPI_MakeWire(
      BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(0.0, 0.0, 20.0), gp::DZ()), 5.0)).Edge());
    aHole.Reverse();
    aMakeFace.Add(aHole);
  }
  return aMakeFace.Face();
}

// Exact volume of the drafted boss of height THE_HEIGHT: the rectangle shrinks with sharp
// corners, the hole grows as a cone.
double bossVolume(const double theAngle, const bool theWithHole)
{
  const double k = std::tan(theAngle), h = THE_HEIGHT;
  double       aVolume = 1200.0 * h - 70.0 * k * h * h + 4.0 * k * k * h * h * h / 3.0;
  if (theWithHole)
  {
    aVolume -= M_PI * (25.0 * h + 5.0 * k * h * h + k * k * h * h * h / 3.0);
  }
  return aVolume;
}

// Builds the drafted boss on the top face of the box.
void makeBoss(BRepFeat_MakeDPrism& theDPrism,
              TopoDS_Shape&        theBox,
              const bool           theWithHole,
              const double         theAngleDeg)
{
  TopoDS_Face aTop;
  theBox = box(aTop);
  theDPrism.Init(theBox, profile(theWithHole), aTop, theAngleDeg * M_PI / 180.0, 1, true);
  theDPrism.Perform(THE_HEIGHT);
}
} // namespace

// A profile with a hole lost the hole in the caps of the drafted prism; the feature had no
// result and was reported as done.
TEST(BRepFeat_MakeDPrismTest, HoledProfile_BossKeepsTheHole)
{
  for (const double anAngle : {1.0, 10.0})
  {
    BRepFeat_MakeDPrism aDPrism;
    TopoDS_Shape        aBox;
    makeBoss(aDPrism, aBox, true, anAngle);
    ASSERT_TRUE(aDPrism.IsDone()) << "angle " << anAngle;
    EXPECT_EQ(aDPrism.CurrentStatusError(), BRepFeat_OK);
    EXPECT_TRUE(BRepCheck_Analyzer(aDPrism.Shape()).IsValid());
    const double aRef = bossVolume(anAngle * M_PI / 180.0, true);
    EXPECT_NEAR(volumeOf(aDPrism.Shape()) - volumeOf(aBox), aRef, 1.0e-9 * aRef)
      << "angle " << anAngle;
  }
}

TEST(BRepFeat_MakeDPrismTest, ProfileWithoutHole_BossUnchanged)
{
  BRepFeat_MakeDPrism aDPrism;
  TopoDS_Shape        aBox;
  makeBoss(aDPrism, aBox, false, 10.0);
  ASSERT_TRUE(aDPrism.IsDone());
  EXPECT_EQ(aDPrism.CurrentStatusError(), BRepFeat_OK);
  EXPECT_TRUE(BRepCheck_Analyzer(aDPrism.Shape()).IsValid());
  const double aRef = bossVolume(10.0 * M_PI / 180.0, false);
  EXPECT_NEAR(volumeOf(aDPrism.Shape()) - volumeOf(aBox), aRef, 1.0e-9 * aRef);
}

// The top of the boss collapses (offset 15 = half width): no solid tool can be built, but the
// feature was reported as done with an empty result.
TEST(BRepFeat_MakeDPrismTest, CollapsingProfile_ReportedAsFailure)
{
  BRepFeat_MakeDPrism aDPrism;
  TopoDS_Shape        aBox;
  makeBoss(aDPrism, aBox, false, 45.0);
  EXPECT_FALSE(aDPrism.IsDone());
  EXPECT_EQ(aDPrism.CurrentStatusError(), BRepFeat_LocOpeNotDone);
}

// At 30 degrees the growing hole breaks through the long sides of the shrinking rectangle. The
// drafted prism is a solid with local defects; the boolean result is valid, its volume within
// 1e-6 of the reference.
// Reference volume: integral of the section area (rectangle minus its part covered by the disk),
// computed piecewise between the breakpoints of the section shape.
TEST(BRepFeat_MakeDPrismTest, HoleBreakingThroughSides_BossIsValid)
{
  BRepFeat_MakeDPrism aDPrism;
  TopoDS_Shape        aBox;
  makeBoss(aDPrism, aBox, true, 30.0);
  ASSERT_TRUE(aDPrism.IsDone());
  EXPECT_EQ(aDPrism.CurrentStatusError(), BRepFeat_OK);
  EXPECT_TRUE(BRepCheck_Analyzer(aDPrism.Shape()).IsValid());
  const double aRef = 6677.226072124;
  EXPECT_NEAR(volumeOf(aDPrism.Shape()) - volumeOf(aBox), aRef, 1.0e-6 * aRef);
}

// At 20 degrees the drafted prism built for the holed profile has a negative volume (its faces
// are inverted); the feature must not be reported as done.
TEST(BRepFeat_MakeDPrismTest, InvertedDraftTool_ReportedAsFailure)
{
  BRepFeat_MakeDPrism aDPrism;
  TopoDS_Shape        aBox;
  makeBoss(aDPrism, aBox, true, 20.0);
  EXPECT_FALSE(aDPrism.IsDone());
  EXPECT_EQ(aDPrism.CurrentStatusError(), BRepFeat_LocOpeNotDone);
}

// Negative draft: the rectangle grows (its corners are rounded by the evolved offset) and the hole
// shrinks. The top cap is built outside the spine, from one face per wire of the spine; the hole
// must be put in the cap of the outer contour.
TEST(BRepFeat_MakeDPrismTest, HoledProfileNegativeAngle_BossKeepsTheHole)
{
  BRepFeat_MakeDPrism aDPrism;
  TopoDS_Shape        aBox;
  makeBoss(aDPrism, aBox, true, -10.0);
  ASSERT_TRUE(aDPrism.IsDone());
  EXPECT_EQ(aDPrism.CurrentStatusError(), BRepFeat_OK);
  EXPECT_TRUE(BRepCheck_Analyzer(aDPrism.Shape()).IsValid());
  const double k = std::tan(10.0 * M_PI / 180.0), h = THE_HEIGHT;
  const double aRef = 1200.0 * h + 70.0 * k * h * h + M_PI * k * k * h * h * h / 3.0
                      - M_PI * (25.0 * h - 5.0 * k * h * h + k * k * h * h * h / 3.0);
  EXPECT_NEAR(volumeOf(aDPrism.Shape()) - volumeOf(aBox), aRef, 1.0e-9 * aRef);
}

// Pocket through the whole box (two-height drafted prism): the pocket widens with rounded corners
// and the material left inside the hole of the profile shrinks.
TEST(BRepFeat_MakeDPrismTest, HoledProfileThruAll_PocketKeepsTheIsland)
{
  TopoDS_Face         aTop;
  const TopoDS_Shape  aBox = box(aTop);
  BRepFeat_MakeDPrism aDPrism(aBox, profile(true), aTop, 5.0 * M_PI / 180.0, 0, true);
  aDPrism.PerformThruAll();
  ASSERT_TRUE(aDPrism.IsDone());
  EXPECT_EQ(aDPrism.CurrentStatusError(), BRepFeat_OK);
  EXPECT_TRUE(BRepCheck_Analyzer(aDPrism.Shape()).IsValid());
  const double k = std::tan(5.0 * M_PI / 180.0), d = 20.0;
  const double aRef = 1200.0 * d + 70.0 * k * d * d + M_PI * k * k * d * d * d / 3.0
                      - M_PI * (25.0 * d - 5.0 * k * d * d + k * k * d * d * d / 3.0);
  EXPECT_NEAR(volumeOf(aBox) - volumeOf(aDPrism.Shape()), aRef, 1.0e-9 * aRef);
}
