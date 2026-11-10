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

#include <BRepGProp_VinertGK.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepGProp_Domain.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepGProp_Vinert.hxx>
#include <gp_Pln.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepGProp_Vinert.hxx>
#include <gp_Pln.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomConvert.hxx>
#include <Geom_Circle.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_SurfaceOfLinearExtrusion.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Plane.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <GProp_GProps.hxx>
#include <GProp_PrincipalProps.hxx>
#include <NCollection_Array1.hxx>
#include <Precision.hxx>
#include <Standard_Handle.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <gtest/gtest.h>

#include <cmath>

TEST(BRepGPropTest, LinearProperties_EdgeLength)
{
  gp_Pnt aP1(0.0, 0.0, 0.0);
  gp_Pnt aP2(3.0, 4.0, 0.0);

  BRepBuilderAPI_MakeEdge aMakeEdge(aP1, aP2);
  ASSERT_TRUE(aMakeEdge.IsDone());

  GProp_GProps aProps;
  BRepGProp::LinearProperties(aMakeEdge.Edge(), aProps);
  EXPECT_NEAR(aProps.Mass(), 5.0, Precision::Confusion()) << "Edge length should be 5";
}

TEST(BRepGPropTest, SurfaceProperties_BoxFaceArea)
{
  BRepPrimAPI_MakeBox aBox(10.0, 20.0, 30.0);
  TopoDS_Shape        aShape = aBox.Shape();
  ASSERT_TRUE(aBox.IsDone());

  // Total surface area = 2*(10*20 + 10*30 + 20*30) = 2*(200+300+600) = 2200
  GProp_GProps aProps;
  BRepGProp::SurfaceProperties(aShape, aProps);
  EXPECT_NEAR(aProps.Mass(), 2200.0, Precision::Confusion());
}

TEST(BRepGPropTest, VolumeProperties_UnitBox)
{
  BRepPrimAPI_MakeBox aBox(1.0, 1.0, 1.0);
  TopoDS_Shape        aShape = aBox.Shape();
  ASSERT_TRUE(aBox.IsDone());

  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aShape, aProps);
  EXPECT_NEAR(aProps.Mass(), 1.0, Precision::Confusion()) << "Unit box volume should be 1";
}

TEST(BRepGPropTest, VolumeProperties_Sphere)
{
  const double           aRadius = 5.0;
  BRepPrimAPI_MakeSphere aSphere(aRadius);
  TopoDS_Shape           aShape = aSphere.Shape();
  ASSERT_TRUE(aSphere.IsDone());

  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aShape, aProps);
  double anExpectedVolume = (4.0 / 3.0) * M_PI * aRadius * aRadius * aRadius;
  EXPECT_NEAR(aProps.Mass(), anExpectedVolume, 0.01) << "Sphere volume mismatch";
}

TEST(BRepGPropTest, VolumeProperties_BoxCenterOfMass)
{
  // Box from (0,0,0) to (10,10,10)
  BRepPrimAPI_MakeBox aBox(10.0, 10.0, 10.0);
  TopoDS_Shape        aShape = aBox.Shape();
  ASSERT_TRUE(aBox.IsDone());

  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aShape, aProps);
  gp_Pnt aCOM = aProps.CentreOfMass();

  // Center of mass should be at (5, 5, 5)
  EXPECT_NEAR(aCOM.X(), 5.0, Precision::Confusion());
  EXPECT_NEAR(aCOM.Y(), 5.0, Precision::Confusion());
  EXPECT_NEAR(aCOM.Z(), 5.0, Precision::Confusion());
}

