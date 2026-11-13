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

#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFeat_MakePipe.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>

#include <gtest/gtest.h>

#include <cmath>

namespace
{
// Face of theBox whose vertices all lie on the plane Z = theZ.
TopoDS_Face faceAtZ(const TopoDS_Shape& theBox, const double theZ)
{
  for (TopExp_Explorer anExp(theBox, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    bool isOnPlane = true;
    for (TopExp_Explorer aVExp(anExp.Current(), TopAbs_VERTEX); aVExp.More() && isOnPlane;
         aVExp.Next())
    {
      isOnPlane = std::abs(BRep_Tool::Pnt(TopoDS::Vertex(aVExp.Current())).Z() - theZ) < 1.0e-7;
    }
    if (isOnPlane)
    {
      return TopoDS::Face(anExp.Current());
    }
  }
  return TopoDS_Face();
}
} // namespace

// A pocket made "until" a face along a straight spine: the guide lines of the pipe are
// straight edges whose curve is an infinite line, and converting it to a B-spline raised
// Standard_DomainError ("No such curve").
TEST(BRepFeat_MakePipeTest, PocketUntilFace_StraightSpine)
{
  const TopoDS_Shape aBox    = BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), 100.0, 60.0, 20.0).Shape();
  const TopoDS_Face  aTop    = faceAtZ(aBox, 20.0);
  const TopoDS_Face  aBottom = faceAtZ(aBox, 0.0);
  ASSERT_FALSE(aTop.IsNull());
  ASSERT_FALSE(aBottom.IsNull());

  const double      aRadius = 5.0;
  const TopoDS_Face aProfile =
    BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(
                                                      gp_Ax2(gp_Pnt(50, 30, 20), gp::DZ()),
                                                      aRadius)))
                              .Wire(),
                            true);
  const TopoDS_Wire aSpine =
    BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Pnt(50, 30, 20), gp_Pnt(50, 30, -10)));

  BRepFeat_MakePipe aPipe;
  aPipe.Init(aBox, aProfile, aTop, aSpine, 0, true);
  ASSERT_NO_THROW(aPipe.Perform(aBottom));
  ASSERT_TRUE(aPipe.IsDone());
  EXPECT_TRUE(BRepCheck_Analyzer(aPipe.Shape()).IsValid());

  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aPipe.Shape(), aProps);
  const double anExpected = 100.0 * 60.0 * 20.0 - M_PI * aRadius * aRadius * 20.0;
  EXPECT_NEAR(aProps.Mass(), anExpected, 1.0e-6 * anExpected);
}
