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

#include <gtest/gtest.h>

#include <BRepBuilderAPI_NurbsConvert.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>

namespace
{

double volumeOf(const TopoDS_Shape& theShape)
{
  GProp_GProps aProps;
  BRepGProp::VolumeProperties(theShape, aProps);
  return aProps.Mass();
}

} // namespace

// The faces that are already B-spline keep their surface when the meshed shape is converted:
// the mesh is copied, the face is not replaced by a face holding only the mesh.
TEST(BRepBuilderAPI_NurbsConvertTest, MeshedBSplineFaces_KeepSurface)
{
  const TopoDS_Shape       aCylinder = BRepPrimAPI_MakeCylinder(20.0, 40.0).Shape();
  const TopoDS_Shape       aNurbs    = BRepBuilderAPI_NurbsConvert(aCylinder).Shape();
  BRepMesh_IncrementalMesh aMesher(aNurbs, 0.1);
  ASSERT_TRUE(aMesher.IsDone());

  const TopoDS_Shape aResult  = BRepBuilderAPI_NurbsConvert(aNurbs).Shape();
  int                aNbFaces = 0;
  for (TopExp_Explorer anExp(aResult, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    ++aNbFaces;
    const TopoDS_Face&               aFace = TopoDS::Face(anExp.Current());
    const occ::handle<Geom_Surface>& aSurf = BRep_Tool::Surface(aFace);
    ASSERT_FALSE(aSurf.IsNull()) << "face " << aNbFaces << " has lost its surface";
    EXPECT_TRUE(aSurf->IsKind(STANDARD_TYPE(Geom_BSplineSurface)));
    TopLoc_Location aLoc;
    EXPECT_FALSE(BRep_Tool::Triangulation(aFace, aLoc).IsNull());
  }
  EXPECT_EQ(aNbFaces, 3);
  EXPECT_TRUE(BRepCheck_Analyzer(aResult).IsValid());
  EXPECT_NEAR(volumeOf(aResult), volumeOf(aNurbs), 1.0e-6 * volumeOf(aNurbs));
}

// Guard: faces that are converted get a new B-spline surface and keep their mesh.
TEST(BRepBuilderAPI_NurbsConvertTest, MeshedElementaryFaces_Converted)
{
  const TopoDS_Shape       aBox = BRepPrimAPI_MakeBox(100.0, 60.0, 30.0).Shape();
  BRepMesh_IncrementalMesh aMesher(aBox, 0.5);
  ASSERT_TRUE(aMesher.IsDone());

  const TopoDS_Shape aResult  = BRepBuilderAPI_NurbsConvert(aBox).Shape();
  int                aNbFaces = 0;
  for (TopExp_Explorer anExp(aResult, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    ++aNbFaces;
    const TopoDS_Face&               aFace = TopoDS::Face(anExp.Current());
    const occ::handle<Geom_Surface>& aSurf = BRep_Tool::Surface(aFace);
    ASSERT_FALSE(aSurf.IsNull());
    EXPECT_TRUE(aSurf->IsKind(STANDARD_TYPE(Geom_BSplineSurface)));
    TopLoc_Location aLoc;
    EXPECT_FALSE(BRep_Tool::Triangulation(aFace, aLoc).IsNull());
  }
  EXPECT_EQ(aNbFaces, 6);
  EXPECT_TRUE(BRepCheck_Analyzer(aResult).IsValid());
  EXPECT_NEAR(volumeOf(aResult), 180000.0, 1.0e-6);
}
