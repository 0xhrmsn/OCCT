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

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <BinTools.hxx>
#include <BinTools_IStream.hxx>
#include <BinTools_ObjectType.hxx>
#include <BinTools_OStream.hxx>
#include <BinTools_ShapeReader.hxx>
#include <BinTools_ShapeWriter.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_BezierSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Plane.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <BRepMesh_IncrementalMesh.hxx>

#include <cctype>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

namespace
{

//! Meshed cylindrical face: its numbers of nodes and triangles are written as two integers that
//! do not appear elsewhere in the stream.
TopoDS_Face meshedFace(int& theNbNodes, int& theNbTriangles)
{
  occ::handle<Geom_CylindricalSurface> aCylinder =
    new Geom_CylindricalSurface(gp_Ax3(gp::XOY()), 20.0);
  const TopoDS_Face        aFace = BRepBuilderAPI_MakeFace(aCylinder, 0.0, 6.0, 0.0, 30.0, 1.0e-7);
  BRepMesh_IncrementalMesh aMesher(aFace, 0.01);
  TopLoc_Location          aLoc;
  const occ::handle<Poly_Triangulation>& aTri = BRep_Tool::Triangulation(aFace, aLoc);
  theNbNodes                                  = aTri->NbNodes();
  theNbTriangles                              = aTri->NbTriangles();
  return aFace;
}

//! Replaces in theData the bytes of the pair (theNbNodes, theNbTriangles), encoded like
//! theEncode, by the bytes of (theHugeNbNodes, theNbTriangles).
template <typename Encode>
bool replaceCounts(std::string& theData,
                   const int    theNbNodes,
                   const int    theNbTriangles,
                   const int    theHugeNbNodes,
                   Encode       theEncode,
                   const int    thePrevious = -1)
{
  // with thePrevious, the number to replace is the one that follows it
  const std::string aPrefix  = thePrevious >= 0 ? theEncode(thePrevious) : std::string();
  const std::string aPattern = aPrefix + theEncode(theNbNodes)
                               + (thePrevious >= 0 ? std::string() : theEncode(theNbTriangles));
  const size_t aFound = theData.find(aPattern);
  if (aFound == std::string::npos || theData.find(aPattern, aFound + 1) != std::string::npos)
  {
    return false;
  }
  const size_t      aPos  = aFound + aPrefix.size();
  const std::string aHuge = theEncode(theHugeNbNodes);
  theData.replace(aPos, aHuge.size(), aHuge);
  return true;
}

// 20 million nodes would take about 600 MiB with their UV coordinates.
const int THE_HUGE_NB_NODES = 20000000;

} // namespace

// A damaged or crafted file declares more nodes than it holds: the reader stops before the
// allocation instead of allocating them (BinTools_ShapeSet, formats up to the current one).
TEST(BinTools_ReadLimitsTest, ShapeSet_TriangulationLargerThanStream)
{
  int                aNbNodes = 0, aNbTriangles = 0;
  const TopoDS_Face  aFace = meshedFace(aNbNodes, aNbTriangles);
  std::ostringstream anOut;
  BinTools::Write(aFace, anOut, true, false, BinTools_FormatVersion_CURRENT);
  std::string aData = anOut.str();
  ASSERT_TRUE(
    replaceCounts(aData, aNbNodes, aNbTriangles, THE_HUGE_NB_NODES, [](const int theValue) {
      std::ostringstream aStream;
      BinTools::PutInteger(aStream, theValue);
      return aStream.str();
    }));

  std::istringstream anIn(aData);
  TopoDS_Shape       aRead;
  std::string        aMessage;
  try
  {
    BinTools::Read(aRead, anIn);
  }
  catch (const Standard_Failure& theFailure)
  {
    aMessage = theFailure.what();
  }
  EXPECT_NE(aMessage.find("larger than the stream"), std::string::npos) << aMessage;
}

