// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <BRepOffsetAPI_MakeOffset.hxx>

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <Bnd_Box.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>

#include <gtest/gtest.h>

namespace
{
//! Face bounded by a circle of radius 10 centred at the origin in XY, moved by theMove.
TopoDS_Face circleFace(const gp_Trsf& theMove)
{
  const gp_Circ     aCirc(gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)), 10.0);
  const TopoDS_Wire aWire = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(aCirc)).Wire();
  TopoDS_Face       aFace = BRepBuilderAPI_MakeFace(aWire).Face();
  aFace.Move(TopLoc_Location(theMove));
  return aFace;
}

//! Checks that the offset of circleFace(theMove) by 2 is a circle of radius 12 around the moved
//! centre.
void checkOffsetCircle(const gp_Trsf& theMove)
{
  BRepOffsetAPI_MakeOffset anOffset(circleFace(theMove), GeomAbs_Arc);
  anOffset.Perform(2.0);
  ASSERT_TRUE(anOffset.IsDone());

  Bnd_Box aBox;
  BRepBndLib::AddOptimal(anOffset.Shape(), aBox, false, false);
  double aXmin, aYmin, aZmin, aXmax, aYmax, aZmax;
  aBox.Get(aXmin, aYmin, aZmin, aXmax, aYmax, aZmax);
  const gp_Pnt aCentre = gp_Pnt(0.0, 0.0, 0.0).Transformed(theMove);
  EXPECT_NEAR(0.5 * (aXmin + aXmax), aCentre.X(), 1.0e-6);
  EXPECT_NEAR(0.5 * (aYmin + aYmax), aCentre.Y(), 1.0e-6);
  EXPECT_NEAR(0.5 * (aZmin + aZmax), aCentre.Z(), 1.0e-6);
  EXPECT_NEAR(aXmax - aXmin, 24.0, 1.0e-6);
}
} // namespace

TEST(BRepOffsetAPI_MakeOffsetTest, Circle_NotMoved)
{
  checkOffsetCircle(gp_Trsf());
}

// The offset of a single circle (KPartCircle) applied the face location twice.
TEST(BRepOffsetAPI_MakeOffsetTest, Circle_MovedFace_PlacedOnce)
{
  gp_Trsf aMove;
  aMove.SetTranslation(gp_Vec(30.0, 20.0, 5.0));
  checkOffsetCircle(aMove);

  gp_Trsf aRotMove;
  aRotMove.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)), 0.5);
  aRotMove.SetTranslationPart(gp_Vec(30.0, 20.0, 5.0));
  checkOffsetCircle(aRotMove);
}