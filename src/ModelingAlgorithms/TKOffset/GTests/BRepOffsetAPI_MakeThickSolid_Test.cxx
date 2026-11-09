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

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_List.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>

#include <gtest/gtest.h>

TEST(BRepOffsetAPI_MakeThickSolidTest, HollowBox)
{
  BRepPrimAPI_MakeBox aBoxMaker(20.0, 20.0, 20.0);
  const TopoDS_Shape& aBox = aBoxMaker.Shape();
  ASSERT_TRUE(aBoxMaker.IsDone());

  // Get the first face to remove.
  TopExp_Explorer anExp(aBox, TopAbs_FACE);
  ASSERT_TRUE(anExp.More());
  TopoDS_Face aFaceToRemove = TopoDS::Face(anExp.Current());

  NCollection_List<TopoDS_Shape> aFacesToRemove;
  aFacesToRemove.Append(aFaceToRemove);

  BRepOffsetAPI_MakeThickSolid aMaker;
  aMaker.MakeThickSolidByJoin(aBox, aFacesToRemove, -1.0, Precision::Confusion());
  aMaker.Build();

  ASSERT_TRUE(aMaker.IsDone());

  const TopoDS_Shape& aResult = aMaker.Shape();
  EXPECT_FALSE(aResult.IsNull());

  BRepCheck_Analyzer aChecker(aResult);
  EXPECT_TRUE(aChecker.IsValid());
}

TEST(BRepOffsetAPI_MakeThickSolidTest, HollowBoxVolume)
{
  BRepPrimAPI_MakeBox aBoxMaker(20.0, 20.0, 20.0);
  const TopoDS_Shape& aBox = aBoxMaker.Shape();
  ASSERT_TRUE(aBoxMaker.IsDone());

  GProp_GProps aOrigProps;
  BRepGProp::VolumeProperties(aBox, aOrigProps);
  const double anOrigVolume = aOrigProps.Mass();

  TopExp_Explorer anExp(aBox, TopAbs_FACE);
  ASSERT_TRUE(anExp.More());
  TopoDS_Face aFaceToRemove = TopoDS::Face(anExp.Current());

  NCollection_List<TopoDS_Shape> aFacesToRemove;
  aFacesToRemove.Append(aFaceToRemove);

  BRepOffsetAPI_MakeThickSolid aMaker;
  aMaker.MakeThickSolidByJoin(aBox, aFacesToRemove, -1.0, Precision::Confusion());
  aMaker.Build();

  ASSERT_TRUE(aMaker.IsDone());

  const TopoDS_Shape& aResult = aMaker.Shape();
  ASSERT_FALSE(aResult.IsNull());

  GProp_GProps aHollowProps;
  BRepGProp::VolumeProperties(aResult, aHollowProps);
  const double aHollowVolume = aHollowProps.Mass();

  // Hollowed box must have less volume than the original solid.
  EXPECT_LT(aHollowVolume, anOrigVolume);
}

TEST(BRepOffsetAPI_MakeThickSolidTest, MakeThickSolidBySimple)
{
  BRepPrimAPI_MakeBox aBoxMaker(10.0, 10.0, 10.0);
  const TopoDS_Shape& aBox = aBoxMaker.Shape();
  ASSERT_TRUE(aBoxMaker.IsDone());

  // Extract a single face (non-closed shell) for MakeThickSolidBySimple.
  TopExp_Explorer anExp(aBox, TopAbs_FACE);
  ASSERT_TRUE(anExp.More());
  TopoDS_Face aFace = TopoDS::Face(anExp.Current());

  BRepOffsetAPI_MakeThickSolid aMaker;
  aMaker.MakeThickSolidBySimple(aFace, 2.0);
  aMaker.Build();

  ASSERT_TRUE(aMaker.IsDone());

  const TopoDS_Shape& aResult = aMaker.Shape();
  EXPECT_FALSE(aResult.IsNull());

  BRepCheck_Analyzer aChecker(aResult);
  EXPECT_TRUE(aChecker.IsValid());
}