// Same for the reader of the shapes written by BinTools_ShapeWriter (binary OCAF documents).
TEST(BinTools_ReadLimitsTest, ShapeReader_TriangulationLargerThanStream)
{
  int                  aNbNodes = 0, aNbTriangles = 0;
  const TopoDS_Face    aFace = meshedFace(aNbNodes, aNbTriangles);
  std::ostringstream   anOut;
  BinTools_ShapeWriter aWriter;
  aWriter.SetWithTriangles(true);
  aWriter.Write(aFace, anOut);
  std::string aData = anOut.str();
  ASSERT_TRUE(
    replaceCounts(aData, aNbNodes, aNbTriangles, THE_HUGE_NB_NODES, [](const int theValue) {
      std::ostringstream aStream;
      BinTools_OStream   aBinStream(aStream);
      aBinStream << theValue;
      return aStream.str();
    }));

  std::istringstream   anIn(aData);
  BinTools_ShapeReader aReader;
  aReader.SetWithTriangles(true);
  TopoDS_Shape aRead;
  std::string  aMessage;
  try
  {
    aReader.Read(anIn, aRead);
  }
  catch (const Standard_Failure& theFailure)
  {
    aMessage = theFailure.what();
  }
  EXPECT_NE(aMessage.find("larger than the stream"), std::string::npos) << aMessage;
}

// A file cut right after the two counts: reading its flags fails without an exception and the
// stream is in a failed state, which must not be taken for a stream of unknown size.
TEST(BinTools_ReadLimitsTest, ShapeReader_TruncatedAfterCounts)
{
  int                  aNbNodes = 0, aNbTriangles = 0;
  const TopoDS_Face    aFace = meshedFace(aNbNodes, aNbTriangles);
  std::ostringstream   anOut;
  BinTools_ShapeWriter aWriter;
  aWriter.SetWithTriangles(true);
  aWriter.Write(aFace, anOut);
  std::string aData    = anOut.str();
  auto        anEncode = [](const int theValue) {
    std::ostringstream aStream;
    BinTools_OStream   aBinStream(aStream);
    aBinStream << theValue;
    return aStream.str();
  };
  const std::string aPattern = anEncode(aNbNodes) + anEncode(aNbTriangles);
  ASSERT_TRUE(replaceCounts(aData, aNbNodes, aNbTriangles, THE_HUGE_NB_NODES, anEncode));
  const std::string aHugePattern = anEncode(THE_HUGE_NB_NODES) + anEncode(aNbTriangles);
  const size_t      aPos         = aData.find(aHugePattern);
  ASSERT_NE(aPos, std::string::npos);
  aData.resize(aPos + aHugePattern.size());

  std::istringstream   anIn(aData);
  BinTools_ShapeReader aReader;
  aReader.SetWithTriangles(true);
  TopoDS_Shape aRead;
  std::string  aMessage;
  try
  {
    aReader.Read(anIn, aRead);
  }
  catch (const Standard_Failure& theFailure)
  {
    aMessage = theFailure.what();
  }
  EXPECT_NE(aMessage.find("larger than the stream"), std::string::npos) << aMessage;
}

namespace
{

//! Knots of a clamped cubic B-spline with theNbPoles poles: theNbPoles - 2 knots.
void cubicKnots(const int                   theNbPoles,
                NCollection_Array1<double>& theKnots,
                NCollection_Array1<int>&    theMults)
{
  theKnots.Resize(1, theNbPoles - 2, false);
  theMults.Resize(1, theNbPoles - 2, false);
  for (int anIter = 1; anIter <= theNbPoles - 2; ++anIter)
  {
    theKnots(anIter) = anIter;
    theMults(anIter) = (anIter == 1 || anIter == theNbPoles - 2) ? 4 : 1;
  }
}

//! Writes theShape, makes it declare theHuge instead of theFirst in the pair (theFirst, theSecond)
//! and returns the message of the failure of its reading.
std::string readWithHugeCount(const TopoDS_Shape& theShape,
                              const int           theFirst,
                              const int           theSecond,
                              const int           theHuge,
                              const int           theNewSecond = -1)
{
  std::ostringstream anOut;
  BinTools::Write(theShape, anOut, true, false, BinTools_FormatVersion_CURRENT);
  std::string aData    = anOut.str();
  auto        anEncode = [](const int theValue) {
    std::ostringstream aStream;
    BinTools::PutInteger(aStream, theValue);
    return aStream.str();
  };
  EXPECT_TRUE(replaceCounts(aData, theFirst, theSecond, theHuge, anEncode));
  if (theNewSecond >= 0)
  {
    EXPECT_TRUE(replaceCounts(aData, theSecond, 0, theNewSecond, anEncode, theHuge));
  }
  std::istringstream anIn(aData);
  TopoDS_Shape       aRead;
  try
  {
    BinTools::Read(aRead, anIn);
  }
  catch (const Standard_Failure& theFailure)
  {
    return theFailure.what();
  }
  return std::string();
}

const int THE_NB_POLES = 13;
const int THE_NB_KNOTS = 11;

} // namespace

