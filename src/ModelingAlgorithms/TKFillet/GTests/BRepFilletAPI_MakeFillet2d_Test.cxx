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
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepFilletAPI_MakeFillet2d.hxx>
#include <gp.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <TopLoc_Location.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>

#include <gtest/gtest.h>

#include <cmath>

namespace
{
using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

TopoDS_Face rectangle(const double theA, const double theB)
{
  BRepBuilderAPI_MakePolygon aPoly(gp_Pnt(0, 0, 0),
                                   gp_Pnt(theA, 0, 0),
                                   gp_Pnt(theA, theB, 0),
                                   gp_Pnt(0, theB, 0),
                                   true);
  return BRepBuilderAPI_MakeFace(aPoly.Wire(), true).Face();
}

TopoDS_Face regularPolygon(const int theN, const double theR)
{
  BRepBuilderAPI_MakePolygon aPoly;
  for (int i = 0; i < theN; ++i)
  {
    aPoly.Add(
      gp_Pnt(theR * std::cos(2 * M_PI * i / theN), theR * std::sin(2 * M_PI * i / theN), 0));
  }
  aPoly.Close();
  return BRepBuilderAPI_MakeFace(aPoly.Wire(), true).Face();
}

ShapeMap shapesOf(const TopoDS_Shape& theShape, const TopAbs_ShapeEnum theType)
{
  ShapeMap aMap;
  TopExp::MapShapes(theShape, theType, aMap);
  return aMap;
}

gp_Pnt midPoint(const TopoDS_Edge& theEdge)
{
  BRepAdaptor_Curve aCurve(theEdge);
  return aCurve.Value(0.5 * (aCurve.FirstParameter() + aCurve.LastParameter()));
}

TopoDS_Vertex vertexAt(const TopoDS_Shape& theShape, const gp_Pnt& thePnt)
{
  const ShapeMap aVertices = shapesOf(theShape, TopAbs_VERTEX);
  for (int i = 1; i <= aVertices.Extent(); ++i)
  {
    if (BRep_Tool::Pnt(TopoDS::Vertex(aVertices(i))).Distance(thePnt) < 1.0e-9)
    {
      return TopoDS::Vertex(aVertices(i));
    }
  }
  return TopoDS_Vertex();
}

TopoDS_Edge edgeThrough(const TopoDS_Shape& theShape, const gp_Pnt& theMid)
{
  const ShapeMap anEdges = shapesOf(theShape, TopAbs_EDGE);
  for (int i = 1; i <= anEdges.Extent(); ++i)
  {
    if (midPoint(TopoDS::Edge(anEdges(i))).Distance(theMid) < 1.0e-9)
    {
      return TopoDS::Edge(anEdges(i));
    }
  }
  return TopoDS_Edge();
}

// Checks that theCorner is deleted and generates exactly one edge of the result, at distance
// theDist from it; returns that edge.
TopoDS_Shape checkCorner(BRepFilletAPI_MakeFillet2d& theAlgo,
                         const TopoDS_Vertex&        theCorner,
                         const double                theDist)
{
  EXPECT_TRUE(theAlgo.IsDeleted(theCorner));
  const NCollection_List<TopoDS_Shape> aGen = theAlgo.Generated(theCorner);
  EXPECT_EQ(aGen.Extent(), 1);
  if (aGen.Extent() != 1)
  {
    return TopoDS_Shape();
  }
  EXPECT_TRUE(shapesOf(theAlgo.Shape(), TopAbs_EDGE).Contains(aGen.First()));
  EXPECT_NEAR(BRep_Tool::Pnt(theCorner).Distance(midPoint(TopoDS::Edge(aGen.First()))),
              theDist,
              1.0e-6);
  return aGen.First();
}

// Checks the history of a fillet or chamfer built at every vertex of theFace: every face and edge
// of the result is traced back to the initial face, every vertex is deleted and generates exactly
// the new edge built at it.
void checkAllCorners(BRepFilletAPI_MakeFillet2d& theAlgo,
                     const TopoDS_Face&          theFace,
                     const double                theCornerDist)
{
  ASSERT_TRUE(theAlgo.IsDone());
  const TopoDS_Shape& aResult = theAlgo.Shape();
  ShapeMap            aTraced;

  const NCollection_List<TopoDS_Shape>& aModFace = theAlgo.Modified(theFace);
  ASSERT_EQ(aModFace.Extent(), 1);
  EXPECT_TRUE(aModFace.First().IsSame(aResult));
  aTraced.Add(aModFace.First());
  EXPECT_FALSE(theAlgo.IsDeleted(theFace));

  const ShapeMap aResultEdges = shapesOf(aResult, TopAbs_EDGE);
  const ShapeMap anInitEdges  = shapesOf(theFace, TopAbs_EDGE);
  for (int i = 1; i <= anInitEdges.Extent(); ++i)
  {
    const NCollection_List<TopoDS_Shape> aMod = theAlgo.Modified(anInitEdges(i));
    ASSERT_EQ(aMod.Extent(), 1) << "edge " << i;
    EXPECT_TRUE(aResultEdges.Contains(aMod.First()));
    EXPECT_FALSE(theAlgo.IsDeleted(anInitEdges(i)));
    aTraced.Add(aMod.First());
  }

  const ShapeMap aVertices = shapesOf(theFace, TopAbs_VERTEX);
  ShapeMap       aNewEdges;
  for (int i = 1; i <= aVertices.Extent(); ++i)
  {
    const TopoDS_Vertex& aV = TopoDS::Vertex(aVertices(i));
    EXPECT_TRUE(theAlgo.IsDeleted(aV)) << "vertex " << i;
    EXPECT_TRUE(theAlgo.Modified(aV).IsEmpty());
    const NCollection_List<TopoDS_Shape> aGen = theAlgo.Generated(aV);
    ASSERT_EQ(aGen.Extent(), 1) << "vertex " << i;
    EXPECT_TRUE(aResultEdges.Contains(aGen.First()));
    EXPECT_NEAR(BRep_Tool::Pnt(aV).Distance(midPoint(TopoDS::Edge(aGen.First()))),
                theCornerDist,
                1.0e-6);
    aNewEdges.Add(aGen.First());
    aTraced.Add(aGen.First());
  }
  EXPECT_EQ(aNewEdges.Extent(), aVertices.Extent());

  for (int i = 1; i <= aResultEdges.Extent(); ++i)
  {
    EXPECT_TRUE(aTraced.Contains(aResultEdges(i))) << "untraced result edge " << i;
  }
}
} // namespace

