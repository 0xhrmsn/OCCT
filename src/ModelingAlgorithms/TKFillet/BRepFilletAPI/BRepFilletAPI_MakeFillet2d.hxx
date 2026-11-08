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

#ifndef _BRepFilletAPI_MakeFillet2d_HeaderFile
#define _BRepFilletAPI_MakeFillet2d_HeaderFile

#include <Standard.hxx>
#include <Standard_DefineAlloc.hxx>
#include <Standard_Handle.hxx>

#include <ChFi2d_Builder.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <TopoDS_Shape.hxx>
#include <NCollection_Sequence.hxx>
#include <NCollection_List.hxx>
#include <ChFi2d_ConstructionError.hxx>
#include <NCollection_DataMap.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Face.hxx>
class TopoDS_Edge;
class TopoDS_Vertex;
class TopoDS_Shape;

// resolve name collisions with X11 headers
#ifdef Status
  #undef Status
#endif

//! Describes functions to build fillets and chamfers on the
//! vertices of a planar face.
//! Fillets and Chamfers on the Vertices of a Planar Face
//! A MakeFillet2d object provides a framework for:
//! - initializing the construction algorithm with a given face,
//! - acquiring the data characterizing the fillets and chamfers,
//! -   building the fillets and chamfers, and constructing the
//! resulting shape, and
//! -   consulting the result.
//! Warning
//! Only segments of straight lines and arcs of circles are
//! treated. BSplines are not processed.
class BRepFilletAPI_MakeFillet2d : public BRepBuilderAPI_MakeShape
{
public:
  DEFINE_STANDARD_ALLOC

  //! Initializes an empty algorithm for computing fillets and
  //! chamfers. The face on which the fillets and
  //! chamfers are built is defined using the Init function.
  //! The vertices on which fillets or chamfers are built are
  //! defined using the AddFillet or AddChamfer function.
  //! Warning
  //! The status of the initialization, as given by the Status
  //! function, can be one of the following:
  //! -   ChFi2d_Ready if the initialization is correct,
  //! -   ChFi2d_NotPlanar if F is not planar,
  //! -   ChFi2d_NoFace if F is a null face.
  Standard_EXPORT BRepFilletAPI_MakeFillet2d();

  //! Initializes an algorithm for computing fillets and chamfers on the face F.
  //! The vertices on which fillets or chamfers are built are
  //! defined using the AddFillet or AddChamfer function.
  //! Warning
  //! The status of the initialization, as given by the Status
  //! function, can be one of the following:
  //! -   ChFi2d_Ready if the initialization is correct,
  //! -   ChFi2d_NotPlanar if F is not planar,
  //! -   ChFi2d_NoFace if F is a null face.
  Standard_EXPORT BRepFilletAPI_MakeFillet2d(const TopoDS_Face& F);

  //! Initializes this algorithm for constructing fillets or
  //! chamfers with the face F.
  //! Warning
  //! The status of the initialization, as given by the Status
  //! function, can be one of the following:
  //! -   ChFi2d_Ready if the initialization is correct,
  //! -   ChFi2d_NotPlanar if F is not planar,
  //! -   ChFi2d_NoFace if F is a null face.
  Standard_EXPORT void Init(const TopoDS_Face& F);

  //! This initialize method allow to init the builder
  //! from a face RefFace and another face ModFace which derive from RefFace.
  //! This is useful to modify a fillet or a chamfer already created on ModFace.
  Standard_EXPORT void Init(const TopoDS_Face& RefFace, const TopoDS_Face& ModFace);