TEST(BRepGPropTest, LinearProperties_SkipShared)
{
  BRepPrimAPI_MakeBox aBox(10.0, 10.0, 10.0);
  TopoDS_Shape        aShape = aBox.Shape();
  ASSERT_TRUE(aBox.IsDone());

  // With SkipShared = false, shared edges are counted multiple times
  GProp_GProps aPropsNotSkipped;
  BRepGProp::LinearProperties(aShape, aPropsNotSkipped, false);

  // With SkipShared = true, each edge is counted once
  GProp_GProps aPropsSkipped;
  BRepGProp::LinearProperties(aShape, aPropsSkipped, true);

  // Box has 12 edges, each of length 10 = total 120
  EXPECT_NEAR(aPropsSkipped.Mass(), 120.0, Precision::Confusion())
    << "Total edge length of 10x10x10 box with SkipShared=true should be 120";

  // Without SkipShared, each edge is counted for each face it belongs to (2 faces per edge)
  // So total = 240
  EXPECT_NEAR(aPropsNotSkipped.Mass(), 240.0, Precision::Confusion())
    << "Total edge length with SkipShared=false should be double";
}

// Test OCC49: GProp_PrincipalProps::HasSymmetryAxis - cylinder has symmetry, cut does not.
// Migrated from QABugs_16.cxx OCC49
TEST(BRepGPropTest, OCC49_CylinderHasSymmetryAxis)
{
  const TopoDS_Shape aCylinder = BRepPrimAPI_MakeCylinder(10., 20.).Shape();

  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aCylinder, aProps);
  const GProp_PrincipalProps aPrincipal = aProps.PrincipalProperties();
  EXPECT_TRUE(aPrincipal.HasSymmetryAxis());
}

TEST(BRepGPropTest, OCC49_CutShapeHasNoSymmetryAxis)
{
  const TopoDS_Shape aCylinder = BRepPrimAPI_MakeCylinder(10., 20.).Shape();
  const TopoDS_Shape aBox      = BRepPrimAPI_MakeBox(10., 10., 10.).Shape();

  BRepAlgoAPI_Cut aCut(aCylinder, aBox);
  ASSERT_TRUE(aCut.IsDone());

  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aCut.Shape(), aProps);
  const GProp_PrincipalProps aPrincipal = aProps.PrincipalProperties();
  EXPECT_FALSE(aPrincipal.HasSymmetryAxis());
}

// OCC8797: Verify that GCPnts_AbscissaPoint::Length and BRepGProp::LinearProperties
// produce consistent arc-length values for a degree-3 BSpline curve with 7 poles.
// Both methods must agree within a tight relative tolerance.

TEST(BRepGPropTest, OCC8797_BSplineLengthConsistencyAbscissaVsLinearProperties)
{
  NCollection_Array1<gp_Pnt> aPoles(0, 6);
  aPoles(0) = gp_Pnt(0.0, 0.0, 0.0);
  aPoles(1) = gp_Pnt(1.0, 1.0, 0.0);
  aPoles(2) = gp_Pnt(2.0, 1.0, 0.0);
  aPoles(3) = gp_Pnt(3.0, 0.0, 0.0);
  aPoles(4) = gp_Pnt(4.0, 1.0, 0.0);
  aPoles(5) = gp_Pnt(5.0, 1.0, 0.0);
  aPoles(6) = gp_Pnt(6.0, 0.0, 0.0);

  NCollection_Array1<double> aKnots(0, 2);
  aKnots(0) = 0.0;
  aKnots(1) = 0.5;
  aKnots(2) = 1.0;

  NCollection_Array1<int> aMults(0, 2);
  aMults(0) = 4;
  aMults(1) = 3;
  aMults(2) = 4;

  occ::handle<Geom_BSplineCurve> aSpline = new Geom_BSplineCurve(aPoles, aKnots, aMults, 3);
  ASSERT_FALSE(aSpline.IsNull());
  EXPECT_EQ(aSpline->NbPoles(), 7);
  EXPECT_EQ(aSpline->NbKnots(), 3);

  // Method 1: GCPnts_AbscissaPoint::Length
  GeomAdaptor_Curve anAdaptor(aSpline);
  const double      aLengthAbscissa = GCPnts_AbscissaPoint::Length(anAdaptor);
  EXPECT_GT(aLengthAbscissa, 0.0);

  // Method 2: BRepGProp::LinearProperties on the equivalent edge
  const TopoDS_Edge aEdge = BRepBuilderAPI_MakeEdge(aSpline);
  GProp_GProps      aEdgeProps;
  BRepGProp::LinearProperties(aEdge, aEdgeProps);
  const double aLengthGProp = aEdgeProps.Mass();
  EXPECT_GT(aLengthGProp, 0.0);

  // Both methods must agree within 0.1 %
  EXPECT_NEAR(aLengthAbscissa, aLengthGProp, aLengthGProp * 1e-3);
}