// The history queries threw Standard_TypeMismatch for faces and vertices (and
// Standard_NoSuchObject for edges that were not modified), the result face was not reported as
// modified from the initial face and the fillets were not reported as generated from the vertices.
TEST(BRepFilletAPI_MakeFillet2dTest, FilletAllCorners_History)
{
  const TopoDS_Face          aFace = rectangle(20, 10);
  BRepFilletAPI_MakeFillet2d aFillet(aFace);
  const ShapeMap             aVertices = shapesOf(aFace, TopAbs_VERTEX);
  for (int i = 1; i <= aVertices.Extent(); ++i)
  {
    aFillet.AddFillet(TopoDS::Vertex(aVertices(i)), 2.0);
  }
  aFillet.Build();
  // Middle of a quarter circle of radius r at a right corner: r (sqrt(2) - 1) from the corner.
  checkAllCorners(aFillet, aFace, 2.0 * (std::sqrt(2.0) - 1.0));
}

TEST(BRepFilletAPI_MakeFillet2dTest, ChamferAllCorners_History)
{
  const TopoDS_Face          aFace = regularPolygon(6, 10);
  BRepFilletAPI_MakeFillet2d aChamfer(aFace);
  NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>
    aVE;
  TopExp::MapShapesAndAncestors(aFace, TopAbs_VERTEX, TopAbs_EDGE, aVE);
  for (int i = 1; i <= aVE.Extent(); ++i)
  {
    aChamfer.AddChamfer(TopoDS::Edge(aVE(i).First()), TopoDS::Edge(aVE(i).Last()), 1.0, 1.0);
  }
  aChamfer.Build();
  // Middle of a symmetric chamfer of distance d at a 120 degree corner: d cos(60 deg) = d / 2.
  checkAllCorners(aChamfer, aFace, 0.5);
}