  //! Adds a fillet of radius Radius between the two edges
  //! adjacent to the vertex V on the face modified by this
  //! algorithm. The two edges do not need to be rectilinear.
  //! This function returns the fillet and builds the resulting face.
  //! Warning
  //! The status of the construction, as given by the Status
  //! function, can be one of the following:
  //! - ChFi2d_IsDone if the fillet is built,
  //! - ChFi2d_ConnexionError if V does not belong to the initial face,
  //! -   ChFi2d_ComputationError if Radius is too large
  //! to build a fillet between the two adjacent edges,
  //! -   ChFi2d_NotAuthorized
  //! -   if one of the two edges connected to V is a fillet or chamfer, or
  //! -   if a curve other than a straight line or an arc of a
  //! circle is used as E, E1 or E2.
  //! Do not use the returned fillet if the status of the construction is not ChFi2d_IsDone.
  //! Exceptions
  //! Standard_NegativeValue if Radius is less than or equal to zero.
  Standard_EXPORT TopoDS_Edge AddFillet(const TopoDS_Vertex& V, const double Radius);

  //! Assigns the radius Radius to the fillet Fillet already
  //! built on the face modified by this algorithm.
  //! This function returns the new fillet and modifies the existing face.
  //! Warning
  //! The status of the construction, as given by the Status
  //! function, can be one of the following:
  //! -   ChFi2d_IsDone if the new fillet is built,
  //! -   ChFi2d_ConnexionError if Fillet does not
  //! belong to the existing face,
  //! -   ChFi2d_ComputationError if Radius is too
  //! large to build a fillet between the two adjacent edges.
  //! Do not use the returned fillet if the status of the
  //! construction is not ChFi2d_IsDone.
  //! Exceptions
  //! Standard_NegativeValue if Radius is less than or equal to zero.
  Standard_EXPORT TopoDS_Edge ModifyFillet(const TopoDS_Edge& Fillet, const double Radius);

  //! Removes the fillet Fillet already built on the face
  //! modified by this algorithm.
  //! This function returns the vertex connecting the two
  //! adjacent edges of Fillet and modifies the existing face.
  //! Warning
  //! -   The returned vertex is only valid if the Status
  //! function returns ChFi2d_IsDone.
  //! -   A null vertex is returned if the edge Fillet does not
  //! belong to the initial face.
  Standard_EXPORT TopoDS_Vertex RemoveFillet(const TopoDS_Edge& Fillet);

  //! Adds a chamfer on the face modified by this algorithm
  //! between the two adjacent edges E1 and E2, where
  //! the extremities of the chamfer are on E1 and E2 at
  //! distances D1 and D2 respectively
  //! In cases where the edges are not rectilinear, distances
  //! are measured using the curvilinear abscissa of the
  //! edges and the angle is measured with respect to the
  //! tangent at the corresponding point.
  //! The angle Ang is given in radians.
  //! This function returns the chamfer and builds the resulting face.
  Standard_EXPORT TopoDS_Edge AddChamfer(const TopoDS_Edge& E1,
                                         const TopoDS_Edge& E2,
                                         const double       D1,
                                         const double       D2);

  //! Adds a chamfer on the face modified by this algorithm
  //! between the two edges connected by the vertex V,
  //! where E is one of the two edges. The chamfer makes
  //! an angle Ang with E and one of its extremities is on
  //! E at distance D from V.
  //! In cases where the edges are not rectilinear, distances
  //! are measured using the curvilinear abscissa of the
  //! edges and the angle is measured with respect to the
  //! tangent at the corresponding point.
  //! The angle Ang is given in radians.
  //! This function returns the chamfer and builds the resulting face.
  //! Warning
  //! The status of the construction, as given by the Status function, can
  //! be one of the following:
  //! -          ChFi2d_IsDone if the chamfer is built,
  //! -  ChFi2d_ParametersError if D1, D2, D or Ang is less than or equal to zero,
  //! -          ChFi2d_ConnexionError if:
  //! - the edge E, E1 or E2 does not belong to the initial face, or
  //! -  the edges E1 and E2 are not adjacent, or
  //! -  the vertex V is not one of the limit points of the edge E,
  //! -          ChFi2d_ComputationError if the parameters of the chamfer
  //! are too large to build a chamfer between the two adjacent edges,
  //! -          ChFi2d_NotAuthorized if:
  //! - the edge E1, E2 or one of the two edges connected to V is a fillet or chamfer, or
  //! - a curve other than a straight line or an arc of a circle is used as E, E1 or E2.
  //! Do not use the returned chamfer if
  //! the status of the construction is not ChFi2d_IsDone.
  Standard_EXPORT TopoDS_Edge AddChamfer(const TopoDS_Edge&   E,
                                         const TopoDS_Vertex& V,
                                         const double         D,
                                         const double         Ang);

