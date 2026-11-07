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

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Polygon3D.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <TopoDS_Shape.hxx>

#include <gtest/gtest.h>

TEST(BRepBuilderAPI_TransformTest, Translate)
{
  BRepPrimAPI_MakeBox aMakeBox(10.0, 10.0, 10.0);
  const TopoDS_Shape& aBox = aMakeBox.Shape();
  ASSERT_TRUE(aMakeBox.IsDone()) << "Box creation failed";

  gp_Trsf aTrsf;
  aTrsf.SetTranslation(gp_Vec(100.0, 0.0, 0.0));

  BRepBuilderAPI_Transform aTransform(aBox, aTrsf);
  const TopoDS_Shape&      aResult = aTransform.Shape();
  ASSERT_FALSE(aResult.IsNull()) << "Transformed shape is null";

  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aResult, aProps);
  gp_Pnt aCenterOfMass = aProps.CentreOfMass();

  EXPECT_NEAR(aCenterOfMass.X(), 105.0, Precision::Confusion())
    << "Center of mass X should be shifted by 100";
  EXPECT_NEAR(aCenterOfMass.Y(), 5.0, Precision::Confusion());
  EXPECT_NEAR(aCenterOfMass.Z(), 5.0, Precision::Confusion());
}

TEST(BRepBuilderAPI_TransformTest, Rotate)
{
  BRepPrimAPI_MakeBox aMakeBox(10.0, 10.0, 10.0);
  const TopoDS_Shape& aBox = aMakeBox.Shape();
  ASSERT_TRUE(aMakeBox.IsDone()) << "Box creation failed";

  gp_Trsf aTrsf;
  gp_Ax1  aZAxis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
  aTrsf.SetRotation(aZAxis, M_PI / 2.0);

  BRepBuilderAPI_Transform aTransform(aBox, aTrsf);
  const TopoDS_Shape&      aResult = aTransform.Shape();
  ASSERT_FALSE(aResult.IsNull()) << "Transformed shape is null";

  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aResult, aProps);
  gp_Pnt aCenterOfMass = aProps.CentreOfMass();

  EXPECT_NEAR(aCenterOfMass.X(), -5.0, Precision::Confusion())
    << "Center of mass X should be ~-5 after 90deg rotation";
  EXPECT_NEAR(aCenterOfMass.Y(), 5.0, Precision::Confusion())
    << "Center of mass Y should be ~5 after 90deg rotation";
}

TEST(BRepBuilderAPI_TransformTest, Scale)
{
  BRepPrimAPI_MakeBox aMakeBox(10.0, 10.0, 10.0);
  const TopoDS_Shape& aBox = aMakeBox.Shape();
  ASSERT_TRUE(aMakeBox.IsDone()) << "Box creation failed";

  gp_Trsf aTrsf;
  aTrsf.SetScale(gp_Pnt(0.0, 0.0, 0.0), 2.0);

  BRepBuilderAPI_Transform aTransform(aBox, aTrsf);
  const TopoDS_Shape&      aResult = aTransform.Shape();
  ASSERT_FALSE(aResult.IsNull()) << "Transformed shape is null";

  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aResult, aProps);
  double aVolume = aProps.Mass();

  EXPECT_NEAR(aVolume, 8000.0, Precision::Confusion())
    << "Volume should be 8x original (2^3 * 1000)";
}

TEST(BRepBuilderAPI_TransformTest, Mirror)
{
  BRepPrimAPI_MakeBox aMakeBox(gp_Pnt(10.0, 0.0, 0.0), 10.0, 10.0, 10.0);
  const TopoDS_Shape& aBox = aMakeBox.Shape();
  ASSERT_TRUE(aMakeBox.IsDone()) << "Box creation failed";

  gp_Trsf aTrsf;
  gp_Ax2  aYZPlane(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0));
  aTrsf.SetMirror(aYZPlane);

  BRepBuilderAPI_Transform aTransform(aBox, aTrsf);
  const TopoDS_Shape&      aResult = aTransform.Shape();
  ASSERT_FALSE(aResult.IsNull()) << "Transformed shape is null";

  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aResult, aProps);
  gp_Pnt aCenterOfMass = aProps.CentreOfMass();

  EXPECT_LT(aCenterOfMass.X(), 0.0)
    << "Center of mass X should be negative after mirroring through YZ plane";
}

TEST(BRepBuilderAPI_TransformTest, ShapeValidity)
{
  BRepPrimAPI_MakeBox aMakeBox(10.0, 10.0, 10.0);
  const TopoDS_Shape& aBox = aMakeBox.Shape();
  ASSERT_TRUE(aMakeBox.IsDone()) << "Box creation failed";

  gp_Trsf aTrsf;
  aTrsf.SetTranslation(gp_Vec(50.0, 50.0, 50.0));

  BRepBuilderAPI_Transform aTransform(aBox, aTrsf);
  const TopoDS_Shape&      aResult = aTransform.Shape();
  ASSERT_FALSE(aResult.IsNull()) << "Transformed shape is null";

  BRepCheck_Analyzer anAnalyzer(aResult);
  EXPECT_TRUE(anAnalyzer.IsValid()) << "Transformed shape should be valid";
}

