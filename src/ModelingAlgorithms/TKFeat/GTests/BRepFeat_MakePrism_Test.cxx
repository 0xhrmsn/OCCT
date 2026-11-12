// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFeat_MakePrism.hxx>
#include <BRepFeat_MakeRevol.hxx>
#include <gp_Pln.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopTools_ShapeMapHasher.hxx>

#include <gtest/gtest.h>

#include <cmath>
#include <functional>

namespace
{
using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

double volumeOf(const TopoDS_Shape& theShape)
{
  GProp_GProps aProps;
  BRepGProp::VolumeProperties(theShape, aProps);
  return aProps.Mass();
}

double areaOf(const TopoDS_Shape& theFace)
{
  GProp_GProps aProps;
  BRepGProp::SurfaceProperties(theFace, aProps);
  return aProps.Mass();
}

// Plane face z = theZ covering the blocks of the tests.
TopoDS_Face rectangleAt(const double theZ)
{
  return BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakePolygon(gp_Pnt(-10, -10, theZ),
                                                            gp_Pnt(110, -10, theZ),
                                                            gp_Pnt(110, 70, theZ),
                                                            gp_Pnt(-10, 70, theZ),
                                                            true)
                                   .Wire(),
                                 true)
    .Face();
}

gp_Pnt centreOf(const TopoDS_Shape& theFace)
{
  GProp_GProps aProps;
  BRepGProp::SurfaceProperties(theFace, aProps);
  return aProps.CentreOfMass();
}

TopoDS_Face faceNearest(const TopoDS_Shape& theShape, const gp_Pnt& thePnt)
{
  TopoDS_Face aBest;
  double      aDist = 1.0e100;
  for (TopExp_Explorer anExp(theShape, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    const double aD = centreOf(anExp.Current()).Distance(thePnt);
    if (aD < aDist)
    {
      aDist = aD;
      aBest = TopoDS::Face(anExp.Current());
    }
  }
  return aBest;
}

// Block 100 x 60 with a slanted top face: z = 20 + 0.2 x.
TopoDS_Shape slantedBlock()
{
  const TopoDS_Face aTrapezoid =
    BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakePolygon(gp_Pnt(0, 0, 0),
                                                       gp_Pnt(100, 0, 0),
                                                       gp_Pnt(100, 0, 40),
                                                       gp_Pnt(0, 0, 20),
                                                       true)
                              .Wire(),
                            true);
  return BRepPrimAPI_MakePrism(aTrapezoid, gp_Vec(0, 60, 0)).Shape();
}

TopoDS_Face disk(const gp_Pnt& theCentre, const double theRadius)
{
  return BRepBuilderAPI_MakeFace(
           BRepBuilderAPI_MakeWire(
             BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(theCentre, gp::DZ()), theRadius)))
             .Wire(),
           true)
    .Face();
}

// Number of faces of the result that are not traced back to the inputs through Modified and
// Generated, nor given by FirstShape/LastShape; also checks that FirstShape/LastShape only give
// faces of the result.
int untracedFaces(BRepFeat_MakePrism& thePrism,
                  const TopoDS_Shape& theBase,
                  const TopoDS_Face&  theProfile,
                  const TopoDS_Shape& theOther = TopoDS_Shape())
{
  ShapeMap aResult;
  TopExp::MapShapes(thePrism.Shape(), TopAbs_FACE, aResult);
  ShapeMap aTraced;
  auto     aTrace = [&](const NCollection_List<TopoDS_Shape>& theList) {
    for (const TopoDS_Shape& aShape : theList)
    {
      EXPECT_TRUE(aResult.Contains(aShape)) << "history gives a face that is not in the result";
      aTraced.Add(aShape);
    }
  };
  for (const TopoDS_Shape& anInput : {theBase, TopoDS_Shape(theProfile), theOther})
  {
    if (anInput.IsNull())
    {
      continue;
    }
    ShapeMap aSub;
    TopExp::MapShapes(anInput, TopAbs_FACE, aSub);
    TopExp::MapShapes(anInput, TopAbs_EDGE, aSub);
    TopExp::MapShapes(anInput, TopAbs_VERTEX, aSub);
    for (int i = 1; i <= aSub.Extent(); ++i)
    {
      if (aResult.Contains(aSub(i)))
      {
        aTraced.Add(aSub(i));
      }
      aTrace(thePrism.Modified(aSub(i)));
      aTrace(thePrism.Generated(aSub(i)));
    }
  }
  aTrace(thePrism.FirstShape());
  aTrace(thePrism.LastShape());
  int aNb = 0;
  for (int i = 1; i <= aResult.Extent(); ++i)
  {
    aNb += aTraced.Contains(aResult(i)) ? 0 : 1;
  }
  return aNb;
}