  //! Modifies the chamfer Chamfer on the face modified
  //! by this algorithm, where:
  //! E1 and E2 are the two adjacent edges on which
  //! Chamfer is already built; the extremities of the new
  //! chamfer are on E1 and E2 at distances D1 and D2 respectively.
  Standard_EXPORT TopoDS_Edge ModifyChamfer(const TopoDS_Edge& Chamfer,
                                            const TopoDS_Edge& E1,
                                            const TopoDS_Edge& E2,
                                            const double       D1,
                                            const double       D2);

  //! Modifies the chamfer Chamfer on the face modified
  //! by this algorithm, where:
  //! E is one of the two adjacent edges on which
  //! Chamfer is already built; the new chamfer makes
  //! an angle Ang with E and one of its extremities is
  //! on E at distance D from the vertex on which the chamfer is built.
  //! In cases where the edges are not rectilinear, the
  //! distances are measured using the curvilinear abscissa
  //! of the edges and the angle is measured with respect
  //! to the tangent at the corresponding point.
  //! The angle Ang is given in radians.
  //! This function returns the new chamfer and modifies the existing face.
  //! Warning
  //! The status of the construction, as given by the Status
  //! function, can be one of the following:
  //! -   ChFi2d_IsDone if the chamfer is built,
  //! -   ChFi2d_ParametersError if D1, D2, D or Ang is less than or equal to zero,
  //! -   ChFi2d_ConnexionError if:
  //! -   the edge E, E1, E2 or Chamfer does not belong
  //! to the existing face, or
  //! -   the edges E1 and E2 are not adjacent,
  //! -   ChFi2d_ComputationError if the parameters of
  //! the chamfer are too large to build a chamfer
  //! between the two adjacent edges,
  //! -   ChFi2d_NotAuthorized if E1 or E2 is a fillet or chamfer.
  //! Do not use the returned chamfer if the status of the
  //! construction is not ChFi2d_IsDone.
  Standard_EXPORT TopoDS_Edge ModifyChamfer(const TopoDS_Edge& Chamfer,
                                            const TopoDS_Edge& E,
                                            const double       D,
                                            const double       Ang);

  //! Removes the chamfer Chamfer already built on the face
  //! modified by this algorithm.
  //! This function returns the vertex connecting the two
  //! adjacent edges of Chamfer and modifies the existing face.
  //! Warning
  //! -   The returned vertex is only valid if the Status
  //! function returns ChFi2d_IsDone.
  //! -   A null vertex is returned if the edge Chamfer does
  //! not belong to the initial face.
  Standard_EXPORT TopoDS_Vertex RemoveChamfer(const TopoDS_Edge& Chamfer);

  //! Returns true if the edge E on the face modified by this
  //! algorithm is chamfered or filleted.
  //! Warning
  //! Returns false if E does not belong to the face modified by this algorithm.
  bool IsModified(const TopoDS_Edge& E) const;

  //! Returns the table of fillets on the face modified by this algorithm.
  const NCollection_Sequence<TopoDS_Shape>& FilletEdges() const;

  //! Returns the number of fillets on the face modified by this algorithm.
  int NbFillet() const;

  //! Returns the table of chamfers on the face modified by this algorithm.
  const NCollection_Sequence<TopoDS_Shape>& ChamferEdges() const;

  //! Returns the number of chamfers on the face modified by this algorithm.
  int NbChamfer() const;

