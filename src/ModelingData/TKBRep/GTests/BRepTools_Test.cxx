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

#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <TCollection_AsciiString.hxx>
#include <TopTools_LocationSet.hxx>
#include <TopoDS_Edge.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_Array1.hxx>
#include <OSD_Parallel.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <Poly_Triangulation.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BinTools.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Vec.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <BRep_Tool.hxx>

#include <gtest/gtest.h>

#include <Standard_Failure.hxx>

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>

namespace
{

// Write a box to a BRep string once, to be reused by all threads.
std::string MakeBoxBrepString()
{
  BRepPrimAPI_MakeBox aBox(10.0, 20.0, 30.0);
  EXPECT_FALSE(aBox.Shape().IsNull());

  std::ostringstream anOss;
  BRepTools::Write(aBox.Shape(), anOss);
  EXPECT_FALSE(anOss.str().empty());
  return anOss.str();
}

// Read a shape from a BRep buffer. Each call uses its own stream.
bool ReadFromBuffer(const std::string& theData, TopoDS_Shape& theShape)
{
  std::istringstream anIss(theData);
  BRep_Builder       aBuilder;
  BRepTools::Read(theShape, anIss, aBuilder);
  return !theShape.IsNull();
}

} // namespace

// Parallel BRep reading must produce shapes with identical volume properties.
// The underlying bug was that Strtod() shared a single Bigint memory pool
// across all threads, causing random mis-parses of numeric values in .brep
// files when read concurrently.
TEST(BRepTools_Test, ParallelRead_IdenticalMass)
{
  const std::string aBrepData = MakeBoxBrepString();

  // Single-threaded reference
  TopoDS_Shape aRefShape;
  ASSERT_TRUE(ReadFromBuffer(aBrepData, aRefShape));
  GProp_GProps aRefProps;
  BRepGProp::VolumeProperties(aRefShape, aRefProps);
  const double aRefMass = std::abs(aRefProps.Mass());
  ASSERT_GT(aRefMass, 0.0);

  constexpr int aNbReads = 200;

  NCollection_Array1<TopoDS_Shape> aShapes(0, aNbReads - 1);
  std::atomic<int>                 aFailCount{0};

  OSD_Parallel::For(0, aNbReads, [&](int theIndex) {
    TopoDS_Shape aShape;
    if (!ReadFromBuffer(aBrepData, aShape))
    {
      aFailCount.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    aShapes(theIndex) = aShape;
  });

  EXPECT_EQ(aFailCount.load(), 0) << "Some parallel reads failed";

  constexpr double aEps = 1.0e-6;
  for (int i = 0; i < aNbReads; ++i)
  {
    ASSERT_FALSE(aShapes(i).IsNull()) << "Shape " << i << " is null";
    GProp_GProps aProps;
    BRepGProp::VolumeProperties(aShapes(i), aProps);
    const double aMass = std::abs(aProps.Mass());
    EXPECT_NEAR(aMass, aRefMass, aEps)
      << "Shape " << i << " mass " << aMass << " != reference " << aRefMass;
  }
}

// Repeated parallel runs to catch intermittent failures.
TEST(BRepTools_Test, ParallelRead_RepeatedRuns)
{
  const std::string aBrepData = MakeBoxBrepString();

  TopoDS_Shape aRefShape;
  ASSERT_TRUE(ReadFromBuffer(aBrepData, aRefShape));
  GProp_GProps aRefProps;
  BRepGProp::VolumeProperties(aRefShape, aRefProps);
  const double aRefMass = std::abs(aRefProps.Mass());

  constexpr int    aNbReads = 100;
  constexpr int    aNbRuns  = 5;
  constexpr double aEps     = 1.0e-6;

  for (int aRun = 0; aRun < aNbRuns; ++aRun)
  {
    NCollection_Array1<TopoDS_Shape> aShapes(0, aNbReads - 1);
    std::atomic<int>                 aFailCount{0};

    OSD_Parallel::For(0, aNbReads, [&](int theIndex) {
      TopoDS_Shape aShape;
      if (!ReadFromBuffer(aBrepData, aShape))
      {
        aFailCount.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      aShapes(theIndex) = aShape;
    });

    EXPECT_EQ(aFailCount.load(), 0) << "Run " << aRun << ": some reads failed";

    for (int i = 0; i < aNbReads; ++i)
    {
      ASSERT_FALSE(aShapes(i).IsNull()) << "Run " << aRun << ", shape " << i << " is null";
      GProp_GProps aProps;
      BRepGProp::VolumeProperties(aShapes(i), aProps);
      const double aMass = std::abs(aProps.Mass());
      EXPECT_NEAR(aMass, aRefMass, aEps) << "Run " << aRun << ", shape " << i << " mass mismatch";
    }
  }
}

TEST(BRepToolsTest, OversizedRealTokenRejectsShapeRead)
{
  const TopoDS_Edge anEdge = BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(1, 0, 0)).Edge();
  std::stringstream aWritten;
  BRepTools::Write(anEdge, aWritten);
  TCollection_AsciiString       aRecord(aWritten.str().c_str());
  const TCollection_AsciiString aPrefix("Curves 1\n1 ");
  const int                     aPosition = aRecord.Search(aPrefix);
  ASSERT_GE(aPosition, 1);
  aRecord.Insert(aPosition + aPrefix.Length(), TCollection_AsciiString(512, '0'));

  std::istringstream aStream(aRecord.ToCString());
  TopoDS_Shape       aRead;
  EXPECT_NO_THROW(BRepTools::Read(aRead, aStream, BRep_Builder()));
  EXPECT_TRUE(aStream.fail());
  EXPECT_TRUE(aRead.IsNull());
}

TEST(BRepToolsTest, OversizedLocationValueDoesNotAddTransform)
{
  TopTools_LocationSet          aLocations;
  const TCollection_AsciiString aHeader("Locations 1\n1\n");
  TCollection_AsciiString       aRecord(aHeader);
  aRecord += TCollection_AsciiString(510, '0');
  aRecord += "1 0 0 4\n0 1 0 0\n0 0 1 0\n";

  std::istringstream aValid(aRecord.ToCString());
  aLocations.Read(aValid);
  ASSERT_FALSE(aValid.fail());
  ASSERT_FALSE(aLocations.Location(1).IsIdentity());
  EXPECT_DOUBLE_EQ(aLocations.Location(1).Transformation().TranslationPart().X(), 4.0);

  aRecord.Insert(aHeader.Length() + 1, "0");
  std::istringstream anOversized(aRecord.ToCString());
  EXPECT_NO_THROW(aLocations.Read(anOversized));
  EXPECT_TRUE(anOversized.fail());
  EXPECT_TRUE(aLocations.Location(1).IsIdentity());
}

// Tolerances and locations were written with 15 significant digits, so a text round trip changed
// them in the last bits (geometry already used 17). The tolerance must exceed the default 1e-7:
// UpdateVertex() only raises a tolerance.
TEST(BRepTools_Test, WriteRead_TolerancesExactLocationPrecise)
{
  const double aTol = 1.0e-5 / 3.0;
  TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  BRep_Builder aBuilder;
  for (TopExp_Explorer anExp(aBox, TopAbs_VERTEX); anExp.More(); anExp.Next())
  {
    aBuilder.UpdateVertex(TopoDS::Vertex(anExp.Current()), aTol);
  }
  gp_Trsf aTrsf;
  aTrsf.SetRotation(gp_Ax1(gp_Pnt(1.0 / 3.0, 0.0, 0.0), gp_Dir(1.0, 2.0, 3.0)), 0.1);
  aTrsf.SetTranslationPart(gp_Vec(100.0 / 3.0, -2.0 / 7.0, 1.0e-3 / 3.0));
  aBox.Move(TopLoc_Location(aTrsf));

  std::ostringstream anOut;
  BRepTools::Write(aBox, anOut);
  std::istringstream anIn(anOut.str());
  TopoDS_Shape       aRead;
  BRepTools::Read(aRead, anIn, aBuilder);
  ASSERT_FALSE(aRead.IsNull());

  int aNbVertices = 0;
  for (TopExp_Explorer anExp(aRead, TopAbs_VERTEX); anExp.More(); anExp.Next(), ++aNbVertices)
  {
    EXPECT_EQ(BRep_Tool::Tolerance(TopoDS::Vertex(anExp.Current())), aTol);
  }
  EXPECT_GT(aNbVertices, 0);
  const gp_Trsf aReadTrsf = aRead.Location().Transformation();
  double        aMaxDiff  = 0.0;
  for (int aRow = 1; aRow <= 3; ++aRow)
  {
    for (int aCol = 1; aCol <= 4; ++aCol)
    {
      aMaxDiff =
        std::max(aMaxDiff, std::abs(aReadTrsf.Value(aRow, aCol) - aTrsf.Value(aRow, aCol)));
    }
  }
  // Read() rebuilds the transformation with gp_Trsf::SetValues(), which renormalises the matrix,
  // so exact bits are not reachable; 15 digits gave 3.6e-14, 17 digits 1.1e-16.
  EXPECT_LE(aMaxDiff, 1.0e-15);
}

// Values that 15 digits restore keep their 15-digit text, so files of ordinary shapes do not
// change (Draw test bugs/moddata_3/bug33318 compares the text with a reference file).
TEST(BRepTools_Test, Write_RestorableValuesKeep15Digits)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  std::ostringstream anOut;
  BRepTools::Write(aBox, anOut);
  const std::string aText = anOut.str();
  EXPECT_NE(aText.find(" 1e-07"), std::string::npos);
  EXPECT_EQ(aText.find("9.9999999999999995e-08"), std::string::npos);
  EXPECT_EQ(anOut.precision(), 6);
}

