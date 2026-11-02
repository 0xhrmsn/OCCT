// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <BRepExtrema_ShapeProximity.hxx>

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Pnt.hxx>

#include <gtest/gtest.h>

namespace
{
TopoDS_Shape meshedBox(const gp_Pnt& theCorner)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(theCorner, 10.0, 10.0, 10.0).Shape();
  BRepMesh_IncrementalMesh(aBox, 0.1);
  return aBox;
}
} // namespace

// The overlap tool kept the face pairs of the previous Perform() when the proximity object was
// reused with another shape, so a distant shape was still reported as overlapping.
TEST(BRepExtrema_ShapeProximityTest, Reuse_PreviousOverlapsCleared)
{
  BRepExtrema_ShapeProximity aProximity(0.0);
  ASSERT_TRUE(aProximity.LoadShape1(meshedBox(gp_Pnt(0.0, 0.0, 0.0))));
  ASSERT_TRUE(aProximity.LoadShape2(meshedBox(gp_Pnt(5.0, 5.0, 5.0))));
  aProximity.Perform();
  ASSERT_TRUE(aProximity.IsDone());
  ASSERT_FALSE(aProximity.OverlapSubShapes1().IsEmpty());

  ASSERT_TRUE(aProximity.LoadShape2(meshedBox(gp_Pnt(100.0, 0.0, 0.0))));
  aProximity.Perform();
  EXPECT_TRUE(aProximity.OverlapSubShapes1().IsEmpty());
  EXPECT_TRUE(aProximity.OverlapSubShapes2().IsEmpty());
}