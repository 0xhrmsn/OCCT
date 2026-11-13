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
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepOffsetAPI_MakeFilling.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_SphericalSurface.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopTools_ShapeMapHasher.hxx>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace
{
using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

std::vector<TopoDS_Edge> polygonEdges(const std::vector<gp_Pnt>& thePnts, const bool theClosed)
{
  std::vector<TopoDS_Edge> anEdges;
  const size_t             aNb = theClosed ? thePnts.size() : thePnts.size() - 1;
  for (size_t i = 0; i < aNb; ++i)
  {
    anEdges.push_back(BRepBuilderAPI_MakeEdge(thePnts[i], thePnts[(i + 1) % thePnts.size()]));
  }
  return anEdges;
}

// Checks that every boundary edge generates exactly its new edge of the result (unchanged
// behaviour: callers such as the Draw command "generated" expect a single edge).
void checkBoundary(BRepOffsetAPI_MakeFilling& theFilling, const std::vector<TopoDS_Edge>& theEdges)
{
  const ShapeMap aResultEdges = [&] {
    ShapeMap aMap;
    TopExp::MapShapes(theFilling.Shape(), TopAbs_EDGE, aMap);
    return aMap;
  }();
  for (size_t i = 0; i < theEdges.size(); ++i)
  {
    const NCollection_List<TopoDS_Shape> aGen = theFilling.Generated(theEdges[i]);
    ASSERT_EQ(aGen.Extent(), 1) << "edge " << i;
    EXPECT_TRUE(aResultEdges.Contains(aGen.First())) << "edge " << i;
  }
}
} // namespace

// Guard: the history of the boundary is unchanged.
TEST(BRepOffsetAPI_MakeFillingTest, BoundaryEdges_GenerateTheirNewEdges)
{
  const std::vector<TopoDS_Edge> anEdges =
    polygonEdges({gp_Pnt(0, 0, 0), gp_Pnt(40, 0, 5), gp_Pnt(40, 30, 0), gp_Pnt(0, 30, 5)}, true);
  BRepOffsetAPI_MakeFilling aFilling;
  for (const TopoDS_Edge& anEdge : anEdges)
  {
    aFilling.Add(anEdge, GeomAbs_C0);
  }
  aFilling.Build();
  ASSERT_TRUE(aFilling.IsDone());
  ASSERT_EQ(aFilling.Shape().ShapeType(), TopAbs_FACE);
  checkBoundary(aFilling, anEdges);

  // Vertices of the boundary still generate only their new vertices.
  TopoDS_Vertex aV1, aV2;
  TopExp::Vertices(anEdges.front(), aV1, aV2);
  const NCollection_List<TopoDS_Shape> aGenV = aFilling.Generated(aV1);
  ASSERT_EQ(aGenV.Extent(), 1);
  EXPECT_EQ(aGenV.First().ShapeType(), TopAbs_VERTEX);
}

// An edge constraint inside the face (not a bound) generates the face; it generated nothing.
TEST(BRepOffsetAPI_MakeFillingTest, InnerEdgeConstraint_GeneratesTheFace)
{
  const std::vector<TopoDS_Edge> anEdges =
    polygonEdges({gp_Pnt(0, 0, 0), gp_Pnt(40, 0, 0), gp_Pnt(40, 30, 0), gp_Pnt(0, 30, 0)}, true);
  const TopoDS_Edge anInner = BRepBuilderAPI_MakeEdge(
    GC_MakeArcOfCircle(gp_Pnt(10, 15, 0), gp_Pnt(20, 15, 4), gp_Pnt(30, 15, 0)).Value());
  BRepOffsetAPI_MakeFilling aFilling;
  for (const TopoDS_Edge& anEdge : anEdges)
  {
    aFilling.Add(anEdge, GeomAbs_C0);
  }
  aFilling.Add(anInner, GeomAbs_C0, false);
  aFilling.Build();
  ASSERT_TRUE(aFilling.IsDone());
  checkBoundary(aFilling, anEdges);
  const NCollection_List<TopoDS_Shape> aGen = aFilling.Generated(anInner);
  ASSERT_EQ(aGen.Extent(), 1);
  EXPECT_TRUE(aGen.First().IsSame(aFilling.Shape()));
}

