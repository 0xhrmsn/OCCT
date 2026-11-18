// Copyright (c) 2025 OPEN CASCADE SAS
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

#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Line.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GeomAPI_PointsToBSplineSurface.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_BSplineSurface.hxx>
#include <NCollection_Array2.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Trsf.hxx>

#include <cmath>

//=================================================================================================

// Bug BUC60870: BRepExtrema_DistShapeShape should correctly compute the minimum distance
// between a linear edge and a vertex.
// The edge runs from (0,0,0) to (0,1,0); the vertex is at (0,0.3,1).
// The theoretical minimum distance is 1.0 (perpendicular from (0,0.3,1) to (0,0.3,0)).
TEST(BRepExtrema_DistShapeShapeTest, BUC60870_EdgeToVertexMinimumDistance)
{
  const TopoDS_Edge   anEdge  = BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(0, 1, 0)).Edge();
  const TopoDS_Vertex aVertex = BRepBuilderAPI_MakeVertex(gp_Pnt(0, 0.3, 1)).Vertex();

  BRepExtrema_DistShapeShape aDist(anEdge, aVertex, 2.0);

  ASSERT_TRUE(aDist.IsDone()) << "BRepExtrema_DistShapeShape failed to compute";
  ASSERT_GE(aDist.NbSolution(), 1) << "Expected at least one solution";

  const double aTheorDist  = 1.0;
  const double aPercentMax = 0.01; // 1% relative tolerance
  EXPECT_NEAR(aDist.Value(), aTheorDist, aTheorDist * aPercentMax)
    << "Minimum distance deviates from expected value 1.0";
}

//=================================================================================================

// Verify that BRepExtrema_DistShapeShape does not crash when computing the distance
// between two edges and one of them has no 3D curve (null Geom_Curve from BRep_Tool::Curve).
// This exercises the null-check in the PERFORM_C0 helper.
TEST(BRepExtrema_DistShapeShapeTest, EdgeEdge_Null3DCurve_NoCrash)
{
  const TopoDS_Edge aEdge1 = BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(1, 0, 0)).Edge();

  const occ::handle<Geom_Circle> aCircle =
    new Geom_Circle(gp_Ax2(gp_Pnt(5, 0, 0), gp_Dir(0, 0, 1)), 1.0);
  TopoDS_Edge aEdge2 = BRepBuilderAPI_MakeEdge(aCircle).Edge();

  BRep_Builder aBuilder;
  aBuilder.UpdateEdge(aEdge2, occ::handle<Geom_Curve>(), 1e-7);

  BRepExtrema_DistShapeShape aDist(aEdge1, aEdge2, 10.0);
  (void)aDist;
}

//=================================================================================================

// Verify that BRepExtrema_DistShapeShape does not crash when computing the distance
// between an edge with no 3D curve and a face.
// This exercises the null-check in Perform(Edge, Face).
TEST(BRepExtrema_DistShapeShapeTest, EdgeFace_Null3DCurve_NoCrash)
{
  const occ::handle<Geom_Circle> aCircle =
    new Geom_Circle(gp_Ax2(gp_Pnt(5, 0, 0), gp_Dir(0, 0, 1)), 1.0);
  TopoDS_Edge aEdge = BRepBuilderAPI_MakeEdge(aCircle).Edge();

  BRep_Builder aBuilder;
  aBuilder.UpdateEdge(aEdge, occ::handle<Geom_Curve>(), 1e-7);

  const TopoDS_Face aFace =
    BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0, 0, 5), gp_Dir(0, 0, 1)), -10, 10, -10, 10);

  BRepExtrema_DistShapeShape aDist(aEdge, aFace, 10.0);
  (void)aDist;
}

// A ball end mill (spherical face with its pole at the tip) over a wavy B-spline face. The
// minimization between the faces ended beyond the pole (V < -PI/2) at the right point, which was
// rejected as out of the face: the distance came from the tip vertex (0.177 instead of 0.097),
// and a ball sunk into the face was found apart from it.
TEST(BRepExtrema_DistShapeShapeTest, BallOverBSpline_MinimumBeyondPoleKept)
{
  NCollection_Array2<gp_Pnt> aPnts(1, 81, 1, 81);
  for (int i = 1; i <= 81; ++i)
  {
    for (int j = 1; j <= 81; ++j)
    {
      const double x = i - 1.0, y = j - 1.0;
      aPnts(i, j) = gp_Pnt(x, y, 2.0 * std::sin(2 * M_PI * x / 40) * std::cos(2 * M_PI * y / 40));
    }
  }
  GeomAPI_PointsToBSplineSurface anInterp;
  anInterp.Interpolate(aPnts);
  const occ::handle<Geom_BSplineSurface> aSurf = anInterp.Surface();
  const TopoDS_Face aWave = BRepBuilderAPI_MakeFace(aSurf, 1e-7).Face();

  const double            R = 3.0, H = 20.0;
  BRepBuilderAPI_MakeWire aWire;
  aWire.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(gp_Pnt(0, 0, 0),
                                                       gp_Pnt(R * std::sin(M_PI / 4),
                                                              0,
                                                              R - R * std::cos(M_PI / 4)),
                                                       gp_Pnt(R, 0, R))
                                      .Value())
              .Edge());
  aWire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(R, 0, R), gp_Pnt(R, 0, H)).Edge());
  aWire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(R, 0, H), gp_Pnt(0, 0, H)).Edge());
  aWire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, H), gp_Pnt(0, 0, 0)).Edge());
  const TopoDS_Shape aTool =
    BRepPrimAPI_MakeRevol(BRepBuilderAPI_MakeFace(aWire.Wire(), true).Face(),
                          gp_Ax1(gp::Origin(), gp::DZ()))
      .Shape();

  // tip above the face: distance = distance of the center of the ball to the face - R
  // tip sunk 0.1 into the face: distance 0
  const gp_Pnt aTipAbove(36.875, 55.740740740740733, 0.92181643685826242);
  const gp_Pnt aTipSunk(58.75, 42.777777777777779, 0.36741863926080243);
  for (const gp_Pnt& aTip : {aTipAbove, aTipSunk})
  {
    gp_Trsf aTrsf;
    aTrsf.SetTranslation(gp_Vec(aTip.XYZ()));
    BRepExtrema_DistShapeShape aDist(aTool.Located(TopLoc_Location(aTrsf)), aWave);
    ASSERT_TRUE(aDist.IsDone());

    GeomAPI_ProjectPointOnSurf aProj(gp_Pnt(aTip.X(), aTip.Y(), aTip.Z() + R), aSurf);
    ASSERT_GT(aProj.NbPoints(), 0);
    const double anExpected = std::max(0.0, aProj.LowerDistance() - R);
    EXPECT_NEAR(aDist.Value(), anExpected, 1e-6) << "tip " << aTip.X() << " " << aTip.Y();
  }
}
