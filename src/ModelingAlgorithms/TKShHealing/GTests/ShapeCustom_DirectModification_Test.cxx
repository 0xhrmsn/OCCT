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

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Geom2d_Curve.hxx>
#include <Poly_PolygonOnTriangulation.hxx>
#include <Poly_Triangulation.hxx>
#include <ShapeCustom.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax2.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>

namespace
{

//! State of the meshes of a shape: every triangle must be oriented along the outer normal of
//! its face, the UV nodes must give the 3D nodes, the edges must keep their polygons.
struct MeshState
{
  int    NbFaces            = 0;
  int    NbMeshedFaces      = 0;
  int    NbTriangles        = 0;
  int    NbReversed         = 0;
  int    NbEdgesWithoutPoly = 0;
  double MaxUVDeviation     = 0.0;
  // polygons on triangulation against the pcurves, in the frame of the forward face: a seam
  // polygon is on the side of its own occurrence or on the other side, depending on how the mesh
  // was built; the modification has to keep these numbers
  int NbPolyOwnSide   = 0;
  int NbPolyOtherSide = 0;
  int NbPolyMismatch  = 0;
};

MeshState meshState(const TopoDS_Shape& theShape)
{
  MeshState aState;
  for (TopExp_Explorer aFaceExp(theShape, TopAbs_FACE); aFaceExp.More(); aFaceExp.Next())
  {
    const TopoDS_Face& aFace = TopoDS::Face(aFaceExp.Current());
    ++aState.NbFaces;
    TopLoc_Location                        aLoc;
    const occ::handle<Poly_Triangulation>& aTri = BRep_Tool::Triangulation(aFace, aLoc);
    if (aTri.IsNull())
    {
      continue;
    }
    ++aState.NbMeshedFaces;
    const gp_Trsf       aTrsf = aLoc.Transformation();
    BRepAdaptor_Surface aSurf(aFace, false);
    for (int aTriIter = 1; aTriIter <= aTri->NbTriangles(); ++aTriIter)
    {
      int aN1 = 0, aN2 = 0, aN3 = 0;
      aTri->Triangle(aTriIter).Get(aN1, aN2, aN3);
      if (aFace.Orientation() == TopAbs_REVERSED)
      {
        std::swap(aN2, aN3);
      }
      const gp_Pnt aP1      = aTri->Node(aN1).Transformed(aTrsf);
      const gp_Pnt aP2      = aTri->Node(aN2).Transformed(aTrsf);
      const gp_Pnt aP3      = aTri->Node(aN3).Transformed(aTrsf);
      const gp_Vec aTriNorm = gp_Vec(aP1, aP2).Crossed(gp_Vec(aP1, aP3));
      if (aTriNorm.Magnitude() < 1.0e-12)
      {
        continue;
      }
      ++aState.NbTriangles;
      const gp_XY aUV =
        (aTri->UVNode(aN1).XY() + aTri->UVNode(aN2).XY() + aTri->UVNode(aN3).XY()) / 3.0;
      BRepLProp_SLProps aProps(aSurf, aUV.X(), aUV.Y(), 1, 1.0e-9);
      if (!aProps.IsNormalDefined())
      {
        continue;
      }
      gp_Dir aFaceNorm = aProps.Normal();
      if (aFace.Orientation() == TopAbs_REVERSED)
      {
        aFaceNorm.Reverse();
      }
      if (aTriNorm.Dot(gp_Vec(aFaceNorm)) <= 0.0)
      {
        ++aState.NbReversed;
      }
    }
    for (int aNodeIter = 1; aNodeIter <= aTri->NbNodes(); ++aNodeIter)
    {
      const gp_Pnt2d aUV = aTri->UVNode(aNodeIter);
      aState.MaxUVDeviation =
        std::max(aState.MaxUVDeviation,
                 aSurf.Value(aUV.X(), aUV.Y()).Distance(aTri->Node(aNodeIter).Transformed(aTrsf)));
    }
    // the polygons and pcurves of the edges are read in the frame of the forward face
    const TopoDS_Face aFwdFace = TopoDS::Face(aFace.Oriented(TopAbs_FORWARD));
    for (TopExp_Explorer anEdgeExp(aFwdFace, TopAbs_EDGE); anEdgeExp.More(); anEdgeExp.Next())
    {
      const TopoDS_Edge&                             anEdge = TopoDS::Edge(anEdgeExp.Current());
      const occ::handle<Poly_PolygonOnTriangulation> aPoly =
        BRep_Tool::PolygonOnTriangulation(anEdge, aTri, aLoc);
      if (aPoly.IsNull())
      {
        ++aState.NbEdgesWithoutPoly;
        continue;
      }
      // largest distance of the polygon nodes to the pcurve of an occurrence of the edge
      auto aDeviation = [&](const TopoDS_Edge& theOccurrence) {
        double                          aFirst = 0.0, aLast = 0.0;
        const occ::handle<Geom2d_Curve> aPCurve =
          BRep_Tool::CurveOnSurface(theOccurrence, aFwdFace, aFirst, aLast);
        double aMax = 0.0;
        for (int aPolyIter = 1; aPolyIter <= aPoly->NbNodes(); ++aPolyIter)
        {
          aMax = std::max(aMax,
                          aPCurve->Value(aPoly->Parameter(aPolyIter))
                            .Distance(aTri->UVNode(aPoly->Node(aPolyIter))));
        }
        return aMax;
      };
      if (!aPoly->HasParameters())
      {
        continue;
      }
      if (aDeviation(anEdge) < 1.0e-7)
      {
        ++aState.NbPolyOwnSide;
      }
      else if (aDeviation(TopoDS::Edge(anEdge.Reversed())) < 1.0e-7)
      {
        ++aState.NbPolyOtherSide;
      }
      else
      {
        ++aState.NbPolyMismatch;
      }
    }
  }
  return aState;
}

double volumeOf(const TopoDS_Shape& theShape)
{
  GProp_GProps aProps;
  BRepGProp::VolumeProperties(theShape, aProps);
  return aProps.Mass();
}

TopoDS_Shape filletedBox()
{
  const TopoDS_Shape       aBox = BRepPrimAPI_MakeBox(100.0, 60.0, 30.0).Shape();
  BRepFilletAPI_MakeFillet aFillet(aBox);
  for (TopExp_Explorer anExp(aBox, TopAbs_EDGE); anExp.More(); anExp.Next())
  {
    aFillet.Add(3.0, TopoDS::Edge(anExp.Current()));
  }
  aFillet.Build();
  return aFillet.Shape();
}

//! Meshes theShape, redresses its faces and checks that every face keeps a mesh consistent with
//! its redressed surface.
void checkDirectFacesKeepsMesh(const TopoDS_Shape& theShape)
{
  BRepMesh_IncrementalMesh aMesher(theShape, 0.1);
  ASSERT_TRUE(aMesher.IsDone());
  const MeshState aBefore = meshState(theShape);
  ASSERT_EQ(aBefore.NbMeshedFaces, aBefore.NbFaces);
  ASSERT_EQ(aBefore.NbReversed, 0);
  ASSERT_EQ(aBefore.NbPolyMismatch, 0);

  const TopoDS_Shape aResult = ShapeCustom::DirectFaces(theShape);
  // the shape has indirect faces, so it is rebuilt
  ASSERT_FALSE(aResult.IsSame(theShape));

  const MeshState anAfter = meshState(aResult);
  EXPECT_EQ(anAfter.NbFaces, aBefore.NbFaces);
  EXPECT_EQ(anAfter.NbMeshedFaces, anAfter.NbFaces);
  EXPECT_EQ(anAfter.NbTriangles, aBefore.NbTriangles);
  EXPECT_EQ(anAfter.NbReversed, 0);
  EXPECT_LT(anAfter.MaxUVDeviation, 1.0e-7);
  EXPECT_EQ(anAfter.NbEdgesWithoutPoly, 0);
  EXPECT_EQ(anAfter.NbPolyMismatch, 0);
  EXPECT_EQ(anAfter.NbPolyOwnSide, aBefore.NbPolyOwnSide);
  EXPECT_EQ(anAfter.NbPolyOtherSide, aBefore.NbPolyOtherSide);
  EXPECT_TRUE(BRepCheck_Analyzer(aResult).IsValid());
  EXPECT_NEAR(volumeOf(aResult), volumeOf(theShape), 1.0e-6 * std::abs(volumeOf(theShape)));
}

} // namespace