void checkPocket(const std::function<
                   void(BRepFeat_MakePrism&, const TopoDS_Face&, const TopoDS_Face&)>& thePerform,
                 const double                                                          theRefVolume)
{
  const TopoDS_Shape aBase    = slantedBlock();
  const TopoDS_Face  aProfile = disk(gp_Pnt(50, 30, 0), 8.0);
  const TopoDS_Face  aBottom  = faceNearest(aBase, gp_Pnt(50, 30, 0));
  const TopoDS_Face  aSlanted = faceNearest(aBase, gp_Pnt(50, 30, 30));
  BRepFeat_MakePrism aPrism(aBase, aProfile, aBottom, gp::DZ(), 0, true);
  thePerform(aPrism, aBottom, aSlanted);
  ASSERT_TRUE(aPrism.IsDone());
  EXPECT_TRUE(BRepCheck_Analyzer(aPrism.Shape()).IsValid());
  if (theRefVolume > 0.0)
  {
    EXPECT_NEAR(volumeOf(aBase) - volumeOf(aPrism.Shape()), theRefVolume, 1.0e-6 * theRefVolume);
  }
  EXPECT_EQ(untracedFaces(aPrism, aBase, aProfile), 0);
}
} // namespace

// The prism trimmed by a face (no gluing) is built by BRepFeat_Builder, whose history was not
// passed on: none of the faces of the result was traced back to the base or the profile, and
// FirstShape/LastShape gave faces that are not in the result.
TEST(BRepFeat_MakePrismTest, PocketUntilFace_HistoryComplete)
{
  // Disk of radius 8 up to the plane z = 20 + 0.2 x: mean height 30 at x = 50.
  checkPocket([](BRepFeat_MakePrism& theP,
                 const TopoDS_Face&,
                 const TopoDS_Face& theUntil) { theP.Perform(theUntil); },
              M_PI * 64.0 * 30.0);
}

// Same pocket: each face of the result is reported from the right input.
TEST(BRepFeat_MakePrismTest, PocketUntilFace_FacesMappedExactly)
{
  const TopoDS_Shape aBase    = slantedBlock();
  const TopoDS_Face  aProfile = disk(gp_Pnt(50, 30, 0), 8.0);
  const TopoDS_Face  aBottom  = faceNearest(aBase, gp_Pnt(50, 30, 0));
  const TopoDS_Face  aSlanted = faceNearest(aBase, gp_Pnt(50, 30, 30));
  BRepFeat_MakePrism aPrism(aBase, aProfile, aBottom, gp::DZ(), 0, true);
  aPrism.Perform(aSlanted);
  ASSERT_TRUE(aPrism.IsDone());
  const double aHole = M_PI * 64.0, aSlope = std::sqrt(1.04);

  const NCollection_List<TopoDS_Shape> aBottomMod = aPrism.Modified(aBottom);
  ASSERT_EQ(aBottomMod.Extent(), 1);
  EXPECT_NEAR(centreOf(aBottomMod.First()).Z(), 0.0, 1.0e-9);
  EXPECT_NEAR(areaOf(aBottomMod.First()), 6000.0 - aHole, 1.0e-6);
  EXPECT_FALSE(aPrism.IsDeleted(aBottom));

  // The slanted face loses the ellipse cut by the cylinder: area of the disk / cos(slope).
  const NCollection_List<TopoDS_Shape> aSlantedMod = aPrism.Modified(aSlanted);
  ASSERT_EQ(aSlantedMod.Extent(), 1);
  EXPECT_NEAR(areaOf(aSlantedMod.First()), aSlope * (6000.0 - aHole), 1.0e-6);

  const TopoDS_Shape                   aCircle = TopExp_Explorer(aProfile, TopAbs_EDGE).Current();
  const NCollection_List<TopoDS_Shape> aWall   = aPrism.Generated(aCircle);
  ASSERT_EQ(aWall.Extent(), 1);
  EXPECT_EQ(BRepAdaptor_Surface(TopoDS::Face(aWall.First())).GetType(), GeomAbs_Cylinder);

  // A through pocket has no cap.
  EXPECT_TRUE(aPrism.FirstShape().IsEmpty());
  EXPECT_TRUE(aPrism.LastShape().IsEmpty());
}