// Only one corner is filleted: the other edges and vertices are kept and have no history.
TEST(BRepFilletAPI_MakeFillet2dTest, FilletOneCorner_OtherShapesKept)
{
  const TopoDS_Face          aFace = rectangle(20, 10);
  BRepFilletAPI_MakeFillet2d aFillet(aFace);
  TopoDS_Vertex              aCorner;
  const ShapeMap             aVertices = shapesOf(aFace, TopAbs_VERTEX);
  for (int i = 1; i <= aVertices.Extent(); ++i)
  {
    if (BRep_Tool::Pnt(TopoDS::Vertex(aVertices(i))).Distance(gp_Pnt(20, 10, 0)) < 1.0e-9)
    {
      aCorner = TopoDS::Vertex(aVertices(i));
    }
  }
  ASSERT_FALSE(aCorner.IsNull());
  aFillet.AddFillet(aCorner, 2.0);
  aFillet.Build();
  ASSERT_TRUE(aFillet.IsDone());

  const ShapeMap aResultEdges    = shapesOf(aFillet.Shape(), TopAbs_EDGE);
  const ShapeMap aResultVertices = shapesOf(aFillet.Shape(), TopAbs_VERTEX);
  const ShapeMap anInitEdges     = shapesOf(aFace, TopAbs_EDGE);
  int            aNbModified     = 0;
  for (int i = 1; i <= anInitEdges.Extent(); ++i)
  {
    const NCollection_List<TopoDS_Shape> aMod = aFillet.Modified(anInitEdges(i));
    EXPECT_FALSE(aFillet.IsDeleted(anInitEdges(i)));
    if (aMod.IsEmpty())
    {
      EXPECT_TRUE(aResultEdges.Contains(anInitEdges(i))) << "edge " << i;
    }
    else
    {
      ++aNbModified;
      EXPECT_TRUE(aResultEdges.Contains(aMod.First()));
    }
  }
  EXPECT_EQ(aNbModified, 2);

  for (int i = 1; i <= aVertices.Extent(); ++i)
  {
    const TopoDS_Shape& aV = aVertices(i);
    if (aV.IsSame(aCorner))
    {
      EXPECT_TRUE(aFillet.IsDeleted(aV));
      ASSERT_EQ(aFillet.Generated(aV).Extent(), 1);
      EXPECT_TRUE(aFillet.Generated(aV).First().IsSame(aFillet.FilletEdges().First()));
    }
    else
    {
      EXPECT_FALSE(aFillet.IsDeleted(aV)) << "vertex " << i;
      EXPECT_TRUE(aFillet.Generated(aV).IsEmpty());
      EXPECT_TRUE(aResultVertices.Contains(aV));
    }
  }
}

// Two fillets of radius 5 at both ends of the short side (length 10) remove the whole side.
TEST(BRepFilletAPI_MakeFillet2dTest, SideRemovedByTwoFillets_EdgeDeleted)
{
  const TopoDS_Face          aFace = rectangle(20, 10);
  BRepFilletAPI_MakeFillet2d aFillet(aFace);
  TopoDS_Edge                aShortSide;
  const ShapeMap             anEdges = shapesOf(aFace, TopAbs_EDGE);
  for (int i = 1; i <= anEdges.Extent(); ++i)
  {
    if (midPoint(TopoDS::Edge(anEdges(i))).Distance(gp_Pnt(20, 5, 0)) < 1.0e-9)
    {
      aShortSide = TopoDS::Edge(anEdges(i));
    }
  }
  ASSERT_FALSE(aShortSide.IsNull());
  TopoDS_Vertex aV1, aV2;
  TopExp::Vertices(aShortSide, aV1, aV2);
  aFillet.AddFillet(aV1, 5.0);
  aFillet.AddFillet(aV2, 5.0);
  aFillet.Build();
  ASSERT_TRUE(aFillet.IsDone());
  EXPECT_FALSE(shapesOf(aFillet.Shape(), TopAbs_EDGE).Contains(aShortSide));
  EXPECT_TRUE(aFillet.Modified(aShortSide).IsEmpty());
  EXPECT_TRUE(aFillet.IsDeleted(aShortSide));
  // The two fillets join each other; each one is generated from its own corner.
  for (const TopoDS_Vertex& aV : {aV1, aV2})
  {
    const NCollection_List<TopoDS_Shape> aGen = aFillet.Generated(aV);
    ASSERT_EQ(aGen.Extent(), 1);
    EXPECT_NEAR(BRep_Tool::Pnt(aV).Distance(midPoint(TopoDS::Edge(aGen.First()))),
                5.0 * (std::sqrt(2.0) - 1.0),
                1.0e-6);
  }
  // Copies: each query refills the list returned by the previous one.
  const NCollection_List<TopoDS_Shape> aGen1 = aFillet.Generated(aV1);
  const NCollection_List<TopoDS_Shape> aGen2 = aFillet.Generated(aV2);
  ASSERT_FALSE(aGen1.IsEmpty());
  ASSERT_FALSE(aGen2.IsEmpty());
  EXPECT_FALSE(aGen1.First().IsSame(aGen2.First()));
}