// Regression: a degenerate edge whose ONLY representation is a Bezier/BSpline-type
// curve-on-surface pcurve (no 3D curve) used to SIGSEGV inside
// BRepGProp_EdgeTool::IntegrationOrder, which read the pole count via BAC.Curve().Curve()
// (null when there is no 3D curve) instead of the adaptor's own NbPoles(). This is the
// shape BRepBuilderAPI_Sewing produces reconciling near-coincident vertices between two
// faces that do not share an edge outright.
TEST(BRepGPropTest, LinearProperties_DegenerateEdgeWithBSplinePCurveNoCrash)
{
  occ::handle<Geom_Plane> aPlane = new Geom_Plane(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));

  NCollection_Array1<gp_Pnt2d> aPoles(1, 4);
  aPoles.SetValue(1, gp_Pnt2d(0.0, 0.0));
  aPoles.SetValue(2, gp_Pnt2d(0.1, 0.05));
  aPoles.SetValue(3, gp_Pnt2d(0.2, -0.05));
  aPoles.SetValue(4, gp_Pnt2d(0.3, 0.0));

  NCollection_Array1<double> aKnots(1, 2);
  aKnots.SetValue(1, 0.0);
  aKnots.SetValue(2, 1.0);

  NCollection_Array1<int> aMults(1, 2);
  aMults.SetValue(1, 4);
  aMults.SetValue(2, 4);

  occ::handle<Geom2d_BSplineCurve> aPCurve = new Geom2d_BSplineCurve(aPoles, aKnots, aMults, 3);

  BRep_Builder aBuilder;
  TopoDS_Edge  anEdge;
  aBuilder.MakeEdge(anEdge);
  aBuilder.UpdateEdge(anEdge, aPCurve, aPlane, TopLoc_Location(), Precision::Confusion());
  aBuilder.Range(anEdge,
                 aPlane,
                 TopLoc_Location(),
                 aPCurve->FirstParameter(),
                 aPCurve->LastParameter());
  aBuilder.Degenerated(anEdge, true);

  GProp_GProps aProps;
  BRepGProp::LinearProperties(anEdge, aProps);

  // Reaching this point at all is the regression check: IntegrationOrder used to SIGSEGV
  // before returning.
  EXPECT_TRUE(std::isfinite(aProps.Mass()));
  EXPECT_GE(aProps.Mass(), 0.0);
}

// A 3 x 4 face at z = 2 and the plane z = 1 delimit a 3 x 4 x 1 block. The by-plane overloads
// must measure about the plane as passed, not its mirror through the origin. The location is the
// centre of the block, so the diagonal is m (b^2 + c^2) / 12 whichever point the matrix refers to.
TEST(BRepGPropTest, VolumePropertiesByPlane_OffsetPlane_Inertia)
{
  const gp_Dir            aNormal(0.0, 0.0, 1.0);
  BRepBuilderAPI_MakeFace aFaceBuilder(gp_Pln(gp_Pnt(0.0, 0.0, 2.0), aNormal), 0.0, 3.0, 0.0, 4.0);
  ASSERT_TRUE(aFaceBuilder.IsDone());
  const TopoDS_Face aFace = aFaceBuilder.Face();
  const gp_Pln      aPlane(gp_Pnt(0.0, 0.0, 1.0), aNormal);
  const gp_Pnt      aCentre(1.5, 2.0, 1.5);

  BRepGProp_Face   aFaceTool(aFace);
  BRepGProp_Domain aDomain(aFace);
  BRepGProp_Vinert aVinert(aFaceTool, aDomain, aPlane, aCentre);
  EXPECT_NEAR(aVinert.MatrixOfInertia().Value(1, 1), 17.0, 1.0e-10);
  EXPECT_NEAR(aVinert.MatrixOfInertia().Value(2, 2), 10.0, 1.0e-10);
  EXPECT_NEAR(aVinert.MatrixOfInertia().Value(3, 3), 25.0, 1.0e-10);
}