// A face given as a free constraint closes the open side of the boundary: it generates the edge of
// the result built on it and the face; it generated nothing, so that edge and the face had no
// origin.
TEST(BRepOffsetAPI_MakeFillingTest, FreeFaceConstraint_GeneratesTheFace)
{
  const std::vector<TopoDS_Edge> anEdges =
    polygonEdges({gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0), gp_Pnt(10, 10, 0), gp_Pnt(0, 10, 0)}, false);
  BRepBuilderAPI_MakePolygon aWall(gp_Pnt(0, -5, -5),
                                   gp_Pnt(0, 15, -5),
                                   gp_Pnt(0, 15, 5),
                                   gp_Pnt(0, -5, 5),
                                   true);
  const TopoDS_Face          aSupport = BRepBuilderAPI_MakeFace(aWall.Wire(), true).Face();
  BRepOffsetAPI_MakeFilling  aFilling;
  for (const TopoDS_Edge& anEdge : anEdges)
  {
    aFilling.Add(anEdge, GeomAbs_C0);
  }
  aFilling.Add(aSupport, GeomAbs_C0);
  aFilling.Build();
  ASSERT_TRUE(aFilling.IsDone());
  checkBoundary(aFilling, anEdges);
  const NCollection_List<TopoDS_Shape> aGen = aFilling.Generated(aSupport);
  ASSERT_EQ(aGen.Extent(), 2);
  EXPECT_EQ(aGen.First().ShapeType(), TopAbs_EDGE);
  EXPECT_TRUE(aGen.Last().IsSame(aFilling.Shape()));

  // Every edge of the result is traced: three from the boundary, one from the free face.
  ShapeMap aTraced;
  aTraced.Add(aGen.First());
  for (const TopoDS_Edge& anEdge : anEdges)
  {
    aTraced.Add(aFilling.Generated(anEdge).First());
  }
  ShapeMap aResultEdges;
  TopExp::MapShapes(aFilling.Shape(), TopAbs_EDGE, aResultEdges);
  EXPECT_EQ(aResultEdges.Extent(), 4);
  for (int i = 1; i <= aResultEdges.Extent(); ++i)
  {
    EXPECT_TRUE(aTraced.Contains(aResultEdges(i))) << "untraced result edge " << i;
  }
}

// G0Error() reported the error of the plate surface only; the face is a B-spline approximation of
// it, and its distance to the constraints was larger than reported (3.8e-7 reported for 8.4e-5).
TEST(BRepOffsetAPI_MakeFillingTest, G0Error_CoversDistanceOfFaceToConstraints)
{
  const occ::handle<Geom_SphericalSurface> aSphere = new Geom_SphericalSurface(gp_Ax3(), 50.0);
  const TopoDS_Face aPatch = BRepBuilderAPI_MakeFace(aSphere, 0.0, 1.2, -0.4, 0.6, 1.0e-7).Face();
  BRepOffsetAPI_MakeFilling aFilling(3, 15, 2, false, 1.0e-5, 1.0e-4, 1.0e-2, 0.1, 8, 9);
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> anEdges;
  TopExp::MapShapes(aPatch, TopAbs_EDGE, anEdges);
  for (int i = 1; i <= anEdges.Extent(); ++i)
  {
    if (!BRep_Tool::Degenerated(TopoDS::Edge(anEdges(i))))
    {
      aFilling.Add(TopoDS::Edge(anEdges(i)), GeomAbs_C0);
    }
  }
  const gp_Pnt anInner = aSphere->Value(0.6, 0.1);
  aFilling.Add(anInner);
  aFilling.Build();
  ASSERT_TRUE(aFilling.IsDone());
  const occ::handle<Geom_Surface> aResult = BRep_Tool::Surface(TopoDS::Face(aFilling.Shape()));

  double aMaxDist = 0.0;
  for (int i = 1; i <= anEdges.Extent(); ++i)
  {
    const TopoDS_Edge& anEdge = TopoDS::Edge(anEdges(i));
    if (BRep_Tool::Degenerated(anEdge))
    {
      continue;
    }
    BRepAdaptor_Curve aCurve(anEdge);
    for (int k = 0; k <= 30; ++k)
    {
      const double aPar =
        aCurve.FirstParameter() + (aCurve.LastParameter() - aCurve.FirstParameter()) * k / 30.0;
      GeomAPI_ProjectPointOnSurf aProj(aCurve.Value(aPar), aResult);
      ASSERT_GT(aProj.NbPoints(), 0);
      aMaxDist = std::max(aMaxDist, aProj.LowerDistance());
    }
  }
  GeomAPI_ProjectPointOnSurf aProj(anInner, aResult);
  ASSERT_GT(aProj.NbPoints(), 0);
  aMaxDist = std::max(aMaxDist, aProj.LowerDistance());

  EXPECT_LE(aMaxDist, aFilling.G0Error() * (1.0 + 1.0e-6)) << "G0Error " << aFilling.G0Error();
}