TEST(BRepOffsetAPI_MakeThickSolidTest, ThickSolidLargerVolume)
{
  BRepPrimAPI_MakeBox aBoxMaker(10.0, 10.0, 10.0);
  const TopoDS_Shape& aBox = aBoxMaker.Shape();
  ASSERT_TRUE(aBoxMaker.IsDone());

  // Extract a single face for MakeThickSolidBySimple.
  TopExp_Explorer anExp(aBox, TopAbs_FACE);
  ASSERT_TRUE(anExp.More());
  TopoDS_Face aFace = TopoDS::Face(anExp.Current());

  // Compute the area of the original face to estimate expected volume.
  GProp_GProps aFaceProps;
  BRepGProp::SurfaceProperties(aFace, aFaceProps);
  const double aFaceArea = aFaceProps.Mass();

  const double                 anOffset = 2.0;
  BRepOffsetAPI_MakeThickSolid aMaker;
  aMaker.MakeThickSolidBySimple(aFace, anOffset);
  aMaker.Build();

  ASSERT_TRUE(aMaker.IsDone());

  const TopoDS_Shape& aResult = aMaker.Shape();
  ASSERT_FALSE(aResult.IsNull());

  GProp_GProps aResultProps;
  BRepGProp::VolumeProperties(aResult, aResultProps);
  const double aResultVolume = std::abs(aResultProps.Mass());

  // The thickened face should produce a solid with volume > 0.
  // A planar face of area A thickened by offset d produces volume approximately A*d.
  EXPECT_GT(aResultVolume, aFaceArea * anOffset * 0.5);
}