namespace
{
//! True if every face of theShape has a triangulation with normals.
bool allFacesHaveNormals(const TopoDS_Shape& theShape)
{
  int aNbFaces = 0;
  for (TopExp_Explorer anExp(theShape, TopAbs_FACE); anExp.More(); anExp.Next(), ++aNbFaces)
  {
    TopLoc_Location                        aLoc;
    const occ::handle<Poly_Triangulation>& aTri =
      BRep_Tool::Triangulation(TopoDS::Face(anExp.Current()), aLoc);
    if (aTri.IsNull() || !aTri->HasNormals())
    {
      return false;
    }
  }
  return aNbFaces > 0;
}

//! Meshed box with normals on every face triangulation. Several faces are needed: an edge shared by
//! two faces carries a polygon on the neighbour's triangulation, which is how that triangulation
//! got registered before its own face (a single-face sphere does not show the defect).
TopoDS_Shape meshedBoxWithNormals()
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  BRepMesh_IncrementalMesh(aBox, 0.5);
  for (TopExp_Explorer anExp(aBox, TopAbs_FACE); anExp.More(); anExp.Next())
  {
    TopLoc_Location aLoc;
    BRep_Tool::Triangulation(TopoDS::Face(anExp.Current()), aLoc)->ComputeNormals();
  }
  return aBox;
}
} // namespace