TEST(BRepGPropTest, VolumePropertiesByPlane_OffsetPlane_MassAndCentre)
{
  const gp_Dir            aNormal(0.0, 0.0, 1.0);
  BRepBuilderAPI_MakeFace aFaceBuilder(gp_Pln(gp_Pnt(0.0, 0.0, 2.0), aNormal), 0.0, 3.0, 0.0, 4.0);
  ASSERT_TRUE(aFaceBuilder.IsDone());
  const TopoDS_Face aFace = aFaceBuilder.Face();
  const gp_Pln      aPlane(gp_Pnt(0.0, 0.0, 1.0), aNormal);

  for (const gp_Pnt& aLocation : {gp_Pnt(0.0, 0.0, 0.0), gp_Pnt(0.0, 0.0, 4.0)})
  {
    BRepGProp_Face     aFaceTool(aFace);
    BRepGProp_Domain   aDomain(aFace);
    BRepGProp_VinertGK aGK(aFaceTool, aDomain, aPlane, aLocation, 1.0e-8, true);
    EXPECT_NEAR(aGK.Mass(), 12.0, 1.0e-8);
    EXPECT_NEAR(aGK.CentreOfMass().X(), 1.5, 1.0e-8);
    EXPECT_NEAR(aGK.CentreOfMass().Y(), 2.0, 1.0e-8);
    EXPECT_NEAR(aGK.CentreOfMass().Z(), 1.5, 1.0e-8);
  }
}

namespace
{
void expectGPropsNear(const GProp_GProps& theActual,
                      const GProp_GProps& theExpected,
                      const double        theTolerance)
{
  EXPECT_NEAR(theActual.Mass(), theExpected.Mass(), theTolerance);
  EXPECT_NEAR(theActual.CentreOfMass().X(), theExpected.CentreOfMass().X(), theTolerance);
  EXPECT_NEAR(theActual.CentreOfMass().Y(), theExpected.CentreOfMass().Y(), theTolerance);
  EXPECT_NEAR(theActual.CentreOfMass().Z(), theExpected.CentreOfMass().Z(), theTolerance);

  const gp_Mat anActualInertia   = theActual.MatrixOfInertia();
  const gp_Mat anExpectedInertia = theExpected.MatrixOfInertia();
  for (int aRow = 1; aRow <= 3; ++aRow)
  {
    for (int aColumn = 1; aColumn <= 3; ++aColumn)
    {
      EXPECT_NEAR(anActualInertia.Value(aRow, aColumn),
                  anExpectedInertia.Value(aRow, aColumn),
                  theTolerance);
    }
  }
}

} // namespace

TEST(BRepGPropTest, VinertByPlane_BoxVolume_KeepsMassAndCentre)
{
  const gp_Dir            aNormal(0.0, 0.0, 1.0);
  BRepBuilderAPI_MakeFace aFaceBuilder(gp_Pln(gp_Pnt(0.0, 0.0, 2.0), aNormal), 0.0, 3.0, 0.0, 4.0);
  ASSERT_TRUE(aFaceBuilder.IsDone());
  const TopoDS_Face aFace = aFaceBuilder.Face();
  const gp_Pln      aReferencePlane(gp_Pnt(0.0, 0.0, 0.0), aNormal);
  const gp_Pnt      aLocation(0.0, 0.0, 0.0);

  // The volume between the face and the reference plane is a 3 x 4 x 2 block.
  GProp_GProps aBlock;
  BRepGProp::VolumeProperties(BRepPrimAPI_MakeBox(3.0, 4.0, 2.0).Shape(), aBlock);

  BRepGProp_Face   aFixedFace(aFace);
  BRepGProp_Domain aFixedDomain(aFace);
  BRepGProp_Vinert aFixed(aFixedFace, aFixedDomain, aReferencePlane, aLocation);
  EXPECT_NEAR(aFixed.Mass(), 24.0, 1.0e-10);
  EXPECT_NEAR(aFixed.CentreOfMass().X(), 1.5, 1.0e-10);
  EXPECT_NEAR(aFixed.CentreOfMass().Y(), 2.0, 1.0e-10);
  EXPECT_NEAR(aFixed.CentreOfMass().Z(), 1.0, 1.0e-10);
  expectGPropsNear(aFixed, aBlock, 1.0e-9);

  BRepGProp_Face   anAdaptiveFace(aFace);
  BRepGProp_Domain anAdaptiveDomain(aFace);
  BRepGProp_Vinert anAdaptive(anAdaptiveFace, anAdaptiveDomain, aReferencePlane, aLocation, 1.0e-8);
  EXPECT_NEAR(anAdaptive.Mass(), 24.0, 1.0e-10);
  EXPECT_NEAR(anAdaptive.CentreOfMass().X(), 1.5, 1.0e-10);
  EXPECT_NEAR(anAdaptive.CentreOfMass().Y(), 2.0, 1.0e-10);
  EXPECT_NEAR(anAdaptive.CentreOfMass().Z(), 1.0, 1.0e-10);
  expectGPropsNear(anAdaptive, aBlock, 1.0e-9);
}