TEST(BRepFeat_MakePrismTest, PocketFromEnd_HistoryComplete)
{
  checkPocket([](BRepFeat_MakePrism& theP,
                 const TopoDS_Face&,
                 const TopoDS_Face& theUntil) { theP.PerformFromEnd(theUntil); },
              -1.0);
}

TEST(BRepFeat_MakePrismTest, PocketUntilHeight_HistoryComplete)
{
  checkPocket([](BRepFeat_MakePrism& theP,
                 const TopoDS_Face&,
                 const TopoDS_Face& theUntil) { theP.PerformUntilHeight(theUntil, 10.0); },
              M_PI * 64.0 * 10.0);
}

// Pocket of height 10: its bottom cap at z = 10 is the only last shape.
TEST(BRepFeat_MakePrismTest, PocketUntilHeight_CapIsTheLastShape)
{
  const TopoDS_Shape aBase    = slantedBlock();
  const TopoDS_Face  aProfile = disk(gp_Pnt(50, 30, 0), 8.0);
  BRepFeat_MakePrism aPrism(aBase,
                            aProfile,
                            faceNearest(aBase, gp_Pnt(50, 30, 0)),
                            gp::DZ(),
                            0,
                            true);
  aPrism.PerformUntilHeight(faceNearest(aBase, gp_Pnt(50, 30, 30)), 10.0);
  ASSERT_TRUE(aPrism.IsDone());
  ASSERT_EQ(aPrism.LastShape().Extent(), 1);
  EXPECT_NEAR(centreOf(aPrism.LastShape().First()).Z(), 10.0, 1.0e-9);
  EXPECT_NEAR(areaOf(aPrism.LastShape().First()), M_PI * 64.0, 1.0e-6);
}

// Boss up to a face that is not part of the base (fuse; the tool is cut by that face). The cap of
// the boss comes from the until face.
TEST(BRepFeat_MakePrismTest, BossUntilOuterFace_HistoryComplete)
{
  const TopoDS_Shape aBase    = BRepPrimAPI_MakeBox(100, 60, 20).Shape();
  const TopoDS_Face  aProfile = disk(gp_Pnt(50, 30, 20), 8.0);
  const TopoDS_Face  anUntil  = rectangleAt(35.0);
  BRepFeat_MakePrism aPrism(aBase,
                            aProfile,
                            faceNearest(aBase, gp_Pnt(50, 30, 20)),
                            gp::DZ(),
                            1,
                            true);
  aPrism.Perform(anUntil);
  ASSERT_TRUE(aPrism.IsDone());
  EXPECT_TRUE(BRepCheck_Analyzer(aPrism.Shape()).IsValid());
  EXPECT_NEAR(volumeOf(aPrism.Shape()) - volumeOf(aBase),
              M_PI * 64.0 * 15.0,
              1.0e-6 * M_PI * 64.0 * 15.0);
  EXPECT_EQ(untracedFaces(aPrism, aBase, aProfile, anUntil), 0);
  const NCollection_List<TopoDS_Shape> aCap = aPrism.Modified(anUntil);
  ASSERT_EQ(aCap.Extent(), 1);
  EXPECT_NEAR(centreOf(aCap.First()).Z(), 35.0, 1.0e-9);
  EXPECT_NEAR(areaOf(aCap.First()), M_PI * 64.0, 1.0e-6);
}