// Fillets of radius 5 at the four corners of a square 10 x 10 give a circle (bug 0022695): every
// side is removed and each fillet ends on the vertex of the previous one.
TEST(BRepFilletAPI_MakeFillet2dTest, SquareToCircle_EachArcFromItsCorner)
{
  const TopoDS_Face          aFace = rectangle(10, 10);
  BRepFilletAPI_MakeFillet2d aFillet(aFace);
  const ShapeMap             aVertices = shapesOf(aFace, TopAbs_VERTEX);
  for (int i = 1; i <= aVertices.Extent(); ++i)
  {
    aFillet.AddFillet(TopoDS::Vertex(aVertices(i)), 5.0);
  }
  aFillet.Build();
  ASSERT_TRUE(aFillet.IsDone());
  ShapeMap anArcs;
  for (int i = 1; i <= aVertices.Extent(); ++i)
  {
    anArcs.Add(checkCorner(aFillet, TopoDS::Vertex(aVertices(i)), 5.0 * (std::sqrt(2.0) - 1.0)));
  }
  EXPECT_EQ(anArcs.Extent(), 4);
  const ShapeMap aResultEdges = shapesOf(aFillet.Shape(), TopAbs_EDGE);
  for (int i = 1; i <= aResultEdges.Extent(); ++i)
  {
    EXPECT_TRUE(anArcs.Contains(aResultEdges(i))) << "untraced result edge " << i;
  }
  const ShapeMap anEdges = shapesOf(aFace, TopAbs_EDGE);
  for (int i = 1; i <= anEdges.Extent(); ++i)
  {
    EXPECT_TRUE(aFillet.Modified(anEdges(i)).IsEmpty());
    EXPECT_TRUE(aFillet.IsDeleted(anEdges(i)));
  }
}

// A quarter disk: the fillet at B removes both sides AB and BC; a second fillet at D. Each fillet
// must be generated from its own corner only.
TEST(BRepFilletAPI_MakeFillet2dTest, QuarterDisk_FilletsNotAttributedToOtherCorner)
{
  const TopoDS_Face          aFace = rectangle(10, 10);
  const TopoDS_Vertex        aB    = vertexAt(aFace, gp_Pnt(10, 0, 0));
  const TopoDS_Vertex        aD    = vertexAt(aFace, gp_Pnt(0, 10, 0));
  BRepFilletAPI_MakeFillet2d aFillet(aFace);
  aFillet.AddFillet(aB, 10.0);
  aFillet.AddFillet(aD, 1.0);
  aFillet.Build();
  ASSERT_TRUE(aFillet.IsDone());
  const TopoDS_Shape anArcB = checkCorner(aFillet, aB, 10.0 * (std::sqrt(2.0) - 1.0));
  const TopoDS_Shape anArcD = checkCorner(aFillet, aD, 1.0 * (std::sqrt(2.0) - 1.0));
  EXPECT_FALSE(anArcB.IsSame(anArcD));
}

// Chamfer defined by a distance on an edge and an angle.
TEST(BRepFilletAPI_MakeFillet2dTest, ChamferDistanceAngle_History)
{
  const TopoDS_Face          aFace   = rectangle(20, 10);
  const TopoDS_Vertex        aCorner = vertexAt(aFace, gp_Pnt(20, 0, 0));
  const TopoDS_Edge          aBottom = edgeThrough(aFace, gp_Pnt(10, 0, 0));
  BRepFilletAPI_MakeFillet2d aChamfer(aFace);
  const TopoDS_Edge          aNew = aChamfer.AddChamfer(aBottom, aCorner, 2.0, M_PI / 4.0);
  aChamfer.Build();
  ASSERT_TRUE(aChamfer.IsDone());
  // 45 degrees at a right corner: symmetric chamfer of distance 2, middle at 2 cos(45 deg).
  EXPECT_TRUE(checkCorner(aChamfer, aCorner, 2.0 * std::sqrt(0.5)).IsSame(aNew));
  EXPECT_EQ(aChamfer.Modified(aBottom).Extent(), 1);
}

