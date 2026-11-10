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

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pln.hxx>

#include <gtest/gtest.h>

namespace
{
//! Face of theShape whose centre of mass is theCentre.
TopoDS_Face faceAt(const TopoDS_Shape& theShape, const gp_Pnt& theCentre)
{
  for (TopExp_Explorer anExp(theShape, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    GProp_GProps aProps;
    BRepGProp::SurfaceProperties(anExp.Current(), aProps);
    if (aProps.CentreOfMass().Distance(theCentre) < 1e-7)
    {
      return TopoDS::Face(anExp.Current());
    }
  }
  return TopoDS_Face();
}

double volumeOf(const TopoDS_Shape& theShape)
{
  GProp_GProps aProps;
  BRepGProp::VolumeProperties(theShape, aProps);
  return aProps.Mass();
}
} // namespace

// The header of Remove() tells to call it when the previous Add() fails. The failed face was not
// recorded, Remove() threw Standard_NoSuchObject and every later Add() threw
// Standard_ConstructionError.
TEST(BRepOffsetAPI_DraftAngleTest, RemoveAfterFailedAdd)
{
  const TopoDS_Shape aBox   = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  const TopoDS_Face  aTop   = faceAt(aBox, gp_Pnt(5.0, 10.0, 30.0));
  const TopoDS_Face  aFront = faceAt(aBox, gp_Pnt(5.0, 0.0, 15.0));
  ASSERT_FALSE(aTop.IsNull());
  ASSERT_FALSE(aFront.IsNull());
  const gp_Pln aNeutral(gp::Origin(), gp::DZ());
  const double anAngle = 5.0 * M_PI / 180.0;

  BRepOffsetAPI_DraftAngle aReference(aBox);
  aReference.Add(aFront, gp::DZ(), anAngle, aNeutral);
  aReference.Build();
  ASSERT_TRUE(aReference.IsDone());

  // the top face is normal to the direction of the draft: no draft can be computed
  BRepOffsetAPI_DraftAngle aDraft(aBox);
  aDraft.Add(aTop, gp::DZ(), anAngle, aNeutral);
  ASSERT_FALSE(aDraft.AddDone());
  EXPECT_NO_THROW(aDraft.Remove(aTop));
  EXPECT_TRUE(aDraft.AddDone());
  aDraft.Add(aFront, gp::DZ(), anAngle, aNeutral);
  EXPECT_TRUE(aDraft.AddDone());
  aDraft.Build();
  ASSERT_TRUE(aDraft.IsDone());
  EXPECT_TRUE(BRepCheck_Analyzer(aDraft.Shape()).IsValid());
  EXPECT_NEAR(volumeOf(aDraft.Shape()), volumeOf(aReference.Shape()), 1e-9);
}

// The way out of a failed Add() works also after removing another face first.
TEST(BRepOffsetAPI_DraftAngleTest, RemoveAfterFailedAdd_OtherFaceRemovedFirst)
{
  const TopoDS_Shape aBox   = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  const TopoDS_Face  aTop   = faceAt(aBox, gp_Pnt(5.0, 10.0, 30.0));
  const TopoDS_Face  aFront = faceAt(aBox, gp_Pnt(5.0, 0.0, 15.0));
  const TopoDS_Face  aBack  = faceAt(aBox, gp_Pnt(5.0, 20.0, 15.0));
  ASSERT_FALSE(aTop.IsNull() || aFront.IsNull() || aBack.IsNull());
  const gp_Pln aNeutral(gp::Origin(), gp::DZ());
  const double anAngle = 5.0 * M_PI / 180.0;

  BRepOffsetAPI_DraftAngle aReference(aBox);
  aReference.Add(aBack, gp::DZ(), anAngle, aNeutral);
  aReference.Build();
  ASSERT_TRUE(aReference.IsDone());

  BRepOffsetAPI_DraftAngle aDraft(aBox);
  aDraft.Add(aFront, gp::DZ(), anAngle, aNeutral);
  ASSERT_TRUE(aDraft.AddDone());
  aDraft.Add(aTop, gp::DZ(), anAngle, aNeutral);
  ASSERT_FALSE(aDraft.AddDone());
  EXPECT_NO_THROW(aDraft.Remove(aFront));
  EXPECT_NO_THROW(aDraft.Remove(aTop));
  EXPECT_TRUE(aDraft.AddDone());
  aDraft.Add(aBack, gp::DZ(), anAngle, aNeutral);
  aDraft.Build();
  ASSERT_TRUE(aDraft.IsDone());
  EXPECT_NEAR(volumeOf(aDraft.Shape()), volumeOf(aReference.Shape()), 1e-9);
}
