// Created on: 1995-08-31
// Created by: Remi LEQUETTE
// Copyright (c) 1995-1999 Matra Datavision
// Copyright (c) 1999-2014 OPEN CASCADE SAS
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

#include <BRepFilletAPI_MakeFillet2d.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>

//=================================================================================================

BRepFilletAPI_MakeFillet2d::BRepFilletAPI_MakeFillet2d() = default;

//=================================================================================================

BRepFilletAPI_MakeFillet2d::BRepFilletAPI_MakeFillet2d(const TopoDS_Face& F)
    : myInitialFace(F)
{
  myMakeChFi2d.Init(F);
}

//=================================================================================================

void BRepFilletAPI_MakeFillet2d::Init(const TopoDS_Face& F)
{
  myInitialFace = F;
  clearHistory();
  myMakeChFi2d.Init(F);
}

//=================================================================================================

void BRepFilletAPI_MakeFillet2d::Init(const TopoDS_Face& RefFace, const TopoDS_Face& ModFace)
{
  myInitialFace = RefFace;
  clearHistory();
  myMakeChFi2d.Init(RefFace, ModFace);
}

//=================================================================================================

TopoDS_Edge BRepFilletAPI_MakeFillet2d::AddFillet(const TopoDS_Vertex& V, const double Radius)
{
  const TopoDS_Edge aFillet = myMakeChFi2d.AddFillet(V, Radius);
  recordCorner(aFillet, V);
  return aFillet;
}

//=================================================================================================

TopoDS_Edge BRepFilletAPI_MakeFillet2d::ModifyFillet(const TopoDS_Edge& Fillet, const double Radius)
{
  // Fillet may refer to an item of FilletEdges(), which the builder removes: keep a copy
  const TopoDS_Edge anOld   = Fillet;
  const TopoDS_Edge aFillet = myMakeChFi2d.ModifyFillet(anOld, Radius);
  moveCorner(anOld, aFillet);
  return aFillet;
}

//=================================================================================================

TopoDS_Vertex BRepFilletAPI_MakeFillet2d::RemoveFillet(const TopoDS_Edge& Fillet)
{
  // Fillet may refer to an item of FilletEdges(), which the builder removes: keep a copy
  const TopoDS_Edge   anOld   = Fillet;
  const TopoDS_Vertex aVertex = myMakeChFi2d.RemoveFillet(anOld);
  moveCorner(anOld, TopoDS_Edge());
  return aVertex;
}

//=================================================================================================

TopoDS_Edge BRepFilletAPI_MakeFillet2d::AddChamfer(const TopoDS_Edge& E1,
                                                   const TopoDS_Edge& E2,
                                                   const double       D1,
                                                   const double       D2)
{
  TopoDS_Vertex aCorner;
  TopExp::CommonVertex(E1, E2, aCorner);
  const TopoDS_Edge aChamfer = myMakeChFi2d.AddChamfer(E1, E2, D1, D2);
  recordCorner(aChamfer, aCorner);
  return aChamfer;
}

//=================================================================================================

TopoDS_Edge BRepFilletAPI_MakeFillet2d::AddChamfer(const TopoDS_Edge&   E,
                                                   const TopoDS_Vertex& V,
                                                   const double         D,
                                                   const double         Ang)
{
  const TopoDS_Edge aChamfer = myMakeChFi2d.AddChamfer(E, V, D, Ang);
  recordCorner(aChamfer, V);
  return aChamfer;
}

//=================================================================================================

TopoDS_Edge BRepFilletAPI_MakeFillet2d::ModifyChamfer(const TopoDS_Edge& Chamfer,
                                                      const TopoDS_Edge& E1,
                                                      const TopoDS_Edge& E2,
                                                      const double       D1,
                                                      const double       D2)
{
  // Chamfer may refer to an item of ChamferEdges(), which the builder removes: keep a copy
  const TopoDS_Edge anOld    = Chamfer;
  const TopoDS_Edge aChamfer = myMakeChFi2d.ModifyChamfer(anOld, E1, E2, D1, D2);
  moveCorner(anOld, aChamfer);
  return aChamfer;
}

//=================================================================================================

TopoDS_Edge BRepFilletAPI_MakeFillet2d::ModifyChamfer(const TopoDS_Edge& Chamfer,
                                                      const TopoDS_Edge& E,
                                                      const double       D,
                                                      const double       Ang)
{
  // Chamfer may refer to an item of ChamferEdges(), which the builder removes: keep a copy
  const TopoDS_Edge anOld    = Chamfer;
  const TopoDS_Edge aChamfer = myMakeChFi2d.ModifyChamfer(anOld, E, D, Ang);
  moveCorner(anOld, aChamfer);
  return aChamfer;
}

//=================================================================================================

TopoDS_Vertex BRepFilletAPI_MakeFillet2d::RemoveChamfer(const TopoDS_Edge& Chamfer)
{
  // Chamfer may refer to an item of ChamferEdges(), which the builder removes: keep a copy
  const TopoDS_Edge   anOld   = Chamfer;
  const TopoDS_Vertex aVertex = myMakeChFi2d.RemoveChamfer(anOld);
  moveCorner(anOld, TopoDS_Edge());
  return aVertex;
}

//=================================================================================================

const TopoDS_Edge& BRepFilletAPI_MakeFillet2d::BasisEdge(const TopoDS_Edge& E) const
{
  return myMakeChFi2d.BasisEdge(E);
}

//=================================================================================================