// The numbers of poles and knots of a B-spline curve are limited by the stream.
TEST(BinTools_ReadLimitsTest, BSplineCurveLargerThanStream)
{
  NCollection_Array1<gp_Pnt> aPoles(1, THE_NB_POLES);
  for (int anIter = 1; anIter <= THE_NB_POLES; ++anIter)
  {
    aPoles(anIter) = gp_Pnt(anIter, anIter % 3, 0.0);
  }
  NCollection_Array1<double> aKnots;
  NCollection_Array1<int>    aMults;
  cubicKnots(THE_NB_POLES, aKnots, aMults);
  const TopoDS_Edge anEdge = BRepBuilderAPI_MakeEdge(
    occ::handle<Geom_Curve>(new Geom_BSplineCurve(aPoles, aKnots, aMults, 3)));
  const std::string aMessage =
    readWithHugeCount(anEdge, THE_NB_POLES, THE_NB_KNOTS, THE_HUGE_NB_NODES);
  EXPECT_NE(aMessage.find("larger than the stream"), std::string::npos) << aMessage;
}

// Same for a 2D curve.
TEST(BinTools_ReadLimitsTest, BSplineCurve2dLargerThanStream)
{
  NCollection_Array1<gp_Pnt2d> aPoles(1, THE_NB_POLES);
  for (int anIter = 1; anIter <= THE_NB_POLES; ++anIter)
  {
    aPoles(anIter) = gp_Pnt2d(anIter, anIter % 3);
  }
  NCollection_Array1<double> aKnots;
  NCollection_Array1<int>    aMults;
  cubicKnots(THE_NB_POLES, aKnots, aMults);
  const TopoDS_Edge anEdge = BRepBuilderAPI_MakeEdge(
    occ::handle<Geom2d_Curve>(new Geom2d_BSplineCurve(aPoles, aKnots, aMults, 3)),
    occ::handle<Geom_Surface>(new Geom_Plane(gp::XOY())));
  const std::string aMessage =
    readWithHugeCount(anEdge, THE_NB_POLES, THE_NB_KNOTS, THE_HUGE_NB_NODES);
  EXPECT_NE(aMessage.find("larger than the stream"), std::string::npos) << aMessage;
}

namespace
{

//! B-spline surface with theNbU x theNbV poles, rational if asked.
TopoDS_Face bsplineFace(const int theNbU, const int theNbV, const bool theIsRational)
{
  NCollection_Array2<gp_Pnt> aPoles(1, theNbU, 1, theNbV);
  NCollection_Array2<double> aWeights(1, theNbU, 1, theNbV);
  for (int aU = 1; aU <= theNbU; ++aU)
  {
    for (int aV = 1; aV <= theNbV; ++aV)
    {
      aPoles(aU, aV)   = gp_Pnt(aU, aV, (aU + aV) % 2);
      aWeights(aU, aV) = 1.0 + ((aU + aV) % 2) * 0.5;
    }
  }
  NCollection_Array1<double> aKnotsU, aKnotsV;
  NCollection_Array1<int>    aMultsU, aMultsV;
  cubicKnots(theNbU, aKnotsU, aMultsU);
  cubicKnots(theNbV, aKnotsV, aMultsV);
  occ::handle<Geom_Surface> aSurf;
  if (theIsRational)
  {
    aSurf = new Geom_BSplineSurface(aPoles, aWeights, aKnotsU, aKnotsV, aMultsU, aMultsV, 3, 3);
  }
  else
  {
    aSurf = new Geom_BSplineSurface(aPoles, aKnotsU, aKnotsV, aMultsU, aMultsV, 3, 3);
  }
  return BRepBuilderAPI_MakeFace(aSurf, 1.0e-7);
}

} // namespace

// The poles of a surface are a grid: each count alone fits in the stream, their product not.
TEST(BinTools_ReadLimitsTest, BSplineSurfaceGridLargerThanStream)
{
  const TopoDS_Face aFace    = bsplineFace(60, 40, false);
  const std::string aMessage = readWithHugeCount(aFace, 60, 40, 300, 300);
  EXPECT_NE(aMessage.find("larger than the stream"), std::string::npos) << aMessage;
}