namespace
{
// Circle of radius theR as a periodic rational BSpline with theNbSpans spans: the geometry
// is exact, only the number of polynomial pieces grows.
occ::handle<Geom_BSplineCurve> circleOfSpans(const gp_Ax2& thePos,
                                             const double  theR,
                                             const int     theNbSpans)
{
  occ::handle<Geom_BSplineCurve> aCurve =
    GeomConvert::CurveToBSplineCurve(new Geom_Circle(thePos, theR));
  const double aFirst = aCurve->FirstParameter(), aLast = aCurve->LastParameter();
  for (int i = 1; i < theNbSpans; ++i)
  {
    aCurve->InsertKnot(aFirst + (aLast - aFirst) * i / theNbSpans);
  }
  return aCurve;
}

TopoDS_Face circleFaceOfSpans(const double theR, const int theNbSpans)
{
  return BRepBuilderAPI_MakeFace(
    BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(
      circleOfSpans(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), theR, theNbSpans))),
    true);
}

// Regular polygon with theNbSides sides of circumradius theR as one degree 1 BSpline edge.
TopoDS_Face polygonFaceAsOneEdge(const double theR, const int theNbSides)
{
  NCollection_Array1<gp_Pnt> aPoles(1, theNbSides + 1);
  NCollection_Array1<double> aKnots(1, theNbSides + 1);
  NCollection_Array1<int>    aMults(1, theNbSides + 1);
  for (int i = 0; i <= theNbSides; ++i)
  {
    const double anAngle = 2 * M_PI * (i % theNbSides) / theNbSides;
    aPoles(i + 1)        = gp_Pnt(theR * std::cos(anAngle), theR * std::sin(anAngle), 0);
    aKnots(i + 1)        = i;
    aMults(i + 1)        = (i == 0 || i == theNbSides) ? 2 : 1;
  }
  occ::handle<Geom_BSplineCurve> aCurve = new Geom_BSplineCurve(aPoles, aKnots, aMults, 1);
  return BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(aCurve)), true);
}
} // namespace