// Fillets at two corners and chamfers at the two others.
TEST(BRepFilletAPI_MakeFillet2dTest, MixedFilletsAndChamfers_History)
{
  const TopoDS_Face          aFace   = rectangle(20, 10);
  const TopoDS_Edge          aBottom = edgeThrough(aFace, gp_Pnt(10, 0, 0));
  const TopoDS_Edge          aRight  = edgeThrough(aFace, gp_Pnt(20, 5, 0));
  const TopoDS_Edge          aTop    = edgeThrough(aFace, gp_Pnt(10, 10, 0));
  const TopoDS_Edge          aLeft   = edgeThrough(aFace, gp_Pnt(0, 5, 0));
  BRepFilletAPI_MakeFillet2d anAlgo(aFace);
  anAlgo.AddFillet(vertexAt(aFace, gp_Pnt(0, 0, 0)), 2.0);
  anAlgo.AddFillet(vertexAt(aFace, gp_Pnt(20, 10, 0)), 2.0);
  anAlgo.AddChamfer(anAlgo.DescendantEdge(aBottom), anAlgo.DescendantEdge(aRight), 1.0, 1.0);
  anAlgo.AddChamfer(anAlgo.DescendantEdge(aTop), anAlgo.DescendantEdge(aLeft), 1.0, 1.0);
  anAlgo.Build();
  ASSERT_TRUE(anAlgo.IsDone());
  ASSERT_EQ(anAlgo.NbFillet(), 2);
  ASSERT_EQ(anAlgo.NbChamfer(), 2);
  const double aFilletDist  = 2.0 * (std::sqrt(2.0) - 1.0);
  const double aChamferDist = std::sqrt(0.5);
  EXPECT_TRUE(checkCorner(anAlgo, vertexAt(aFace, gp_Pnt(0, 0, 0)), aFilletDist)
                .IsSame(anAlgo.FilletEdges().First()));
  EXPECT_TRUE(checkCorner(anAlgo, vertexAt(aFace, gp_Pnt(20, 10, 0)), aFilletDist)
                .IsSame(anAlgo.FilletEdges().Last()));
  EXPECT_TRUE(checkCorner(anAlgo, vertexAt(aFace, gp_Pnt(20, 0, 0)), aChamferDist)
                .IsSame(anAlgo.ChamferEdges().First()));
  EXPECT_TRUE(checkCorner(anAlgo, vertexAt(aFace, gp_Pnt(0, 10, 0)), aChamferDist)
                .IsSame(anAlgo.ChamferEdges().Last()));
}

// Modify and remove before Build: the modified fillet and chamfer are generated from their
// corners, the removed fillet from nothing.
TEST(BRepFilletAPI_MakeFillet2dTest, ModifyAndRemoveBeforeBuild_History)
{
  const TopoDS_Face          aFace   = rectangle(20, 10);
  const TopoDS_Vertex        aV1     = vertexAt(aFace, gp_Pnt(20, 0, 0));
  const TopoDS_Vertex        aV2     = vertexAt(aFace, gp_Pnt(0, 10, 0));
  const TopoDS_Vertex        aV3     = vertexAt(aFace, gp_Pnt(0, 0, 0));
  const TopoDS_Edge          aBottom = edgeThrough(aFace, gp_Pnt(10, 0, 0));
  const TopoDS_Edge          aLeft   = edgeThrough(aFace, gp_Pnt(0, 5, 0));
  BRepFilletAPI_MakeFillet2d anAlgo(aFace);
  const TopoDS_Edge          aF1 = anAlgo.AddFillet(aV1, 2.0);
  const TopoDS_Edge          aF2 = anAlgo.AddFillet(aV2, 2.0);
  const TopoDS_Edge          aC3 =
    anAlgo.AddChamfer(anAlgo.DescendantEdge(aBottom), anAlgo.DescendantEdge(aLeft), 1.0, 1.0);
  const TopoDS_Edge   aF1b  = anAlgo.ModifyFillet(aF1, 3.0);
  const TopoDS_Vertex aBack = anAlgo.RemoveFillet(aF2);
  const TopoDS_Edge   aC3b  = anAlgo.ModifyChamfer(aC3,
                                                anAlgo.DescendantEdge(aBottom),
                                                anAlgo.DescendantEdge(aLeft),
                                                2.0,
                                                2.0);
  ASSERT_FALSE(aF1b.IsNull());
  ASSERT_FALSE(aBack.IsNull());
  ASSERT_FALSE(aC3b.IsNull());
  anAlgo.Build();
  ASSERT_TRUE(anAlgo.IsDone());
  EXPECT_TRUE(checkCorner(anAlgo, aV1, 3.0 * (std::sqrt(2.0) - 1.0)).IsSame(aF1b));
  EXPECT_TRUE(checkCorner(anAlgo, aV3, 2.0 * std::sqrt(0.5)).IsSame(aC3b));
  EXPECT_TRUE(anAlgo.Generated(aV2).IsEmpty());
  EXPECT_TRUE(anAlgo.Generated(aBack).IsEmpty());
}