// Edges are registered before their faces; the edge registered the shared triangulation
// "without normals" first, so normals requested for the face were silently dropped.
TEST(BRepTools_Test, WriteRead_TriangulationNormalsKept_Text)
{
  const TopoDS_Shape aShape = meshedBoxWithNormals();
  ASSERT_TRUE(allFacesHaveNormals(aShape));

  std::ostringstream anOut;
  BRepTools::Write(aShape, anOut, true, true, TopTools_FormatVersion_CURRENT);
  std::istringstream anIn(anOut.str());
  TopoDS_Shape       aRead;
  BRepTools::Read(aRead, anIn, BRep_Builder());
  EXPECT_TRUE(allFacesHaveNormals(aRead));
}

TEST(BRepTools_Test, WriteRead_TriangulationNormalsKept_Binary)
{
  const TopoDS_Shape aShape = meshedBoxWithNormals();
  ASSERT_TRUE(allFacesHaveNormals(aShape));

  std::ostringstream anOut(std::ios::binary);
  BinTools::Write(aShape, anOut, true, true, BinTools_FormatVersion_CURRENT);
  std::istringstream anIn(anOut.str(), std::ios::binary);
  TopoDS_Shape       aRead;
  BinTools::Read(aRead, anIn);
  EXPECT_TRUE(allFacesHaveNormals(aRead));
}


// A damaged or crafted file replaces a word of the text format by a word longer than the buffer
// it is read into: the reader rejects the file instead of writing past the end of the buffer.
TEST(BRepTools_Test, Read_LongWord_Text)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  BRepMesh_IncrementalMesh(aBox, 0.5);
  std::ostringstream anOut;
  BRepTools::Write(aBox, anOut, true, true, TopTools_FormatVersion_CURRENT);
  const std::string anIntact = anOut.str();

  struct Word
  {
    const char* Line;     //!< the line start to find, with the character that follows the word
    bool        IsLast;   //!< search from the end of the file
    const char* Prefix;   //!< kept before the long word
    bool        MustFail; //!< a wrong section header or reference rejects the whole file
  };

  // section headers, a shape type, the shape flags, the marker of a polygon on triangulation and
  // the reference of the root shape
  const Word aWords[] = {{"Locations ", false, "", true},
                         {"Curve2ds ", false, "", true},
                         {"Curves ", false, "", true},
                         {"Polygon3D ", false, "", true},
                         {"PolygonOnTriangulations ", false, "", true},
                         {"Surfaces ", false, "", true},
                         {"Triangulations ", false, "", true},
                         {"TShapes ", false, "", true},
#ifndef No_Exception
                         // these lead to a shape index read from the file, which is checked only
                         // when exceptions are enabled
                         {"Fa\n", false, "", false},
                         {"0101000\n", false, "", false},
                         {"+1 ", true, "+", true},
#endif
                         {"p ", false, "", false}};
  for (const Word& aWord : aWords)
  {
    const std::string aLine = std::string("\n") + aWord.Line;
    const size_t      aPos  = aWord.IsLast ? anIntact.rfind(aLine) : anIntact.find(aLine);
    ASSERT_NE(aPos, std::string::npos) << aWord.Line;
    std::string aData = anIntact;
    aData.replace(aPos + 1,
                  strlen(aWord.Line) - 1,
                  std::string(aWord.Prefix) + std::string(2000, 'A'));

    std::istringstream anIn(aData);
    TopoDS_Shape       aRead;
    bool               isFailed = false;
    try
    {
      BRepTools::Read(aRead, anIn, BRep_Builder());
    }
    catch (const Standard_Failure&)
    {
      isFailed = true;
    }
    // without the bound, the long word is written past the end of the buffer and the process
    // stops before this point
    if (aWord.MustFail)
    {
      EXPECT_TRUE(isFailed || aRead.IsNull()) << aWord.Line;
    }
  }
}