// Guard: valid surfaces whose poles are measured against the stream (above 1 MiB) are read back.
TEST(BinTools_ReadLimitsTest, IntactLargeBSplineSurfaceRead)
{
  for (const bool isRational : {false, true})
  {
    const int          aNbU = 210, aNbV = 210;
    const TopoDS_Face  aFace = bsplineFace(aNbU, aNbV, isRational);
    std::ostringstream anOut;
    BinTools::Write(aFace, anOut, true, false, BinTools_FormatVersion_CURRENT);
    std::istringstream anIn(anOut.str());
    TopoDS_Shape       aRead;
    BinTools::Read(aRead, anIn);
    ASSERT_FALSE(aRead.IsNull());
    const occ::handle<Geom_BSplineSurface> aSurf =
      occ::down_cast<Geom_BSplineSurface>(BRep_Tool::Surface(TopoDS::Face(aRead)));
    ASSERT_FALSE(aSurf.IsNull());
    EXPECT_EQ(aSurf->NbUPoles(), aNbU);
    EXPECT_EQ(aSurf->NbVPoles(), aNbV);
    EXPECT_EQ(aSurf->IsURational() || aSurf->IsVRational(), isRational);
  }
}

// A number of bytes that does not fit in 64 bits is not available.
TEST(BinTools_ReadLimitsTest, IsAvailable_SizeOverflow)
{
  std::istringstream anIn("x");
  EXPECT_FALSE(BinTools::IsAvailable(anIn, 1 << 30, size_t(1) << 34));
  EXPECT_FALSE(BinTools::IsAvailable(anIn, -1, 1));
  EXPECT_TRUE(BinTools::IsAvailable(anIn, 1, 1));
}

// Guard: an intact stream is read with its triangulation.
TEST(BinTools_ReadLimitsTest, IntactStream_TriangulationRead)
{
  int                aNbNodes = 0, aNbTriangles = 0;
  const TopoDS_Face  aFace = meshedFace(aNbNodes, aNbTriangles);
  std::ostringstream anOut;
  BinTools::Write(aFace, anOut, true, false, BinTools_FormatVersion_CURRENT);
  std::istringstream anIn(anOut.str());
  TopoDS_Shape       aRead;
  BinTools::Read(aRead, anIn);
  ASSERT_FALSE(aRead.IsNull());
  TopLoc_Location                        aLoc;
  const occ::handle<Poly_Triangulation>& aTri = BRep_Tool::Triangulation(TopoDS::Face(aRead), aLoc);
  ASSERT_FALSE(aTri.IsNull());
  EXPECT_EQ(aTri->NbNodes(), aNbNodes);
  EXPECT_EQ(aTri->NbTriangles(), aNbTriangles);
}

// A damaged or crafted file replaces a section header by a word longer than the buffer it is
// read into: the reader reports a bad header instead of writing past the end of the buffer.
TEST(BinTools_ReadLimitsTest, ShapeSet_LongSectionHeader)
{
  int                aNbNodes = 0, aNbTriangles = 0;
  const TopoDS_Face  aFace = meshedFace(aNbNodes, aNbTriangles);
  std::ostringstream anOut;
  BinTools::Write(aFace, anOut, true, false, BinTools_FormatVersion_CURRENT);
  const std::string anIntact = anOut.str();

  const char* const aHeaders[] = {"Locations",
                                  "Curve2ds",
                                  "Curves",
                                  "Polygon3D",
                                  "PolygonOnTriangulations",
                                  "Surfaces",
                                  "Triangulations",
                                  "TShapes"};
  for (const char* aHeader : aHeaders)
  {
    // the header may follow binary data, so it is the first occurrence not preceded by a
    // letter ("Triangulations" is also the end of "PolygonOnTriangulations")
    const std::string aWord = std::string(aHeader) + " ";
    size_t            aPos  = anIntact.find(aWord);
    while (aPos != std::string::npos && aPos > 0 && isalpha((unsigned char)anIntact[aPos - 1]))
    {
      aPos = anIntact.find(aWord, aPos + 1);
    }
    ASSERT_NE(aPos, std::string::npos) << aHeader;
    std::string aData = anIntact;
    aData.replace(aPos, strlen(aHeader), std::string(2000, 'A'));

    std::istringstream anIn(aData);
    TopoDS_Shape       aRead;
    bool               isFailed = false;
    try
    {
      BinTools::Read(aRead, anIn);
    }
    catch (const Standard_Failure&)
    {
      isFailed = true;
    }
    EXPECT_TRUE(isFailed) << aHeader;
  }
}

