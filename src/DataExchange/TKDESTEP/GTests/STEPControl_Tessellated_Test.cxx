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

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_NurbsConvert.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <DESTEP_Parameters.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomLProp_SLProps.hxx>
#include <Geom_Surface.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax2.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <sstream>

namespace
{

//! Meshes read back from STEP: number of triangles, triangles not oriented along the outer
//! normal of their face, largest distance of a node to the surface of its face.
struct MeshState
{
  int    NbFaces       = 0;
  int    NbMeshedFaces = 0;
  int    NbTriangles   = 0;
  int    NbReversed    = 0;
  double MaxNodeDist   = 0.0;
};

MeshState meshState(const TopoDS_Shape& theShape)
{
  MeshState aState;
  for (TopExp_Explorer anExp(theShape, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    const TopoDS_Face& aFace = TopoDS::Face(anExp.Current());
    ++aState.NbFaces;
    TopLoc_Location                        aLoc;
    const occ::handle<Poly_Triangulation>& aTri = BRep_Tool::Triangulation(aFace, aLoc);
    if (aTri.IsNull())
    {
      continue;
    }
    ++aState.NbMeshedFaces;
    aState.NbTriangles += aTri->NbTriangles();
    const gp_Trsf                   aTrsf = aLoc.Transformation();
    const occ::handle<Geom_Surface> aSurf = BRep_Tool::Surface(aFace);
    for (int aNodeIter = 1; aNodeIter <= aTri->NbNodes(); ++aNodeIter)
    {
      GeomAPI_ProjectPointOnSurf aProj(aTri->Node(aNodeIter).Transformed(aTrsf), aSurf);
      if (aProj.NbPoints() > 0)
      {
        aState.MaxNodeDist = std::max(aState.MaxNodeDist, aProj.LowerDistance());
      }
    }
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
      GeomAPI_ProjectPointOnSurf aProj(gp_Pnt((aP1.XYZ() + aP2.XYZ() + aP3.XYZ()) / 3.0), aSurf);
      if (aProj.NbPoints() == 0)
      {
        continue;
      }
      double aU = 0.0, aV = 0.0;
      aProj.LowerDistanceParameters(aU, aV);
      GeomLProp_SLProps aProps(aSurf, aU, aV, 1, 1.0e-9);
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
  }
  return aState;
}

int nbTriangles(const TopoDS_Shape& theShape)
{
  int aNb = 0;
  for (TopExp_Explorer anExp(theShape, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    TopLoc_Location                        aLoc;
    const occ::handle<Poly_Triangulation>& aTri =
      BRep_Tool::Triangulation(TopoDS::Face(anExp.Current()), aLoc);
    aNb += aTri.IsNull() ? 0 : aTri->NbTriangles();
  }
  return aNb;
}

//! Filleted box with a cylindrical hole, meshed: the fillets give indirect cylinders and spheres,
//! redressed by the shape processing of the writer (DirectFaces).
TopoDS_Shape meshedShape()
{
  const TopoDS_Shape       aBox = BRepPrimAPI_MakeBox(100.0, 60.0, 30.0).Shape();
  BRepFilletAPI_MakeFillet aFillet(aBox);
  for (TopExp_Explorer anExp(aBox, TopAbs_EDGE); anExp.More(); anExp.Next())
  {
    aFillet.Add(3.0, TopoDS::Edge(anExp.Current()));
  }
  const TopoDS_Shape aShape =
    BRepAlgoAPI_Cut(
      aFillet.Shape(),
      BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(50.0, 30.0, -5.0), gp::DZ()), 10.0, 40.0).Shape())
      .Shape();
  BRepMesh_IncrementalMesh aMesher(aShape, 0.1);
  return aShape;
}

std::string writeStep(const TopoDS_Shape&                         theShape,
                      const DESTEP_Parameters::RWMode_Tessellated theMode)
{
  STEPControl_Writer aWriter;
  DESTEP_Parameters  aParams;
  aParams.InitFromStatic();
  aParams.WriteTessellated = theMode;
  EXPECT_EQ(aWriter.Transfer(theShape, STEPControl_AsIs, aParams), IFSelect_RetDone);
  std::ostringstream aStream;
  EXPECT_EQ(aWriter.WriteStream(aStream), IFSelect_RetDone);
  return aStream.str();
}

TopoDS_Shape readStep(const std::string&                          theContent,
                      const DESTEP_Parameters::RWMode_Tessellated theMode)
{
  STEPControl_Reader aReader;
  DESTEP_Parameters  aParams;
  aParams.InitFromStatic();
  aParams.ReadTessellated = theMode;
  std::istringstream aStream(theContent);
  EXPECT_EQ(aReader.ReadStream("tessellated.stp", aParams, aStream), IFSelect_RetDone);
  aReader.TransferRoots();
  return aReader.OneShape();
}

//! Writes theShape with its tessellation, reads it back and checks that every face has its mesh
//! at the right place and with the right orientation.
void checkRoundTrip(const TopoDS_Shape& theShape)
{
  const int aNbSource = nbTriangles(theShape);
  ASSERT_GT(aNbSource, 0);
  const TopoDS_Shape aRead = readStep(writeStep(theShape, DESTEP_Parameters::RWMode_Tessellated_On),
                                      DESTEP_Parameters::RWMode_Tessellated_On);
  const MeshState    aState = meshState(aRead);
  EXPECT_GT(aState.NbFaces, 0);
  EXPECT_EQ(aState.NbMeshedFaces, aState.NbFaces);
  EXPECT_EQ(aState.NbTriangles, aNbSource);
  EXPECT_EQ(aState.NbReversed, 0);
  EXPECT_LT(aState.MaxNodeDist, 1.0e-6);
}

} // namespace