// The fillet or chamfer given to Modify or Remove is a reference to an item of FilletEdges() or
// ChamferEdges() (TopoDS::Edge returns a reference to its argument): the builder removes that item,
// and the history read the freed item afterwards (heap-use-after-free under AddressSanitizer). The
// history is that of the copies.
TEST(BRepFilletAPI_MakeFillet2dTest, ModifyAndRemoveByListReference_History)
{
  const TopoDS_Face          aFace   = rectangle(20, 10);
  const TopoDS_Vertex        aV1     = vertexAt(aFace, gp_Pnt(20, 0, 0));
  const TopoDS_Vertex        aV2     = vertexAt(aFace, gp_Pnt(0, 10, 0));
  const TopoDS_Vertex        aV3     = vertexAt(aFace, gp_Pnt(0, 0, 0));
  const TopoDS_Vertex        aV4     = vertexAt(aFace, gp_Pnt(20, 10, 0));
  const TopoDS_Edge          aBottom = edgeThrough(aFace, gp_Pnt(10, 0, 0));
  const TopoDS_Edge          aLeft   = edgeThrough(aFace, gp_Pnt(0, 5, 0));
  const TopoDS_Edge          aRight  = edgeThrough(aFace, gp_Pnt(20, 5, 0));
  const TopoDS_Edge          aTop    = edgeThrough(aFace, gp_Pnt(10, 10, 0));
  BRepFilletAPI_MakeFillet2d anAlgo(aFace);
  anAlgo.AddFillet(aV1, 2.0);
  anAlgo.AddFillet(aV2, 2.0);
  anAlgo.AddChamfer(anAlgo.DescendantEdge(aBottom), anAlgo.DescendantEdge(aLeft), 1.0, 1.0);
  anAlgo.AddChamfer(anAlgo.DescendantEdge(aRight), anAlgo.DescendantEdge(aTop), 1.0, 1.0);
  ASSERT_EQ(anAlgo.FilletEdges().Length(), 2);
  ASSERT_EQ(anAlgo.ChamferEdges().Length(), 2);

  const TopoDS_Edge aF1b = anAlgo.ModifyFillet(TopoDS::Edge(anAlgo.FilletEdges().First()), 3.0);
  ASSERT_FALSE(aF1b.IsNull());
  // the modified fillet is now the last item; the first one is the fillet at aV2
  const TopoDS_Vertex aBack = anAlgo.RemoveFillet(TopoDS::Edge(anAlgo.FilletEdges().First()));
  ASSERT_FALSE(aBack.IsNull());
  const TopoDS_Edge aC3b = anAlgo.ModifyChamfer(TopoDS::Edge(anAlgo.ChamferEdges().First()),
                                                anAlgo.DescendantEdge(aBottom),
                                                anAlgo.DescendantEdge(aLeft),
                                                2.0,
                                                2.0);
  ASSERT_FALSE(aC3b.IsNull());
  // the modified chamfer is now the last item; the first one is the chamfer at aV4
  const TopoDS_Vertex aBack4 = anAlgo.RemoveChamfer(TopoDS::Edge(anAlgo.ChamferEdges().First()));
  ASSERT_FALSE(aBack4.IsNull());

  anAlgo.Build();
  ASSERT_TRUE(anAlgo.IsDone());
  EXPECT_TRUE(checkCorner(anAlgo, aV1, 3.0 * (std::sqrt(2.0) - 1.0)).IsSame(aF1b));
  EXPECT_TRUE(checkCorner(anAlgo, aV3, 2.0 * std::sqrt(0.5)).IsSame(aC3b));
  EXPECT_TRUE(anAlgo.Generated(aV2).IsEmpty());
  EXPECT_TRUE(anAlgo.Generated(aV4).IsEmpty());
}

// Modified of an edge is available before Build, as before.
TEST(BRepFilletAPI_MakeFillet2dTest, ModifiedEdgeBeforeBuild)
{
  const TopoDS_Face          aFace = rectangle(20, 10);
  BRepFilletAPI_MakeFillet2d anAlgo(aFace);
  anAlgo.AddFillet(vertexAt(aFace, gp_Pnt(20, 0, 0)), 2.0);
  const TopoDS_Edge aBottom = edgeThrough(aFace, gp_Pnt(10, 0, 0));
  ASSERT_EQ(anAlgo.Modified(aBottom).Extent(), 1);
  EXPECT_TRUE(anAlgo.Modified(aBottom).First().IsSame(anAlgo.DescendantEdge(aBottom)));
  EXPECT_TRUE(anAlgo.Modified(aFace).IsEmpty()); // not built yet
}