namespace
{

//! Inserts theNb nested trimmed records (type byte, theNbReals reals 0 or 1, then the basis
//! geometry) before the first geometry of the section theHeader.
std::string nestTrimmed(const std::string& theData,
                        const char*        theHeader,
                        const uint8_t      theType,
                        const int          theNbReals,
                        const int          theNb)
{
  const std::string aWord = std::string(theHeader) + " ";
  size_t            aPos  = theData.find(aWord);
  while (aPos != std::string::npos && aPos > 0 && isalpha((unsigned char)theData[aPos - 1]))
  {
    aPos = theData.find(aWord, aPos + 1);
  }
  if (aPos == std::string::npos)
  {
    return std::string();
  }
  aPos = theData.find('\n', aPos) + 1;

  std::ostringstream aRecord;
  aRecord.put(char(theType));
  for (int i = 0; i < theNbReals; ++i)
  {
    BinTools::PutReal(aRecord, double(i % 2));
  }
  std::string aNested;
  for (int i = 0; i < theNb; ++i)
  {
    aNested += aRecord.str();
  }
  return theData.substr(0, aPos) + aNested + theData.substr(aPos);
}

} // namespace

// A damaged or crafted file nests trimmed curves and surfaces, which are read with their basis
// geometry: the reader stops at a fixed depth instead of overflowing the stack. A nesting written
// by OCCT stays readable.
TEST(BinTools_ReadLimitsTest, ShapeSet_NestedTrimmedGeometry)
{
  int                aNbNodes = 0, aNbTriangles = 0;
  const TopoDS_Face  aFace = meshedFace(aNbNodes, aNbTriangles);
  std::ostringstream anOut;
  BinTools::Write(aFace, anOut, false, false, BinTools_FormatVersion_CURRENT);
  const std::string anIntact = anOut.str();

  struct Section
  {
    const char* Header;
    uint8_t     Type;
    int         NbReals;
  };

  const Section aSections[] = {{"Curve2ds", 8, 2}, {"Curves", 8, 2}, {"Surfaces", 10, 4}};
  for (const Section& aSection : aSections)
  {
    for (const int aNb : {50, 100000})
    {
      const std::string aData =
        nestTrimmed(anIntact, aSection.Header, aSection.Type, aSection.NbReals, aNb);
      ASSERT_FALSE(aData.empty()) << aSection.Header;
      std::istringstream anIn(aData);
      TopoDS_Shape       aRead;
      std::string        aMessage;
      try
      {
        BinTools::Read(aRead, anIn);
      }
      catch (const Standard_Failure& theFailure)
      {
        aMessage = theFailure.what();
      }
      if (aNb == 50)
      {
        EXPECT_TRUE(aMessage.empty()) << aSection.Header << ": " << aMessage;
        EXPECT_FALSE(aRead.IsNull()) << aSection.Header;
      }
      else
      {
        EXPECT_NE(aMessage.find("nested too deeply"), std::string::npos)
          << aSection.Header << ": " << aMessage.substr(0, 300);
      }
    }
  }
}