// write.step.tessellated On writes the mesh of every face, also the faces redressed by the
// shape processing of the writer (DirectFaces).
TEST(STEPControl_TessellatedTest, WriteOn_AllFacesKeepTheirMesh)
{
  checkRoundTrip(meshedShape());
}

// The nodes are written in the frame of the face location, like the geometry.
TEST(STEPControl_TessellatedTest, WriteOn_LocatedShape)
{
  gp_Trsf aMove;
  aMove.SetTranslation(gp_Vec(50.0, 0.0, 0.0));
  checkRoundTrip(meshedShape().Moved(TopLoc_Location(aMove)));
}

// A mirror location reverses the placement of the elementary surfaces, which STEP writes as
// direct ones: the triangles of these faces are reversed with them.
TEST(STEPControl_TessellatedTest, WriteOn_MirroredLocation)
{
  gp_Trsf aMirror;
  aMirror.SetMirror(gp_Ax2(gp::Origin(), gp::DX()));
  checkRoundTrip(meshedShape().Moved(TopLoc_Location(aMirror)));
}

// The poles of a B-spline surface are written as they are: with a mirror location its normal is
// mirrored with the triangles, which are not reversed.
TEST(STEPControl_TessellatedTest, WriteOn_MirroredLocation_BSpline)
{
  const TopoDS_Shape aNurbs =
    BRepBuilderAPI_NurbsConvert(BRepPrimAPI_MakeBox(100.0, 60.0, 30.0).Shape()).Shape();
  BRepMesh_IncrementalMesh aMesher(aNurbs, 0.5);
  gp_Trsf                  aMirror;
  aMirror.SetMirror(gp_Ax2(gp::Origin(), gp::DX()));
  checkRoundTrip(aNurbs.Moved(TopLoc_Location(aMirror)));
}

// read.step.tessellated Off does not read the tessellation written with the B-rep, OnNoBRep
// reads it only for the faces without B-rep.
TEST(STEPControl_TessellatedTest, ReadOffAndOnNoBRep_NoMeshOnBRepFaces)
{
  const std::string aContent = writeStep(meshedShape(), DESTEP_Parameters::RWMode_Tessellated_On);
  for (const DESTEP_Parameters::RWMode_Tessellated aMode :
       {DESTEP_Parameters::RWMode_Tessellated_Off, DESTEP_Parameters::RWMode_Tessellated_OnNoBRep})
  {
    const MeshState aState = meshState(readStep(aContent, aMode));
    EXPECT_EQ(aState.NbFaces, 27) << "mode " << aMode;
    EXPECT_EQ(aState.NbMeshedFaces, 0) << "mode " << aMode;
  }
}

// Guard: the default writing mode (OnNoBRep) writes no tessellation for a shape with B-rep.
TEST(STEPControl_TessellatedTest, WriteOnNoBRep_NoMesh)
{
  const MeshState aState =
    meshState(readStep(writeStep(meshedShape(), DESTEP_Parameters::RWMode_Tessellated_OnNoBRep),
                       DESTEP_Parameters::RWMode_Tessellated_On));
  EXPECT_EQ(aState.NbFaces, 27);
  EXPECT_EQ(aState.NbMeshedFaces, 0);
}