// Guard, not on the changed path: through all is a direct BRepAlgoAPI_Cut.
TEST(BRepFeat_MakePrismTest, PocketThruAll_HistoryComplete)
{
  checkPocket(
    [](BRepFeat_MakePrism& theP, const TopoDS_Face&, const TopoDS_Face&) { theP.PerformThruAll(); },
    M_PI * 64.0 * 30.0);
}

// A pocket from a plane face z = 5 (not a face of the base) up to the slanted face removed nothing,
// yet the feature was done: the result was the base, its slanted face split, with the part of the
// tool kept as a second solid inside it. It must either remove the material between z = 5 and the
// slanted face, or report that the base is unchanged.
TEST(BRepFeat_MakePrismTest, PocketFromOuterFace_NoSilentNoOp)
{
  const TopoDS_Shape aBase    = slantedBlock();
  const TopoDS_Face  aProfile = disk(gp_Pnt(50, 30, 0), 8.0);
  const TopoDS_Face  aBottom  = faceNearest(aBase, gp_Pnt(50, 30, 0));
  const TopoDS_Face  aSlanted = faceNearest(aBase, gp_Pnt(50, 30, 30));
  BRepFeat_MakePrism aPrism(aBase, aProfile, aBottom, gp::DZ(), 0, true);
  aPrism.Perform(rectangleAt(5.0), aSlanted);
  if (aPrism.IsDone())
  {
    // mean height of the slanted face over the disk is 30
    EXPECT_NEAR(volumeOf(aBase) - volumeOf(aPrism.Shape()),
                M_PI * 64.0 * 25.0,
                1.0e-6 * M_PI * 64.0 * 25.0);
  }
  else
  {
    EXPECT_EQ(aPrism.CurrentStatusError(), BRepFeat_NoChange);
  }
}

// MakePrism and MakeRevol set the status of their failure in a member that hid the one of
// BRepFeat_Form: CurrentStatusError() reported no error for a feature that was not done.
TEST(BRepFeat_MakePrismTest, FailureStatusReported)
{
  const TopoDS_Shape aBase    = slantedBlock();
  const TopoDS_Face  aProfile = disk(gp_Pnt(50, 30, 0), 8.0);
  const TopoDS_Face  aBottom  = faceNearest(aBase, gp_Pnt(50, 30, 0));
  const TopoDS_Face  aSlanted = faceNearest(aBase, gp_Pnt(50, 30, 30));
  // an infinite plane and a face of the base are not of the same kind
  const TopoDS_Face anInfinite =
    BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0.0, 0.0, 5.0), gp::DZ())).Face();

  BRepFeat_MakePrism aPrism(aBase, aProfile, aBottom, gp::DZ(), 0, true);
  aPrism.Perform(anInfinite, aSlanted);
  EXPECT_FALSE(aPrism.IsDone());
  EXPECT_EQ(aPrism.CurrentStatusError(), BRepFeat_IncTypes);

  BRepFeat_MakeRevol aRevol(aBase,
                            aProfile,
                            aBottom,
                            gp_Ax1(gp_Pnt(80.0, 30.0, 0.0), gp::DY()),
                            0,
                            true);
  aRevol.Perform(anInfinite, aSlanted);
  EXPECT_FALSE(aRevol.IsDone());
  EXPECT_EQ(aRevol.CurrentStatusError(), BRepFeat_IncTypes);
}
