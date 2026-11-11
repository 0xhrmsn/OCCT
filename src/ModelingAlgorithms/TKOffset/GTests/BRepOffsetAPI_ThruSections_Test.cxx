// Copyright (c) 2026 OPEN CASCADE SAS
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

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFill_ThruSectionErrorStatus.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <gp.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_HArray1.hxx>
#include <NCollection_List.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
//==================================================================================================

TopoDS_Wire makeCapsuleWire(const double theX)
{
  const gp_Circ           aLeft(gp_Ax2(gp_Pnt(theX, 0.0, 0.0), gp::DZ(), gp::DX()), 5.0);
  const gp_Circ           aRight(gp_Ax2(gp_Pnt(theX + 10.0, 0.0, 0.0), gp::DZ(), gp::DX()), 5.0);
  BRepBuilderAPI_MakeWire aWire;
  aWire.Add(BRepBuilderAPI_MakeEdge(aLeft, 0.5 * M_PI, 1.5 * M_PI).Edge());
  aWire.Add(
    BRepBuilderAPI_MakeEdge(gp_Pnt(theX, -5.0, 0.0), gp_Pnt(theX + 10.0, -5.0, 0.0)).Edge());
  aWire.Add(BRepBuilderAPI_MakeEdge(aRight, 1.5 * M_PI, 2.5 * M_PI).Edge());
  aWire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(theX + 10.0, 5.0, 0.0), gp_Pnt(theX, 5.0, 0.0)).Edge());
  return aWire.Wire();
}
} // namespace

TEST(BRepOffsetAPI_ThruSectionsTest, RepeatedClosedSectionPreservesGeneratedFaces)
{
  const TopoDS_Wire aFirst = makeCapsuleWire(0.0);
  const gp_Circ     aCircle(gp_Ax2(gp_Pnt(50.0, 0.0, 15.0), gp::DX(), gp_Dir(0.0, 0.0, -1.0)), 3.0);
  const TopoDS_Wire aSecond =
    BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(aCircle).Edge()).Wire();

  BRepOffsetAPI_ThruSections aLoft;
  aLoft.SetMaxDegree(5);
  aLoft.AddWire(aFirst);
  aLoft.AddWire(aSecond);
  aLoft.AddWire(makeCapsuleWire(90.0));
  aLoft.AddWire(aFirst);
  aLoft.Build();

  ASSERT_TRUE(aLoft.IsDone());
  ASSERT_FALSE(aLoft.Shape().IsNull());
  EXPECT_TRUE(BRepCheck_Analyzer(aLoft.Shape()).IsValid());
  for (TopExp_Explorer anExp(aFirst, TopAbs_EDGE); anExp.More(); anExp.Next())
  {
    const NCollection_List<TopoDS_Shape>& aGenerated = aLoft.Generated(anExp.Current());
    ASSERT_FALSE(aGenerated.IsEmpty());
    for (const TopoDS_Shape& aFace : aGenerated)
    {
      ASSERT_FALSE(aFace.IsNull());
      EXPECT_EQ(aFace.ShapeType(), TopAbs_FACE);
    }
  }
}

TEST(BRepOffsetAPI_ThruSectionsTest, UnboundedSectionReturnsFailure)
{
  const TopoDS_Wire aFirst =
    BRepBuilderAPI_MakeWire(
      BRepBuilderAPI_MakeEdge(gp_Pnt(0.0, 0.0, 0.0), gp_Pnt(10.0, 0.0, 0.0)).Edge())
      .Wire();
  const gp_Lin      aLine(gp_Pnt(0.0, 0.0, 10.0), gp::DX());
  const TopoDS_Edge aFullLine = BRepBuilderAPI_MakeEdge(aLine).Edge();
  const TopoDS_Edge aHalfLine = BRepBuilderAPI_MakeEdge(aLine, 0.0, Precision::Infinite()).Edge();
  for (const TopoDS_Edge& anEdge : std::array<TopoDS_Edge, 2>{aFullLine, aHalfLine})
  {
    const TopoDS_Wire anUnbounded = BRepBuilderAPI_MakeWire(anEdge).Wire();
    for (const bool toCheckCompatibility : std::array<bool, 2>{false, true})
    {
      SCOPED_TRACE(toCheckCompatibility);
      BRepOffsetAPI_ThruSections aLoft;
      aLoft.CheckCompatibility(toCheckCompatibility);
      aLoft.AddWire(aFirst);
      aLoft.AddWire(anUnbounded);
      aLoft.Build();

      EXPECT_FALSE(aLoft.IsDone());
      EXPECT_EQ(aLoft.GetStatus(), BRepFill_ThruSectionErrorStatus_ProfilesInconsistent);
    }
  }
}