namespace
{
double volumeOf(const TopoDS_Shape& theShape)
{
  GProp_GProps aProps;
  BRepGProp::VolumeProperties(theShape, aProps);
  return aProps.Mass();
}

// Box 100 x 60 x 30 with every edge filleted with theRadius.
TopoDS_Shape filletedBox(const double theRadius)
{
  const TopoDS_Shape       aBox = BRepPrimAPI_MakeBox(100.0, 60.0, 30.0).Shape();
  BRepFilletAPI_MakeFillet aFillet(aBox);
  for (TopExp_Explorer anExp(aBox, TopAbs_EDGE); anExp.More(); anExp.Next())
  {
    aFillet.Add(theRadius, TopoDS::Edge(anExp.Current()));
  }
  aFillet.Build();
  return aFillet.Shape();
}

// Faces whose bounding box reaches above theZ: with theWhole, the whole box must lie above theZ.
NCollection_List<TopoDS_Shape> facesAbove(const TopoDS_Shape& theShape,
                                          const double        theZ,
                                          const bool          theWhole = true)
{
  NCollection_List<TopoDS_Shape> aFaces;
  for (TopExp_Explorer anExp(theShape, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    Bnd_Box aBox;
    BRepBndLib::Add(anExp.Current(), aBox);
    if ((theWhole ? aBox.CornerMin().Z() : aBox.CornerMax().Z()) > theZ)
    {
      aFaces.Append(anExp.Current());
    }
  }
  return aFaces;
}
} // namespace

// Shelling a filleted box inwards: the offset of the side faces is removed by the unwinding, only
// the trimmed top cap remains, and the result used to be the initial solid reported as done.
// Required: either a failure, or a result that really removes material.
TEST(BRepOffsetAPI_MakeThickSolidTest, ShellWithoutOffsetFaces_ReportsError)
{
  const TopoDS_Shape                   aShape = filletedBox(2.0);
  const NCollection_List<TopoDS_Shape> aTop   = facesAbove(aShape, 29.9);
  ASSERT_EQ(aTop.Extent(), 1);

  const struct
  {
    GeomAbs_JoinType Join;
    double           Thickness;
  } aCases[] = {{GeomAbs_Arc, 2.0}, {GeomAbs_Intersection, 1.2}, {GeomAbs_Intersection, 2.0}};

  for (const auto& aCase : aCases)
  {
    BRepOffsetAPI_MakeThickSolid aMaker;
    aMaker.MakeThickSolidByJoin(aShape,
                                aTop,
                                -aCase.Thickness,
                                1.0e-4,
                                BRepOffset_Skin,
                                aCase.Join == GeomAbs_Intersection,
                                false,
                                aCase.Join);
    if (aMaker.IsDone())
    {
      const double anInitial = volumeOf(aShape);
      EXPECT_LT(std::abs(volumeOf(aMaker.Shape())), anInitial * (1.0 - 1.0e-6))
        << "join " << aCase.Join << ", thickness " << aCase.Thickness
        << ": done without removing material, initial volume " << anInitial;
    }
    else
    {
      EXPECT_EQ(aMaker.MakeOffset().Error(), BRepOffset_UnknownError);
    }
  }
}

// Guard: a box with its top removed, joined by intersection (planar faces, the extended faces
// path) and by arcs outwards (faces built on edges and vertices).
TEST(BRepOffsetAPI_MakeThickSolidTest, BoxWithTopRemoved_IntersectionAndArc)
{
  const TopoDS_Shape                   aBox = BRepPrimAPI_MakeBox(100.0, 60.0, 30.0).Shape();
  const NCollection_List<TopoDS_Shape> aTop = facesAbove(aBox, 29.9);
  ASSERT_EQ(aTop.Extent(), 1);

  const struct
  {
    GeomAbs_JoinType Join;
    double           Offset;
    double           Volume;
  } aCases[] = {{GeomAbs_Intersection, -2.0, 100.0 * 60.0 * 30.0 - 96.0 * 56.0 * 28.0},
                {GeomAbs_Intersection, 2.0, 104.0 * 64.0 * 32.0 - 100.0 * 60.0 * 30.0},
                // Rounded outside: smaller than the sharp one.
                {GeomAbs_Arc, 2.0, 32599.055928}};

  for (const auto& aCase : aCases)
  {
    BRepOffsetAPI_MakeThickSolid aMaker;
    aMaker.MakeThickSolidByJoin(aBox,
                                aTop,
                                aCase.Offset,
                                1.0e-4,
                                BRepOffset_Skin,
                                aCase.Join == GeomAbs_Intersection,
                                false,
                                aCase.Join);
    ASSERT_TRUE(aMaker.IsDone()) << "join " << aCase.Join << ", offset " << aCase.Offset;
    EXPECT_TRUE(BRepCheck_Analyzer(aMaker.Shape()).IsValid());
    EXPECT_NEAR(std::abs(volumeOf(aMaker.Shape())), aCase.Volume, 1.0e-5);
  }
}

// Guard: a thick solid whose result has as many faces as the initial solid (one face kept,
// five removed) is a slab, not a missing offset.
TEST(BRepOffsetAPI_MakeThickSolidTest, BoxWithFiveFacesRemoved_GivesSlab)
{
  const TopoDS_Shape                   aBox  = BRepPrimAPI_MakeBox(100.0, 60.0, 30.0).Shape();
  const NCollection_List<TopoDS_Shape> aFive = facesAbove(aBox, 1.0e-3, false);
  ASSERT_EQ(aFive.Extent(), 5);

  for (const GeomAbs_JoinType aJoin : {GeomAbs_Arc, GeomAbs_Intersection})
  {
    for (const double anOffset : {-2.0, 2.0})
    {
      BRepOffsetAPI_MakeThickSolid aMaker;
      aMaker.MakeThickSolidByJoin(aBox,
                                  aFive,
                                  anOffset,
                                  1.0e-4,
                                  BRepOffset_Skin,
                                  aJoin == GeomAbs_Intersection,
                                  false,
                                  aJoin);
      ASSERT_TRUE(aMaker.IsDone()) << "join " << aJoin << ", offset " << anOffset;
      EXPECT_TRUE(BRepCheck_Analyzer(aMaker.Shape()).IsValid());
      EXPECT_NEAR(std::abs(volumeOf(aMaker.Shape())), 100.0 * 60.0 * 2.0, 1.0e-6);
    }
  }
}