// A damaged or crafted file nests trimmed curves and surfaces, which are read with their basis
// geometry: the reader stops at a fixed depth instead of overflowing the stack. A nesting written
// by OCCT stays readable.
TEST(BRepTools_Test, Read_NestedTrimmedGeometry_Text)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  std::ostringstream anOut;
  BRepTools::Write(aBox, anOut, false, false, TopTools_FormatVersion_CURRENT);
  const std::string anIntact = anOut.str();

  struct Section
  {
    const char* Header;
    const char* Record;
  };

  const Section aSections[] = {{"\nCurve2ds ", "8 0 1\n"},
                               {"\nCurves ", "8 0 1\n"},
                               {"\nSurfaces ", "10 0 1 0 1\n"}};
  for (const Section& aSection : aSections)
  {
    for (const int aNb : {50, 100000})
    {
      size_t aPos = anIntact.find(aSection.Header);
      ASSERT_NE(aPos, std::string::npos) << aSection.Header;
      aPos = anIntact.find('\n', aPos + 1) + 1;
      std::string aNested;
      for (int i = 0; i < aNb; ++i)
      {
        aNested += aSection.Record;
      }
      std::istringstream anIn(anIntact.substr(0, aPos) + aNested + anIntact.substr(aPos));
      TopoDS_Shape       aRead;
      bool               isFailed = false;
      try
      {
        BRepTools::Read(aRead, anIn, BRep_Builder());
      }
      catch (const Standard_Failure&)
      {
        isFailed = true;
      }
      // without the limit, the deep nesting stops the process before this point
      if (aNb == 50)
      {
        EXPECT_FALSE(isFailed) << aSection.Header;
        EXPECT_FALSE(aRead.IsNull()) << aSection.Header;
      }
      else
      {
        // the table stops at the entry that is not read, instead of shifting the next ones
        EXPECT_TRUE(isFailed) << aSection.Header;
      }
    }
  }
}

// A damaged or crafted file composes a location of a composed location raised to a large power,
// or of a very long chain of items: TopLoc_Location::Powered and Multiplied recursed as deep as the
// file asked. Such a location is rejected.
TEST(BRepTools_Test, Read_ComposedLocation_Text)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  std::ostringstream anOut;
  BRepTools::Write(aBox, anOut, false, false, TopTools_FormatVersion_CURRENT);
  const std::string anIntact = anOut.str();
  const std::string anEmpty  = "\nLocations 0\n";
  const size_t      aPos     = anIntact.find(anEmpty);
  ASSERT_NE(aPos, std::string::npos);

  // two translations, then the composed location
  const std::string anElementary = "1\n1 0 0 1\n0 1 0 0\n0 0 1 0\n1\n1 0 0 0\n0 1 0 1\n0 0 1 0\n";
  std::string       aLong        = "2";
  for (int i = 0; i < 10000; ++i)
  {
    aLong += " 1 1 2 1";
  }
  const std::string aTables[] = {"\nLocations 4\n" + anElementary + "2 1 1 2 1 0\n2 3 1000000 0\n",
                                 "\nLocations 3\n" + anElementary + aLong + " 0\n"};
  for (const std::string& aTable : aTables)
  {
    std::string aData = anIntact;
    aData.replace(aPos, anEmpty.size(), aTable);
    std::istringstream anIn(aData);
    TopoDS_Shape       aRead;
    bool               isFailed = false;
    try
    {
      BRepTools::Read(aRead, anIn, BRep_Builder());
    }
    catch (const Standard_Failure&)
    {
      isFailed = true;
    }
    EXPECT_TRUE(isFailed) << aTable.substr(0, 40);
  }
}
