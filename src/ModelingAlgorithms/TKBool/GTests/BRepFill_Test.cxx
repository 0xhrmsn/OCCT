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
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepFill.hxx>
#include <Geom2d_Line.hxx>
#include <Geom_Plane.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt2d.hxx>

#include <gtest/gtest.h>

namespace
{
//! Edge of length 10 along X on the plane z = theZ, with a pcurve only (no 3D curve).
TopoDS_Edge pcurveOnlyEdge(const double theZ)
{
  const occ::handle<Geom_Plane> aPlane =
    new Geom_Plane(gp_Pln(gp_Pnt(0.0, 0.0, theZ), gp_Dir(0.0, 0.0, 1.0)));
  const occ::handle<Geom2d_Line> aLine = new Geom2d_Line(gp_Pnt2d(0.0, 0.0), gp_Dir2d(1.0, 0.0));
  return BRepBuilderAPI_MakeEdge(aLine, aPlane, 0.0, 10.0).Edge();
}
} // namespace

// An edge with pcurves only is valid B-rep (e.g. read from a file), but Face() and Shell()
// dereferenced its null 3D curve (access violation). They now return a null shape, which the
// IGES ruled surface translator already reports as a construction error.
TEST(BRepFill_Test, Face_EdgeWithoutCurve3d_ReturnsNull)
{
  const TopoDS_Edge anEdge1 = pcurveOnlyEdge(0.0);
  const TopoDS_Edge anEdge2 = pcurveOnlyEdge(5.0);
  double            aFirst = 0.0, aLast = 0.0;
  ASSERT_TRUE(BRep_Tool::Curve(anEdge1, aFirst, aLast).IsNull());
  EXPECT_TRUE(BRepFill::Face(anEdge1, anEdge2).IsNull());
}

TEST(BRepFill_Test, Shell_EdgeWithoutCurve3d_ReturnsNull)
{
  const TopoDS_Wire aWire1 = BRepBuilderAPI_MakeWire(pcurveOnlyEdge(0.0)).Wire();
  const TopoDS_Wire aWire2 = BRepBuilderAPI_MakeWire(pcurveOnlyEdge(5.0)).Wire();
  EXPECT_TRUE(BRepFill::Shell(aWire1, aWire2).IsNull());
}