// Init on a used algorithm: the history of the previous face is not reported for the new one.
TEST(BRepFilletAPI_MakeFillet2dTest, InitAfterBuild_HistoryCleared)
{
  const TopoDS_Face          aFace1 = rectangle(20, 10);
  const TopoDS_Face          aFace2 = rectangle(30, 15);
  const TopoDS_Vertex        aV1    = vertexAt(aFace1, gp_Pnt(20, 0, 0));
  BRepFilletAPI_MakeFillet2d anAlgo(aFace1);
  anAlgo.AddFillet(aV1, 2.0);
  anAlgo.Build();
  ASSERT_TRUE(anAlgo.IsDone());
  ASSERT_EQ(anAlgo.Generated(aV1).Extent(), 1);

  anAlgo.Init(aFace2);
  EXPECT_TRUE(anAlgo.Modified(aFace1).IsEmpty());
  EXPECT_TRUE(anAlgo.Modified(aFace2).IsEmpty());
  EXPECT_TRUE(anAlgo.Generated(aV1).IsEmpty());
  EXPECT_FALSE(anAlgo.IsDeleted(aV1));

  const TopoDS_Vertex aV2 = vertexAt(aFace2, gp_Pnt(30, 15, 0));
  anAlgo.AddFillet(aV2, 3.0);
  anAlgo.Build();
  ASSERT_TRUE(anAlgo.IsDone());
  checkCorner(anAlgo, aV2, 3.0 * (std::sqrt(2.0) - 1.0));
  EXPECT_TRUE(anAlgo.Generated(aV1).IsEmpty());
  EXPECT_TRUE(anAlgo.Modified(aFace1).IsEmpty());
  EXPECT_EQ(anAlgo.Modified(aFace2).Extent(), 1);
}

TEST(BRepFilletAPI_MakeFillet2dTest, ReversedFace_History)
{
  const TopoDS_Face          aFace = TopoDS::Face(rectangle(20, 10).Reversed());
  BRepFilletAPI_MakeFillet2d aFillet(aFace);
  const ShapeMap             aVertices = shapesOf(aFace, TopAbs_VERTEX);
  for (int i = 1; i <= aVertices.Extent(); ++i)
  {
    aFillet.AddFillet(TopoDS::Vertex(aVertices(i)), 2.0);
  }
  aFillet.Build();
  checkAllCorners(aFillet, aFace, 2.0 * (std::sqrt(2.0) - 1.0));
}

// Face with a hole: the corners of the outer contour are filleted, the hole is kept.
TEST(BRepFilletAPI_MakeFillet2dTest, FaceWithHole_HoleKept)
{
  BRepBuilderAPI_MakeFace aMakeFace(rectangle(30, 20));
  TopoDS_Wire             aHole = BRepBuilderAPI_MakeWire(
                        BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(15, 10, 0), gp::DZ()), 4.0)))
                        .Wire();
  aHole.Reverse();
  aMakeFace.Add(aHole);
  const TopoDS_Face          aFace       = aMakeFace.Face();
  const gp_Pnt               aCorners[4] = {gp_Pnt(0, 0, 0),
                                            gp_Pnt(30, 0, 0),
                                            gp_Pnt(30, 20, 0),
                                            gp_Pnt(0, 20, 0)};
  BRepFilletAPI_MakeFillet2d aFillet(aFace);
  for (const gp_Pnt& aP : aCorners)
  {
    aFillet.AddFillet(vertexAt(aFace, aP), 2.0);
  }
  aFillet.Build();
  ASSERT_TRUE(aFillet.IsDone());
  for (const gp_Pnt& aP : aCorners)
  {
    checkCorner(aFillet, vertexAt(aFace, aP), 2.0 * (std::sqrt(2.0) - 1.0));
  }
  const TopoDS_Edge aCircle = edgeThrough(aFace, gp_Pnt(11, 10, 0));
  ASSERT_FALSE(aCircle.IsNull());
  EXPECT_TRUE(shapesOf(aFillet.Shape(), TopAbs_EDGE).Contains(aCircle));
  EXPECT_TRUE(aFillet.Modified(aCircle).IsEmpty());
  EXPECT_FALSE(aFillet.IsDeleted(aCircle));
}