namespace
{

//! Reads theData from thePosition with BinTools_ShapeReader and returns the failure message.
std::string readShapeFrom(const std::string& theData, const std::streamoff thePosition)
{
  std::istringstream anIn(theData);
  anIn.seekg(thePosition);
  BinTools_ShapeReader aReader;
  TopoDS_Shape         aShape;
  try
  {
    aReader.Read(anIn, aShape);
  }
  catch (const Standard_Failure& theFailure)
  {
    return theFailure.what();
  }
  return std::string();
}

//! Bytes of a 32-bit integer and of a 64-bit unsigned integer as BinTools_OStream writes them.
std::string int32Bytes(const int theValue)
{
  return std::string(reinterpret_cast<const char*>(&theValue), sizeof(int));
}

std::string uint64Bytes(const uint64_t theValue)
{
  return std::string(reinterpret_cast<const char*>(&theValue), sizeof(uint64_t));
}

//! Simple location record: a translation by (theX, theY, 0).
std::string simpleLocation(const double theX, const double theY)
{
  const double aValues[12] = {1., 0., 0., theX, 0., 1., 0., theY, 0., 0., 1., 0.};
  return std::string(1, char(BinTools_ObjectType_SimpleLocation))
         + std::string(reinterpret_cast<const char*>(aValues), sizeof(aValues));
}

//! Reads a location from theData with BinTools_ShapeReader and returns the failure message.
std::string readLocationFrom(const std::string& theData)
{
  std::istringstream   anIn(theData);
  BinTools_IStream     aStream(anIn);
  BinTools_ShapeReader aReader;
  try
  {
    aReader.ReadLocation(aStream);
  }
  catch (const Standard_Failure& theFailure)
  {
    return theFailure.what();
  }
  return std::string();
}

} // namespace

// A damaged or crafted stream of BinTools_ShapeWriter (binary OCAF documents) leads the reader
// into a cycle of references: the reader stops instead of following it until the stack overflows.
TEST(BinTools_ReadLimitsTest, ShapeReader_CyclicReference)
{
  const char aRef8 = char(BinTools_ObjectType_Reference8);

  // a 64-bit delta that wraps around points to a later reference, which points back
  {
    std::string aData(1, char(BinTools_ObjectType_Reference64));
    aData += uint64Bytes(uint64_t(0) - 9); // target 9: 1 - delta - 1
    aData += aRef8;
    aData += char(9); // target 0: 10 - 9 - 1
    EXPECT_NE(readShapeFrom(aData, 0).find("later position"), std::string::npos);
  }

  // a sub-shape refers to its compound, which is bound only after its sub-shapes
  {
    std::string aData(1, char(BinTools_ObjectType_EndShape + 1)); // compound, forward
    aData += char(BinTools_ObjectType_EmptyLocation);
    aData += char(0); // flags
    aData += aRef8;
    aData += char(3); // target 0: 4 - 3 - 1
    EXPECT_NE(readShapeFrom(aData, 0).find("nested too deeply"), std::string::npos);
  }

  // an item of a location refers to the location record
  {
    std::string aData(1, char(BinTools_ObjectType_Location));
    aData += aRef8;
    aData += char(1); // target 0: 2 - 1 - 1
    EXPECT_NE(readLocationFrom(aData).find("nested too deeply"), std::string::npos);
  }

  // a long chain of references, each to the previous one
  {
    const int   aNb = 100000;
    std::string aChain(1, char(BinTools_ObjectType_EmptyShape));
    for (int i = 1; i < aNb; ++i)
    {
      aChain += aRef8;
      aChain += char(i == 1 ? 1 : 2);
    }
    EXPECT_NE(readShapeFrom(aChain, 2 * (aNb - 1) - 1).find("nested too deeply"),
              std::string::npos);
  }
}

// A damaged or crafted stream composes a location of a composed location raised to a large power:
// TopLoc_Location::Powered recursed once per unit of the power.
TEST(BinTools_ReadLimitsTest, ShapeReader_ComposedLocationItem)
{
  std::string anInner(1, char(BinTools_ObjectType_Location));
  anInner += simpleLocation(1., 0.) + int32Bytes(1);
  anInner += simpleLocation(0., 1.) + int32Bytes(1);
  anInner += char(BinTools_ObjectType_LocationEnd);

  std::string anOuter(1, char(BinTools_ObjectType_Location));
  anOuter += anInner + int32Bytes(1000000);
  anOuter += char(BinTools_ObjectType_LocationEnd);
  EXPECT_NE(readLocationFrom(anOuter).find("not elementary"), std::string::npos);

  // guard: the inner location alone, as OCCT writes it, is read
  EXPECT_TRUE(readLocationFrom(anInner).empty());
}