// Likewise G1Error() reported the angle of the plate surface: the face, its approximation, made
// angles with the support faces 3 to 15 times larger than reported. Here the filling of a
// spherical patch is tangent to the patch itself along its boundary.
TEST(BRepOffsetAPI_MakeFillingTest, G1Error_CoversAngleOfFaceToSupports)
{
  const occ::handle<Geom_SphericalSurface> aSphere = new Geom_SphericalSurface(gp_Ax3(), 50.0);
  const TopoDS_Face aPatch = BRepBuilderAPI_MakeFace(aSphere, 0.0, 1.2, -0.4, 0.6, 1.0e-7).Face();
  BRepOffsetAPI_MakeFilling aFilling(3, 15, 2, false, 1.0e-5, 1.0e-4, 1.0e-2, 0.1, 8, 9);
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> anEdges;
  TopExp::MapShapes(aPatch, TopAbs_EDGE, anEdges);
  for (int i = 1; i <= anEdges.Extent(); ++i)
  {
    if (!BRep_Tool::Degenerated(TopoDS::Edge(anEdges(i))))
    {
      aFilling.Add(TopoDS::Edge(anEdges(i)), aPatch, GeomAbs_G1);
    }
  }
  aFilling.Build();
  ASSERT_TRUE(aFilling.IsDone());
  const occ::handle<Geom_Surface> aResult = BRep_Tool::Surface(TopoDS::Face(aFilling.Shape()));

  // angle between the normals of the face and of the sphere at 101 points of each edge
  double aMaxAngle = 0.0;
  for (int i = 1; i <= anEdges.Extent(); ++i)
  {
    const TopoDS_Edge& anEdge = TopoDS::Edge(anEdges(i));
    if (BRep_Tool::Degenerated(anEdge))
    {
      continue;
    }
    BRepAdaptor_Curve aCurve(anEdge);
    for (int k = 0; k <= 100; ++k)
    {
      const double aPar =
        aCurve.FirstParameter() + (aCurve.LastParameter() - aCurve.FirstParameter()) * k / 100.0;
      const gp_Pnt aPnt = aCurve.Value(aPar);
      GeomAPI_ProjectPointOnSurf aProj(aPnt, aResult);
      ASSERT_GT(aProj.NbPoints(), 0);
      double aU = 0.0, aV = 0.0;
      aProj.LowerDistanceParameters(aU, aV);
      gp_Pnt aP;
      gp_Vec aD1U, aD1V;
      aResult->D1(aU, aV, aP, aD1U, aD1V);
      const gp_Vec aNFace(aD1U.Crossed(aD1V));
      const gp_Vec aNSphere(gp_Pnt(0.0, 0.0, 0.0), aPnt); // radial
      const double anAngle = aNFace.Angle(aNSphere);
      aMaxAngle            = std::max(aMaxAngle, std::min(anAngle, M_PI - anAngle));
    }
  }
  // 10% for the angle between the sample points of G1Error()
  EXPECT_LE(aMaxAngle, aFilling.G1Error() * 1.1) << "G1Error " << aFilling.G1Error();
}