// Indirect cylinders and spheres made by the fillet, with direct faces rebuilt only through their
// edges and vertices.
TEST(ShapeCustom_DirectModificationTest, DirectFaces_FilletedBox_KeepsMesh)
{
  checkDirectFacesKeepsMesh(filletedBox());
}

// A cone with a negative semi-angle is redressed by reversing U and V.
TEST(ShapeCustom_DirectModificationTest, DirectFaces_Cone_KeepsMesh)
{
  checkDirectFacesKeepsMesh(BRepPrimAPI_MakeCone(20.0, 5.0, 30.0).Shape());
}

// The mesh stays in the frame of a mirrored location.
TEST(ShapeCustom_DirectModificationTest, DirectFaces_MirroredLocation_KeepsMesh)
{
  gp_Trsf aMirror;
  aMirror.SetMirror(gp_Ax2(gp::Origin(), gp::DX()));
  checkDirectFacesKeepsMesh(filletedBox().Moved(TopLoc_Location(aMirror)));
}

// A mirrored cylinder has an indirect cylindrical surface (U reversed, wires reversed): the
// polygons of the seam stay on the same side as their pcurves, which are swapped with the wires.
TEST(ShapeCustom_DirectModificationTest, DirectFaces_MirroredCylinder_SeamPolygons)
{
  gp_Trsf aMirror;
  aMirror.SetMirror(gp_Ax2(gp::Origin(), gp::DX()));
  checkDirectFacesKeepsMesh(
    BRepBuilderAPI_Transform(BRepPrimAPI_MakeCylinder(20.0, 40.0).Shape(), aMirror, true).Shape());
}