namespace
{
// Face holding only a triangulation (as read from STL): a tetrahedron with outward normals.
TopoDS_Face meshOnlyFace()
{
  occ::handle<Poly_Triangulation> aTri = new Poly_Triangulation(4, 4, false);
  aTri->SetNode(1, gp_Pnt(0, 0, 0));
  aTri->SetNode(2, gp_Pnt(10, 0, 0));
  aTri->SetNode(3, gp_Pnt(0, 10, 0));
  aTri->SetNode(4, gp_Pnt(0, 0, 10));
  aTri->SetTriangle(1, Poly_Triangle(1, 3, 2));
  aTri->SetTriangle(2, Poly_Triangle(1, 2, 4));
  aTri->SetTriangle(3, Poly_Triangle(1, 4, 3));
  aTri->SetTriangle(4, Poly_Triangle(2, 3, 4));
  aTri->AddNormals();
  aTri->SetNormal(1, gp_Dir(-1, -1, -1));
  aTri->SetNormal(2, gp_Dir(1, 0, 0));
  aTri->SetNormal(3, gp_Dir(0, 1, 0));
  aTri->SetNormal(4, gp_Dir(0, 0, 1));
  TopoDS_Face aFace;
  BRep_Builder().MakeFace(aFace, aTri);
  return aFace;
}

// Signed volume of the triangulation of theFace with its location and orientation.
double meshVolume(const TopoDS_Face& theFace)
{
  TopLoc_Location                        aLoc;
  const occ::handle<Poly_Triangulation>& aTri = BRep_Tool::Triangulation(theFace, aLoc);
  double                                 aVol = 0.0;
  for (int i = 1; i <= aTri->NbTriangles(); ++i)
  {
    int n1, n2, n3;
    aTri->Triangle(i).Get(n1, n2, n3);
    const gp_XYZ p1 = aTri->Node(n1).Transformed(aLoc.Transformation()).XYZ();
    const gp_XYZ p2 = aTri->Node(n2).Transformed(aLoc.Transformation()).XYZ();
    const gp_XYZ p3 = aTri->Node(n3).Transformed(aLoc.Transformation()).XYZ();
    aVol += p1.Dot(p2.Crossed(p3)) / 6.0;
  }
  return theFace.Orientation() == TopAbs_REVERSED ? -aVol : aVol;
}
} // namespace

// The triangulation was the only geometry of such a face and was copied untransformed unless the
// copy of meshes was requested: the shape came back unchanged without any error.
TEST(BRepBuilderAPI_TransformTest, MeshOnlyFace_TransformedWithoutCopyMesh)
{
  const TopoDS_Face aFace   = meshOnlyFace();
  const double      aVolume = meshVolume(aFace);
  ASSERT_NEAR(aVolume, 1000.0 / 6.0, 1.0e-9);

  gp_Trsf aScale, aMirror, aRigid;
  aScale.SetScale(gp_Pnt(1, 2, 3), 25.4);
  aMirror.SetMirror(gp_Ax2(gp_Pnt(3, 0, 0), gp_Dir(1, 0, 0)));
  aRigid.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(1, 1, 1)), 0.7);
  aRigid.SetTranslationPart(gp_Vec(5, -3, 2));
  for (const gp_Trsf& aTrsf : {aScale, aMirror, aRigid})
  {
    const double       aFactor = std::abs(aTrsf.ScaleFactor());
    const TopoDS_Shape aRes    = BRepBuilderAPI_Transform(aFace, aTrsf, true).Shape();
    TopExp_Explorer    anExp(aRes, TopAbs_FACE);
    ASSERT_TRUE(anExp.More());
    const TopoDS_Face                      aResFace = TopoDS::Face(anExp.Current());
    TopLoc_Location                        aLoc;
    const occ::handle<Poly_Triangulation>& aTri = BRep_Tool::Triangulation(aResFace, aLoc);
    ASSERT_FALSE(aTri.IsNull());
    const gp_Pnt anExpected = gp_Pnt(0, 0, 10).Transformed(aTrsf);
    EXPECT_LT(aTri->Node(4).Transformed(aLoc.Transformation()).Distance(anExpected), 1.0e-9);
    // volume scaled and still positive (orientation kept for a mirror)
    const double anExpectedVolume = aVolume * aFactor * aFactor * aFactor;
    EXPECT_NEAR(meshVolume(aResFace), anExpectedVolume, 1.0e-9 * anExpectedVolume);
  }
}

// Same for an edge holding only a 3D polygon.
TEST(BRepBuilderAPI_TransformTest, PolygonOnlyEdge_TransformedWithoutCopyMesh)
{
  NCollection_Array1<gp_Pnt> aNodes(1, 2);
  aNodes(1) = gp_Pnt(0, 0, 0);
  aNodes(2) = gp_Pnt(10, 0, 0);
  TopoDS_Edge anEdge;
  BRep_Builder().MakeEdge(anEdge, new Poly_Polygon3D(aNodes));
  gp_Trsf aScale;
  aScale.SetScale(gp_Pnt(0, 0, 0), 2.0);
  const TopoDS_Shape                 aRes = BRepBuilderAPI_Transform(anEdge, aScale, true).Shape();
  TopLoc_Location                    aLoc;
  const occ::handle<Poly_Polygon3D>& aPoly = BRep_Tool::Polygon3D(TopoDS::Edge(aRes), aLoc);
  ASSERT_FALSE(aPoly.IsNull());
  EXPECT_LT(aPoly->Nodes()(2).Transformed(aLoc.Transformation()).Distance(gp_Pnt(20, 0, 0)),
            1.0e-9);
}