// A location with a very long chain of items is rejected by both binary readers; a long chain
// that OCCT models can have is read back.
TEST(BinTools_ReadLimitsTest, LongLocationChain)
{
  gp_Trsf aT1, aT2;
  aT1.SetTranslation(gp_Vec(1., 0., 0.));
  aT2.SetTranslation(gp_Vec(0., 1., 0.));
  const TopLoc_Location aL1(aT1), aL2(aT2);
  int                   aNbNodes = 0, aNbTriangles = 0;
  const TopoDS_Face     aFace = meshedFace(aNbNodes, aNbTriangles);
  for (const int aNbItems : {900, 1001})
  {
    TopLoc_Location aLoc;
    for (int i = 0; i < aNbItems; ++i)
    {
      aLoc = aLoc * (i % 2 == 0 ? aL1 : aL2);
    }
    const TopoDS_Shape aShape = aFace.Located(aLoc);

    std::ostringstream anOut1;
    BinTools::Write(aShape, anOut1, false, false, BinTools_FormatVersion_CURRENT);
    std::istringstream anIn1(anOut1.str());
    std::string        aMessage1;
    TopoDS_Shape       aRead1;
    try
    {
      BinTools::Read(aRead1, anIn1);
    }
    catch (const Standard_Failure& theFailure)
    {
      aMessage1 = theFailure.what();
    }

    std::ostringstream   anOut2;
    BinTools_ShapeWriter aWriter;
    aWriter.Write(aShape, anOut2);
    const std::string aMessage2 = readShapeFrom(anOut2.str(), 0);

    if (aNbItems == 900)
    {
      EXPECT_TRUE(aMessage1.empty()) << aMessage1;
      EXPECT_TRUE(aMessage2.empty()) << aMessage2;
      // datums read back are new objects: compare the transformations
      EXPECT_LT((aRead1.Location().Transformation().TranslationPart()
                 - aLoc.Transformation().TranslationPart())
                  .Modulus(),
                1.0e-9);
    }
    else
    {
      EXPECT_NE(aMessage1.find("too many items"), std::string::npos) << aMessage1;
      EXPECT_NE(aMessage2.find("too many items"), std::string::npos) << aMessage2;
    }
  }
}

// Guard: compounds nested deeper than in usual models are written and read back.
TEST(BinTools_ReadLimitsTest, ShapeReader_IntactNestedCompounds)
{
  BRep_Builder aBuilder;
  int          aNbNodes = 0, aNbTriangles = 0;
  TopoDS_Shape aChild = meshedFace(aNbNodes, aNbTriangles);
  const int    aDepth = 150;
  for (int i = 0; i < aDepth; ++i)
  {
    TopoDS_Compound aParent;
    aBuilder.MakeCompound(aParent);
    aBuilder.Add(aParent, aChild);
    aChild = aParent;
  }
  std::ostringstream   anOut;
  BinTools_ShapeWriter aWriter;
  aWriter.Write(aChild, anOut);

  std::istringstream   anIn(anOut.str());
  BinTools_ShapeReader aReader;
  TopoDS_Shape         aRead;
  aReader.Read(anIn, aRead);
  int aLevels = 0;
  for (TopoDS_Shape aShape = aRead; aShape.ShapeType() == TopAbs_COMPOUND; ++aLevels)
  {
    TopoDS_Iterator anIt(aShape);
    ASSERT_TRUE(anIt.More());
    aShape = anIt.Value();
  }
  EXPECT_EQ(aLevels, aDepth);
}

namespace
{

//! Encoding of an integer by BinTools_OStream (shapes of binary OCAF documents).
std::string streamInteger(const int theValue)
{
  std::ostringstream aStream;
  BinTools_OStream   aBinStream(aStream);
  aBinStream << theValue;
  return aStream.str();
}

//! Encoding of an integer by BinTools::PutInteger (BinTools_ShapeSet).
std::string putInteger(const int theValue)
{
  std::ostringstream aStream;
  BinTools::PutInteger(aStream, theValue);
  return aStream.str();
}

//! Message of the failure of reading theData by BinTools_ShapeReader.
std::string readerFailure(const std::string& theData)
{
  std::istringstream   anIn(theData);
  BinTools_ShapeReader aReader;
  TopoDS_Shape         aRead;
  try
  {
    aReader.Read(anIn, aRead);
  }
  catch (const Standard_Failure& theFailure)
  {
    return theFailure.what();
  }
  return std::string();
}

//! Message of the failure of reading theData by BinTools::Read.
std::string shapeSetFailure(const std::string& theData)
{
  std::istringstream anIn(theData);
  TopoDS_Shape       aRead;
  try
  {
    BinTools::Read(aRead, anIn);
  }
  catch (const Standard_Failure& theFailure)
  {
    return theFailure.what();
  }
  return std::string();
}

//! Edge on a cubic B-spline curve of THE_NB_POLES poles.
TopoDS_Edge bsplineEdge()
{
  NCollection_Array1<gp_Pnt> aPoles(1, THE_NB_POLES);
  for (int anIter = 1; anIter <= THE_NB_POLES; ++anIter)
  {
    aPoles(anIter) = gp_Pnt(anIter, anIter % 3, 0.0);
  }
  NCollection_Array1<double> aKnots;
  NCollection_Array1<int>    aMults;
  cubicKnots(THE_NB_POLES, aKnots, aMults);
  return BRepBuilderAPI_MakeEdge(
    occ::handle<Geom_Curve>(new Geom_BSplineCurve(aPoles, aKnots, aMults, 3)));
}

} // namespace