// The boundary curve and the extrusion surface used to be integrated with one Gauss rule over
// the whole range: with many BSpline spans area and volume were wrong without any error.
TEST(BRepGPropTest, ManySpans_CircleAsBSpline_AreaAndVolume)
{
  const double aR = 5.0, aH = 3.0;
  for (const int aNbSpans : {4, 60, 300})
  {
    const TopoDS_Face  aFace  = circleFaceOfSpans(aR, aNbSpans);
    const TopoDS_Shape aPrism = BRepPrimAPI_MakePrism(aFace, gp_Vec(0, 0, aH)).Shape();
    const double       anArea = M_PI * aR * aR;

    GProp_GProps aSurf, aSurfEps, aVol, aVolEps;
    BRepGProp::SurfaceProperties(aFace, aSurf);
    BRepGProp::SurfaceProperties(aFace, aSurfEps, 1.0e-9);
    BRepGProp::VolumeProperties(aPrism, aVol);
    BRepGProp::VolumeProperties(aPrism, aVolEps, 1.0e-9);
    EXPECT_NEAR(aSurf.Mass(), anArea, 1.0e-7 * anArea) << aNbSpans << " spans";
    EXPECT_NEAR(aSurfEps.Mass(), anArea, 1.0e-7 * anArea) << aNbSpans << " spans";
    EXPECT_NEAR(aVol.Mass(), anArea * aH, 1.0e-7 * anArea * aH) << aNbSpans << " spans";
    EXPECT_NEAR(aVolEps.Mass(), anArea * aH, 1.0e-7 * anArea * aH) << aNbSpans << " spans";
    EXPECT_NEAR(aVol.CentreOfMass().Z(), 0.5 * aH, 1.0e-7) << aNbSpans << " spans";
  }
}

TEST(BRepGPropTest, ManySpans_PolygonAsOneBSplineEdge_AreaAndVolume)
{
  const double aR = 4.0, aH = 2.0;
  for (const int aNbSides : {6, 40, 200, 2500})
  {
    const TopoDS_Face  aFace  = polygonFaceAsOneEdge(aR, aNbSides);
    const TopoDS_Shape aPrism = BRepPrimAPI_MakePrism(aFace, gp_Vec(0, 0, aH)).Shape();
    const double       anArea = 0.5 * aNbSides * aR * aR * std::sin(2 * M_PI / aNbSides);

    GProp_GProps aSurf, aSurfEps, aVol, aVolEps;
    BRepGProp::SurfaceProperties(aFace, aSurf);
    BRepGProp::SurfaceProperties(aFace, aSurfEps, 1.0e-9);
    BRepGProp::VolumeProperties(aPrism, aVol);
    BRepGProp::VolumeProperties(aPrism, aVolEps, 1.0e-9);
    EXPECT_NEAR(aSurf.Mass(), anArea, 1.0e-7 * anArea) << aNbSides << " sides";
    // more spans than the adaptive path used to keep (SM subintervals)
    EXPECT_NEAR(aSurfEps.Mass(), anArea, 1.0e-7 * anArea) << aNbSides << " sides";
    EXPECT_NEAR(aVol.Mass(), anArea * aH, 1.0e-7 * anArea * aH) << aNbSides << " sides";
    EXPECT_NEAR(aVolEps.Mass(), anArea * aH, 1.0e-7 * anArea * aH) << aNbSides << " sides";
  }
}

// The span bounds of the surface in V are crossed by the seam of the torus: the integral along
// the boundary must be split there as well.
TEST(BRepGPropTest, ManySpans_TorusOfBSplineCircle_Volume)
{
  const double aRc = 10.0, aR = 3.0;
  for (const int aNbSpans : {4, 60, 300})
  {
    const occ::handle<Geom_BSplineCurve> aProfile =
      circleOfSpans(gp_Ax2(gp_Pnt(aRc, 0, 0), gp_Dir(0, 1, 0)), aR, aNbSpans);
    const TopoDS_Face aFace =
      BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(aProfile)), true);
    const TopoDS_Shape aTorus =
      BRepPrimAPI_MakeRevol(aFace, gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1))).Shape();
    const double aVolume = 2 * M_PI * M_PI * aRc * aR * aR;

    GProp_GProps aVol, aVolEps;
    BRepGProp::VolumeProperties(aTorus, aVol);
    BRepGProp::VolumeProperties(aTorus, aVolEps, 1.0e-9);
    EXPECT_NEAR(aVol.Mass(), aVolume, 1.0e-7 * aVolume) << aNbSpans << " spans";
    // the adaptive path stops at about 2e-7 on this revolution surface whatever the spans
    EXPECT_NEAR(aVolEps.Mass(), aVolume, 1.0e-6 * aVolume) << aNbSpans << " spans";
  }
}