// Resuming on a face modified earlier: the new fillet is generated from its corner; the fillet
// found on the modified face is not attributed (its vertex is not known).
TEST(BRepFilletAPI_MakeFillet2dTest, InitRefAndModifiedFace_NewFilletOnly)
{
  const TopoDS_Face          aFace = rectangle(20, 10);
  const TopoDS_Vertex        aV1   = vertexAt(aFace, gp_Pnt(20, 0, 0));
  const TopoDS_Vertex        aV2   = vertexAt(aFace, gp_Pnt(0, 10, 0));
  BRepFilletAPI_MakeFillet2d aFirst(aFace);
  aFirst.AddFillet(aV1, 2.0);
  aFirst.Build();
  ASSERT_TRUE(aFirst.IsDone());

  BRepFilletAPI_MakeFillet2d aSecond;
  aSecond.Init(aFace, TopoDS::Face(aFirst.Shape()));
  aSecond.AddFillet(aV2, 3.0);
  aSecond.Build();
  ASSERT_TRUE(aSecond.IsDone());
  checkCorner(aSecond, aV2, 3.0 * (std::sqrt(2.0) - 1.0));
  EXPECT_TRUE(aSecond.Generated(aV1).IsEmpty());
  EXPECT_EQ(aSecond.Modified(aFace).Extent(), 1);
}

// RemoveFillet and ModifyFillet given a chamfer change nothing: the chamfer keeps its corner.
TEST(BRepFilletAPI_MakeFillet2dTest, RemoveFilletOnChamfer_HistoryKept)
{
  const TopoDS_Face          aFace   = rectangle(20, 10);
  const TopoDS_Vertex        aCorner = vertexAt(aFace, gp_Pnt(20, 0, 0));
  BRepFilletAPI_MakeFillet2d anAlgo(aFace);
  const TopoDS_Edge          aChamfer = anAlgo.AddChamfer(edgeThrough(aFace, gp_Pnt(10, 0, 0)),
                                                 edgeThrough(aFace, gp_Pnt(20, 5, 0)),
                                                 1.0,
                                                 1.0);
  ASSERT_FALSE(aChamfer.IsNull());
  EXPECT_TRUE(anAlgo.RemoveFillet(aChamfer).IsNull());
  EXPECT_TRUE(anAlgo.ModifyFillet(aChamfer, 2.0).IsNull());
  ASSERT_EQ(anAlgo.NbChamfer(), 1);
  // The status reflects the last operation; a successful fillet elsewhere makes it done again.
  const TopoDS_Vertex anOther = vertexAt(aFace, gp_Pnt(0, 10, 0));
  ASSERT_FALSE(anAlgo.AddFillet(anOther, 2.0).IsNull());
  anAlgo.Build();
  ASSERT_TRUE(anAlgo.IsDone());
  EXPECT_TRUE(checkCorner(anAlgo, aCorner, std::sqrt(0.5)).IsSame(aChamfer));
  checkCorner(anAlgo, anOther, 2.0 * (std::sqrt(2.0) - 1.0));
}

// The history refers to the last Build: removing the fillet afterwards does not change it.
TEST(BRepFilletAPI_MakeFillet2dTest, HistoryOfLastBuild_AfterRemove)
{
  const TopoDS_Face          aFace   = rectangle(20, 10);
  const TopoDS_Vertex        aCorner = vertexAt(aFace, gp_Pnt(20, 0, 0));
  const TopoDS_Edge          aBottom = edgeThrough(aFace, gp_Pnt(10, 0, 0));
  BRepFilletAPI_MakeFillet2d anAlgo(aFace);
  const TopoDS_Edge          aFillet = anAlgo.AddFillet(aCorner, 2.0);
  anAlgo.Build();
  ASSERT_TRUE(anAlgo.IsDone());
  anAlgo.RemoveFillet(aFillet);
  EXPECT_FALSE(anAlgo.IsDeleted(aBottom));
  EXPECT_TRUE(anAlgo.IsDeleted(aCorner));
  EXPECT_EQ(anAlgo.Generated(aCorner).Extent(), 1);
}

// Located face (TopLoc_Location).
TEST(BRepFilletAPI_MakeFillet2dTest, LocatedFace_History)
{
  gp_Trsf aTrsf;
  aTrsf.SetTranslation(gp_Vec(5, -3, 2));
  const TopoDS_Face          aFace = TopoDS::Face(rectangle(20, 10).Moved(TopLoc_Location(aTrsf)));
  BRepFilletAPI_MakeFillet2d aFillet(aFace);
  const ShapeMap             aVertices = shapesOf(aFace, TopAbs_VERTEX);
  for (int i = 1; i <= aVertices.Extent(); ++i)
  {
    aFillet.AddFillet(TopoDS::Vertex(aVertices(i)), 2.0);
  }
  aFillet.Build();
  checkAllCorners(aFillet, aFace, 2.0 * (std::sqrt(2.0) - 1.0));
}