// The number of knots of a B-spline curve, which follows the number of poles, is limited by the
// stream as well.
TEST(BinTools_ReadLimitsTest, BSplineCurveKnotsLargerThanStream)
{
  std::ostringstream anOut;
  BinTools::Write(bsplineEdge(), anOut, true, false, BinTools_FormatVersion_CURRENT);
  std::string aData = anOut.str();
  ASSERT_TRUE(replaceCounts(aData, THE_NB_KNOTS, 0, THE_HUGE_NB_NODES, putInteger, THE_NB_POLES));
  const std::string aMessage = shapeSetFailure(aData);
  EXPECT_NE(aMessage.find("larger than the stream"), std::string::npos) << aMessage;
}

// The degrees of a Bezier surface, two 16-bit values, alone can ask for (2^16)^2 poles.
TEST(BinTools_ReadLimitsTest, BezierSurfaceLargerThanStream)
{
  NCollection_Array2<gp_Pnt> aPoles(1, 8, 1, 10);
  for (int aU = 1; aU <= 8; ++aU)
  {
    for (int aV = 1; aV <= 10; ++aV)
    {
      aPoles(aU, aV) = gp_Pnt(aU, aV, (aU * aV) % 3);
    }
  }
  const TopoDS_Face aFace =
    BRepBuilderAPI_MakeFace(occ::handle<Geom_Surface>(new Geom_BezierSurface(aPoles)), 1.0e-7);
  std::ostringstream anOut;
  BinTools::Write(aFace, anOut, true, false, BinTools_FormatVersion_CURRENT);
  std::string aData     = anOut.str();
  auto        anExtChar = [](const int theValue) {
    std::ostringstream aStream;
    BinTools::PutExtChar(aStream, static_cast<char16_t>(theValue));
    return aStream.str();
  };
  ASSERT_TRUE(replaceCounts(aData, 7, 9, 60000, anExtChar));
  const std::string aMessage = shapeSetFailure(aData);
  EXPECT_NE(aMessage.find("Bezier surface: larger than the stream"), std::string::npos) << aMessage;
}

// The geometry of a binary OCAF document is read by BinTools_ShapeReader through the same
// guards: a curve and a surface that declare more poles than the stream holds.
TEST(BinTools_ReadLimitsTest, ShapeReader_GeometryLargerThanStream)
{
  {
    std::ostringstream   anOut;
    BinTools_ShapeWriter aWriter;
    aWriter.Write(bsplineEdge(), anOut);
    std::string aData = anOut.str();
    ASSERT_TRUE(replaceCounts(aData, THE_NB_POLES, THE_NB_KNOTS, THE_HUGE_NB_NODES, streamInteger));
    const std::string aMessage = readerFailure(aData);
    EXPECT_NE(aMessage.find("larger than the stream"), std::string::npos) << aMessage;
  }
  {
    std::ostringstream   anOut;
    BinTools_ShapeWriter aWriter;
    aWriter.Write(bsplineFace(60, 40, false), anOut);
    std::string aData = anOut.str();
    ASSERT_TRUE(replaceCounts(aData, 60, 40, 300, streamInteger));
    ASSERT_TRUE(replaceCounts(aData, 40, 0, 300, streamInteger, 300));
    const std::string aMessage = readerFailure(aData);
    EXPECT_NE(aMessage.find("larger than the stream"), std::string::npos) << aMessage;
  }
}