// Face without wires (natural restriction) on an extrusion of a circle with many spans.
TEST(BRepGPropTest, ManySpans_NaturalRestrictionOnExtrusion_Area)
{
  const double aR = 2.0, aH = 5.0;
  for (const int aNbSpans : {4, 300})
  {
    const occ::handle<Geom_BSplineCurve> aCurve =
      circleOfSpans(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), aR, aNbSpans);
    const occ::handle<Geom_Surface> aSurf =
      new Geom_RectangularTrimmedSurface(new Geom_SurfaceOfLinearExtrusion(aCurve, gp_Dir(0, 0, 1)),
                                         aCurve->FirstParameter(),
                                         aCurve->LastParameter(),
                                         0.0,
                                         aH);
    TopoDS_Face  aFace;
    BRep_Builder aBuilder;
    aBuilder.MakeFace(aFace, aSurf, Precision::Confusion());
    const double anArea = 2 * M_PI * aR * aH;

    GProp_GProps aSurfProps, aSurfEps;
    BRepGProp::SurfaceProperties(aFace, aSurfProps);
    BRepGProp::SurfaceProperties(aFace, aSurfEps, 1.0e-9);
    EXPECT_NEAR(aSurfProps.Mass(), anArea, 1.0e-7 * anArea) << aNbSpans << " spans";
    EXPECT_NEAR(aSurfEps.Mass(), anArea, 1.0e-7 * anArea) << aNbSpans << " spans";
  }
}

// Moment of inertia and orientation with span by span integration.
TEST(BRepGPropTest, ManySpans_CylinderAsPrism_InertiaAndReversed)
{
  const double       aR = 5.0, aH = 3.0;
  const TopoDS_Shape aPrism =
    BRepPrimAPI_MakePrism(circleFaceOfSpans(aR, 300), gp_Vec(0, 0, aH)).Shape();
  const double aVolume = M_PI * aR * aR * aH;

  GProp_GProps aVol, aVolRev;
  BRepGProp::VolumeProperties(aPrism, aVol);
  BRepGProp::VolumeProperties(aPrism.Reversed(), aVolRev);
  const gp_Mat anInertia = aVol.MatrixOfInertia();
  EXPECT_NEAR(anInertia(3, 3), 0.5 * aVolume * aR * aR, 1.0e-7 * aVolume * aR * aR);
  EXPECT_NEAR(anInertia(1, 1), aVolume * (aR * aR / 4 + aH * aH / 12), 1.0e-7 * aVolume * aR * aR);
  EXPECT_NEAR(aVol.CentreOfMass().X(), 0.0, 1.0e-7);
  EXPECT_NEAR(aVolRev.Mass(), -aVolume, 1.0e-7 * aVolume);
}

// The non-adaptive Gauss integration of the volume between the faces of a shape and a plane always
// gave a null mass: the mass was kept only for the volume relative to a point.
TEST(BRepGPropTest, VolumeRelativeToPlane_Gauss)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(gp_Pnt(1.0, 2.0, 3.0), 10.0, 20.0, 30.0).Shape();
  const gp_Pln       aPlane(gp_Pnt(0.0, 0.0, -5.0), gp_Dir(0.0, 0.0, 1.0));

  double aMass = 0.0;
  gp_XYZ aMoment(0.0, 0.0, 0.0);
  for (TopExp_Explorer anExp(aBox, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    const BRepGProp_Face   aFace(TopoDS::Face(anExp.Current()));
    const BRepGProp_Vinert aVinert(aFace, aPlane, gp::Origin());
    aMass += aVinert.Mass();
    aMoment += aVinert.CentreOfMass().XYZ() * aVinert.Mass();
  }
  EXPECT_NEAR(aMass, 6000.0, 1e-6);

  // guard: the adaptive integration gives the same volume and centre
  GProp_GProps aProps;
  BRepGProp::VolumePropertiesGK(aBox, aProps, aPlane, 0.001, false, false, true);
  EXPECT_NEAR(aProps.Mass(), 6000.0, 1e-6);
  EXPECT_NEAR((aMoment / aMass - aProps.CentreOfMass().XYZ()).Modulus(), 0.0, 1e-6);
}