TEST(BRepOffsetAPI_ThruSectionsTest, BoundedOpenSectionsBuildFace)
{
  const TopoDS_Edge aFirstEdge =
    BRepBuilderAPI_MakeEdge(gp_Pnt(0.0, 0.0, 0.0), gp_Pnt(10.0, 0.0, 0.0)).Edge();
  const TopoDS_Edge aLastEdge =
    BRepBuilderAPI_MakeEdge(gp_Pnt(0.0, 0.0, 10.0), gp_Pnt(10.0, 0.0, 10.0)).Edge();
  const TopoDS_Wire aFirst = BRepBuilderAPI_MakeWire(aFirstEdge).Wire();
  const TopoDS_Wire aLast  = BRepBuilderAPI_MakeWire(aLastEdge).Wire();
  for (const bool toCheckCompatibility : std::array<bool, 2>{false, true})
  {
    SCOPED_TRACE(toCheckCompatibility);
    BRepOffsetAPI_ThruSections aLoft;
    aLoft.CheckCompatibility(toCheckCompatibility);
    aLoft.AddWire(aFirst);
    aLoft.AddWire(aLast);
    aLoft.Build();

    ASSERT_TRUE(aLoft.IsDone());
    ASSERT_FALSE(aLoft.Shape().IsNull());
    EXPECT_TRUE(BRepCheck_Analyzer(aLoft.Shape()).IsValid());
    TopExp_Explorer anExp(aLoft.Shape(), TopAbs_FACE);
    ASSERT_TRUE(anExp.More());
    anExp.Next();
    EXPECT_FALSE(anExp.More());
  }
}

namespace
{
//! Closed BSpline wire interpolating an involute gear profile of theNbTeeth teeth of module 1,
//! 12 points per flank.
TopoDS_Wire gearWire(const int theNbTeeth)
{
  const double anAlpha = 20.0 * M_PI / 180.0;
  const double aPitch  = theNbTeeth / 2.0;
  const double aBase   = aPitch * std::cos(anAlpha);
  const double aTip    = aPitch + 1.0;
  const double aRoot   = std::max(aPitch - 1.25, aBase * 0.98);
  const double aTTip   = std::sqrt((aTip / aBase) * (aTip / aBase) - 1.0);
  const double aTStart = aRoot > aBase ? std::sqrt((aRoot / aBase) * (aRoot / aBase) - 1.0) : 0.0;
  const double aHalf   = M_PI / (2.0 * theNbTeeth) + std::tan(anAlpha) - anAlpha;
  std::vector<gp_Pnt> aPoints;
  for (int aTooth = 0; aTooth < theNbTeeth; ++aTooth)
  {
    const double aCenter = 2.0 * M_PI * aTooth / theNbTeeth;
    for (int aSide = -1; aSide <= 1; aSide += 2)
    {
      for (int i = 0; i <= 12; ++i)
      {
        const double aT  = aTStart + (aTTip - aTStart) * (aSide < 0 ? i : 12 - i) / 12.0;
        const double aR  = aBase * std::sqrt(1.0 + aT * aT);
        const double anA = aCenter + aSide * (aHalf - (aT - std::atan(aT)));
        aPoints.emplace_back(aR * std::cos(anA), aR * std::sin(anA), 0.0);
      }
    }
  }
  occ::handle<NCollection_HArray1<gp_Pnt>> aPnts =
    new NCollection_HArray1<gp_Pnt>(1, static_cast<int>(aPoints.size()));
  for (size_t i = 0; i < aPoints.size(); ++i)
  {
    aPnts->SetValue(static_cast<int>(i + 1), aPoints[i]);
  }
  GeomAPI_Interpolate anInterpolate(aPnts, true, 1.0e-9);
  anInterpolate.Perform();
  return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(anInterpolate.Curve()).Edge()).Wire();
}
} // namespace

// A loft through 5 sections of a 100-tooth gear profile, each turned by 7.5 degrees, was a valid
// solid turned inside out (volume -A*H): the classification of the infinite point that oriented
// it is not reliable on such a face. The volume of the twisted extrusion is A*H.
TEST(BRepOffsetAPI_ThruSectionsTest, TwistedGearLoft_OrientedOutwards)
{
  const TopoDS_Wire aWire = gearWire(100);
  GProp_GProps      aProfile;
  BRepGProp::SurfaceProperties(BRepBuilderAPI_MakeFace(aWire, true).Face(), aProfile);
  const double aHeight = 10.0;
  for (const double aTwist : {10.0, 30.0})
  {
    BRepOffsetAPI_ThruSections aLoft(true, false, 1.0e-6);
    for (int i = 0; i <= 4; ++i)
    {
      gp_Trsf aTrsf;
      aTrsf.SetRotation(gp_Ax1(gp::Origin(), gp::DZ()), aTwist * M_PI / 180.0 * i / 4.0);
      aTrsf.SetTranslationPart(gp_Vec(0.0, 0.0, aHeight * i / 4.0));
      aLoft.AddWire(TopoDS::Wire(BRepBuilderAPI_Transform(aWire, aTrsf, true).Shape()));
    }
    aLoft.Build();
    ASSERT_TRUE(aLoft.IsDone()) << aTwist;
    EXPECT_TRUE(BRepCheck_Analyzer(aLoft.Shape()).IsValid()) << aTwist;
    GProp_GProps aVolume;
    BRepGProp::VolumeProperties(aLoft.Shape(), aVolume);
    EXPECT_NEAR(aVolume.Mass(), aProfile.Mass() * aHeight, 1.0e-6 * aProfile.Mass() * aHeight)
      << aTwist;
  }
}