// A mirrored cone with a negative semi-angle is an indirect negative cone (V reversed, wires
// reversed).
TEST(ShapeCustom_DirectModificationTest, DirectFaces_MirroredCone_SeamPolygons)
{
  gp_Trsf aMirror;
  aMirror.SetMirror(gp_Ax2(gp::Origin(), gp::DX()));
  checkDirectFacesKeepsMesh(
    BRepBuilderAPI_Transform(BRepPrimAPI_MakeCone(20.0, 5.0, 30.0).Shape(), aMirror, true).Shape());
}

// Without a mesh, no mesh is created.
TEST(ShapeCustom_DirectModificationTest, DirectFaces_WithoutMesh_NoMeshCreated)
{
  const TopoDS_Shape aShape  = filletedBox();
  const TopoDS_Shape aResult = ShapeCustom::DirectFaces(aShape);
  ASSERT_FALSE(aResult.IsSame(aShape));
  const MeshState aState = meshState(aResult);
  EXPECT_EQ(aState.NbFaces, 26);
  EXPECT_EQ(aState.NbMeshedFaces, 0);
  EXPECT_TRUE(BRepCheck_Analyzer(aResult).IsValid());
  EXPECT_NEAR(volumeOf(aResult), volumeOf(aShape), 1.0e-6 * volumeOf(aShape));
}

// Guard: a shape whose surfaces are all direct is not modified, even with a mesh.
TEST(ShapeCustom_DirectModificationTest, DirectFaces_DirectShape_NotModified)
{
  const TopoDS_Shape       aBox = BRepPrimAPI_MakeBox(100.0, 60.0, 30.0).Shape();
  BRepMesh_IncrementalMesh aMesher(aBox, 0.5);
  ASSERT_TRUE(aMesher.IsDone());
  EXPECT_TRUE(ShapeCustom::DirectFaces(aBox).IsSame(aBox));
}
