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

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRep_Tool.hxx>
#include <BinTools.hxx>
#include <BinTools_OStream.hxx>
#include <BinTools_ShapeReader.hxx>
#include <BinTools_ShapeWriter.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>

#include <BRepMesh_IncrementalMesh.hxx>

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
                   Encode       theEncode)
{
  const std::string aPattern = theEncode(theNbNodes) + theEncode(theNbTriangles);
  const size_t      aPos     = theData.find(aPattern);
  if (aPos == std::string::npos || theData.find(aPattern, aPos + 1) != std::string::npos)
  {
    return false;
  }
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
