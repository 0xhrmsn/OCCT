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
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepLib_CheckCurveOnSurface.hxx>
#include <BRepOffsetAPI_NormalProjection.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>

#include <gtest/gtest.h>

// The tolerance of a projected edge was the approximation error at its fitting parameters only.
// An L-shaped polyline projected onto a sphere gave 6 edges whose curve on the sphere deviated
// from the 3D curve by more than the edge tolerance.
TEST(BRepOffsetAPI_NormalProjectionTest, PolylineOnSphere_ToleranceCoversDeviation)
{
  const TopoDS_Shape         aSphere = BRepPrimAPI_MakeSphere(gp_Pnt(0.0, 0.0, 0.0), 25.0).Shape();
  BRepBuilderAPI_MakePolygon aLetterL;
  aLetterL.Add(gp_Pnt(-3.0, -5.0, 50.0));
  aLetterL.Add(gp_Pnt(-3.0, 5.0, 50.0));
  aLetterL.Add(gp_Pnt(-1.0, 5.0, 50.0));
  aLetterL.Add(gp_Pnt(-1.0, -3.0, 50.0));
  aLetterL.Add(gp_Pnt(4.0, -3.0, 50.0));
  aLetterL.Add(gp_Pnt(4.0, -5.0, 50.0));
  aLetterL.Close();

  BRepOffsetAPI_NormalProjection aProj(aSphere);
  aProj.Add(aLetterL.Wire());
  aProj.Build();
  ASSERT_TRUE(aProj.IsDone());

  int aNbChecked = 0;
  for (TopExp_Explorer anEdgeExp(aProj.Shape(), TopAbs_EDGE); anEdgeExp.More(); anEdgeExp.Next())
  {
    const TopoDS_Edge& anEdge = TopoDS::Edge(anEdgeExp.Current());
    for (TopExp_Explorer aFaceExp(aSphere, TopAbs_FACE); aFaceExp.More(); aFaceExp.Next())
    {
      const TopoDS_Face& aFace = TopoDS::Face(aFaceExp.Current());
      double             aFirst = 0.0, aLast = 0.0;
      bool               isStored = false;
      if (BRep_Tool::CurveOnSurface(anEdge, aFace, aFirst, aLast, &isStored).IsNull() || !isStored)
      {
        continue;
      }
      BRepLib_CheckCurveOnSurface aCheck(anEdge, aFace);
      aCheck.Perform();
      ASSERT_TRUE(aCheck.IsDone());
      EXPECT_LE(aCheck.MaxDistance(), BRep_Tool::Tolerance(anEdge) * (1.0 + 1.0e-9))
        << "edge " << aNbChecked;
      ++aNbChecked;
    }
  }
  EXPECT_GE(aNbChecked, 6);
}
