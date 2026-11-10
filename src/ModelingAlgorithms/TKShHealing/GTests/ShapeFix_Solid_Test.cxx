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
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <ShapeExtend.hxx>
#include <ShapeFix_Solid.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>

#include <gtest/gtest.h>

namespace
{
//! Shell of the first theNbFaces faces of a box 10 x 20 x 30.
TopoDS_Shell boxShell(const int theNbFaces)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  BRep_Builder       aBuilder;
  TopoDS_Shell       aShell;
  aBuilder.MakeShell(aShell);
  int anIndex = 0;
  for (TopExp_Explorer anExp(aBox, TopAbs_FACE); anExp.More() && anIndex < theNbFaces;
       anExp.Next(), ++anIndex)
  {
    aBuilder.Add(aShell, anExp.Current());
  }
  return aShell;
}
} // namespace

// A solid was built on an open shell without any status: it was invalid and bounded no volume.
TEST(ShapeFix_SolidTest, SolidFromShell_OpenShell)
{
  ShapeFix_Solid anOpen;
  EXPECT_TRUE(anOpen.SolidFromShell(boxShell(5)).IsNull());
  EXPECT_TRUE(anOpen.Status(ShapeExtend_FAIL));

  // an open solid is still built on request
  ShapeFix_Solid anOpenMode;
  anOpenMode.CreateOpenSolidMode() = true;
  EXPECT_FALSE(anOpenMode.SolidFromShell(boxShell(5)).IsNull());
  EXPECT_FALSE(anOpenMode.Status(ShapeExtend_FAIL));

  // guard: a closed shell gives a valid solid
  ShapeFix_Solid     aClosed;
  const TopoDS_Solid aSolid = aClosed.SolidFromShell(boxShell(6));
  ASSERT_FALSE(aSolid.IsNull());
  EXPECT_FALSE(aClosed.Status(ShapeExtend_FAIL));
  EXPECT_TRUE(BRepCheck_Analyzer(aSolid).IsValid());
  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aSolid, aProps);
  EXPECT_NEAR(aProps.Mass(), 6000.0, 1e-6);
}