  //! Returns the list of shapes modified from the shape <S>:
  //! - for the initial face, the result face (once the algorithm is done);
  //! - for a chamfered or filleted edge, the edge built from it.
  //! The list is empty for any other shape.
  Standard_EXPORT const NCollection_List<TopoDS_Shape>& Modified(const TopoDS_Shape& S) override;

  //! Returns the list of shapes generated from the shape <S>: for a vertex of the
  //! initial face, the fillet or chamfer built at this vertex. The list is empty for
  //! any other shape.
  //! Warning
  //! The history (Modified of the face, Generated, IsDeleted) refers to the last call
  //! of Build(). The fillets and chamfers found on the modified face given to
  //! Init(RefFace, ModFace) are not reported as generated: their vertex is not known.
  //! Modified of an edge reflects the current state of the algorithm (also before Build()).
  //! On a face of two edges, ModifyFillet and ModifyChamfer may rebuild the fillet at the
  //! other common vertex; the modified fillet is then not reported as generated.
  Standard_EXPORT const NCollection_List<TopoDS_Shape>& Generated(const TopoDS_Shape& S) override;

  //! Returns true if <S>, a face, edge or vertex of the initial face, is not in the
  //! result and has no modified shape: a filleted or chamfered vertex, or an edge
  //! entirely removed by its fillets or chamfers.
  Standard_EXPORT bool IsDeleted(const TopoDS_Shape& S) override;

  //! returns the number of new curves
  //! after the shape creation.
  Standard_EXPORT int NbCurves() const;

  //! Return the Edges created for curve I.
  Standard_EXPORT const NCollection_List<TopoDS_Shape>& NewEdges(const int I);

  bool HasDescendant(const TopoDS_Edge& E) const;

  //! Returns the chamfered or filleted edge built from the
  //! edge E on the face modified by this algorithm. If E has
  //! not been modified, this function returns E.
  //! Exceptions
  //! Standard_NoSuchObject if the edge E does not
  //! belong to the initial face.
  const TopoDS_Edge& DescendantEdge(const TopoDS_Edge& E) const;

  //! Returns the basis edge on the face modified by this
  //! algorithm from which the chamfered or filleted edge E is
  //! built. If E has not been modified, this function returns E.
  //! Warning
  //! E is returned if it does not belong to the initial face.
  Standard_EXPORT const TopoDS_Edge& BasisEdge(const TopoDS_Edge& E) const;

  ChFi2d_ConstructionError Status() const;

  //! Update the result and set the Done flag
  Standard_EXPORT void Build(
    const Message_ProgressRange& theRange = Message_ProgressRange()) override;

private:
  //! Records theCorner as the vertex replaced by the new edge theNewEdge.
  void recordCorner(const TopoDS_Edge& theNewEdge, const TopoDS_Vertex& theCorner);

  //! Moves the vertex recorded for theOldEdge to theNewEdge (or forgets it if theNewEdge is
  //! null), when theOldEdge has been removed.
  void moveCorner(const TopoDS_Edge& theOldEdge, const TopoDS_Edge& theNewEdge);

  //! Clears the recorded history.
  void clearHistory();

private:
  ChFi2d_Builder                                                myMakeChFi2d;
  TopoDS_Face                                                   myInitialFace;
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> myInitialShapes;
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> myResultShapes;
  //! New edge (fillet or chamfer) -> vertex of the face it replaces.
  NCollection_DataMap<TopoDS_Shape, TopoDS_Shape, TopTools_ShapeMapHasher> myCorners;
  //! Vertex of the initial face -> fillets and chamfers of the result built at it (last Build).
  NCollection_DataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>
    myGeneratedFrom;
  //! Edges of the initial face that had a descendant at the last Build.
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> myModifiedEdges;
};

#include <BRepFilletAPI_MakeFillet2d.lxx>

#endif // _BRepFilletAPI_MakeFillet2d_HeaderFile