void BRepFilletAPI_MakeFillet2d::Build(const Message_ProgressRange& /*theRange*/)
{
  // test if the operation is done
  myInitialShapes.Clear();
  myResultShapes.Clear();
  myGeneratedFrom.Clear();
  myModifiedEdges.Clear();
  if (Status() == ChFi2d_IsDone)
  {
    Done();
    myShape = myMakeChFi2d.Result();
    for (const TopAbs_ShapeEnum aType : {TopAbs_FACE, TopAbs_EDGE, TopAbs_VERTEX})
    {
      TopExp::MapShapes(myInitialFace, aType, myInitialShapes);
      TopExp::MapShapes(myShape, aType, myResultShapes);
    }
    for (int i = 1; i <= myInitialShapes.Extent(); ++i)
    {
      const TopoDS_Shape& aShape = myInitialShapes(i);
      if (aShape.ShapeType() == TopAbs_EDGE && HasDescendant(TopoDS::Edge(aShape)))
      {
        myModifiedEdges.Add(aShape);
      }
    }
    // A fillet or chamfer of the result is generated from the vertex of the initial face it
    // replaces.
    for (NCollection_DataMap<TopoDS_Shape, TopoDS_Shape, TopTools_ShapeMapHasher>::Iterator anIt(
           myCorners);
         anIt.More();
         anIt.Next())
    {
      const TopoDS_Shape& aCorner = anIt.Value();
      if (myResultShapes.Contains(anIt.Key()) && myInitialShapes.Contains(aCorner)
          && !myResultShapes.Contains(aCorner))
      {
        NCollection_List<TopoDS_Shape>* aList = myGeneratedFrom.ChangeSeek(aCorner);
        if (aList == nullptr)
        {
          aList = myGeneratedFrom.Bound(aCorner, NCollection_List<TopoDS_Shape>());
        }
        aList->Append(anIt.Key());
      }
    }
  }
  else
  {
    NotDone();
  }
}

//=================================================================================================

const NCollection_List<TopoDS_Shape>& BRepFilletAPI_MakeFillet2d::Modified(const TopoDS_Shape& S)
{
  myGenerated.Clear();
  if (S.IsNull())
  {
    return myGenerated;
  }
  if (S.ShapeType() == TopAbs_FACE)
  {
    if (IsDone() && myInitialShapes.Contains(S) && !myShape.IsSame(S))
    {
      myGenerated.Append(myShape);
    }
  }
  else if (S.ShapeType() == TopAbs_EDGE && HasDescendant(TopoDS::Edge(S)))
  {
    myGenerated.Append(DescendantEdge(TopoDS::Edge(S)));
  }
  return myGenerated;
}

//=================================================================================================

const NCollection_List<TopoDS_Shape>& BRepFilletAPI_MakeFillet2d::Generated(const TopoDS_Shape& S)
{
  myGenerated.Clear();
  if (IsDone())
  {
    if (const NCollection_List<TopoDS_Shape>* aList = myGeneratedFrom.Seek(S))
    {
      myGenerated = *aList;
    }
  }
  return myGenerated;
}

//=================================================================================================

void BRepFilletAPI_MakeFillet2d::recordCorner(const TopoDS_Edge&   theNewEdge,
                                              const TopoDS_Vertex& theCorner)
{
  if (!theNewEdge.IsNull() && !theCorner.IsNull() && Status() == ChFi2d_IsDone)
  {
    myCorners.Bind(theNewEdge, theCorner);
  }
}

//=================================================================================================

void BRepFilletAPI_MakeFillet2d::moveCorner(const TopoDS_Edge& theOldEdge,
                                            const TopoDS_Edge& theNewEdge)
{
  // Nothing changes if the old edge was not removed (e.g. a chamfer given to RemoveFillet).
  for (const NCollection_Sequence<TopoDS_Shape>* aNewEdges : {&FilletEdges(), &ChamferEdges()})
  {
    for (const TopoDS_Shape& aNewEdge : *aNewEdges)
    {
      if (aNewEdge.IsSame(theOldEdge))
      {
        return;
      }
    }
  }
  TopoDS_Shape aCorner;
  if (myCorners.Find(theOldEdge, aCorner))
  {
    myCorners.UnBind(theOldEdge);
    if (!theNewEdge.IsNull())
    {
      recordCorner(theNewEdge, TopoDS::Vertex(aCorner));
    }
  }
}

//=================================================================================================

void BRepFilletAPI_MakeFillet2d::clearHistory()
{
  myCorners.Clear();
  myInitialShapes.Clear();
  myResultShapes.Clear();
  myGeneratedFrom.Clear();
  myModifiedEdges.Clear();
}

//=================================================================================================

bool BRepFilletAPI_MakeFillet2d::IsDeleted(const TopoDS_Shape& S)
{
  if (!IsDone() || S.IsNull() || !myInitialShapes.Contains(S) || myResultShapes.Contains(S))
  {
    return false;
  }
  if (S.ShapeType() == TopAbs_FACE)
  {
    return false; // the initial face is modified into the result
  }
  return !myModifiedEdges.Contains(S);
}

//=================================================================================================

int BRepFilletAPI_MakeFillet2d::NbCurves() const
{
  return NbFillet() + NbChamfer();
}

//=================================================================================================

const NCollection_List<TopoDS_Shape>& BRepFilletAPI_MakeFillet2d::NewEdges(const int I)
{
  myGenerated.Clear();
  if (I <= NbFillet())
  {
    myGenerated.Append(FilletEdges()(I));
  }
  else
  {
    myGenerated.Append(ChamferEdges()(I - NbFillet()));
  }

  return myGenerated;
}
