// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <RWStl.hxx>
#include <StlAPI_Writer.hxx>

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopExp_Explorer.hxx>
#include <Poly_Triangulation.hxx>
#include <NCollection_Sequence.hxx>
#include <gp_Pnt.hxx>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>

namespace
{
//! Numeric punctuation of locales that write "0,5" (e.g. de_DE).
class CommaDecimalPoint : public std::numpunct<char>
{
protected:
  char do_decimal_point() const override { return ','; }
};
} // namespace

// The ASCII writer used the stream locale, so a comma locale produced "0,5", which no STL reader
// accepts.
TEST(RWStlTest, WriteAscii_CommaLocale_WritesDecimalPoint)
{
  occ::handle<Poly_Triangulation> aMesh = new Poly_Triangulation(3, 1, false);
  aMesh->SetNode(1, gp_Pnt(0.5, 0.0, 0.0));
  aMesh->SetNode(2, gp_Pnt(1.5, 0.0, 0.0));
  aMesh->SetNode(3, gp_Pnt(0.5, 2.5, 0.0));
  aMesh->SetTriangle(1, Poly_Triangle(1, 2, 3));

  std::ostringstream aStream;
  aStream.imbue(std::locale(std::locale::classic(), new CommaDecimalPoint()));
  ASSERT_TRUE(RWStl::WriteAscii(aMesh, aStream));

  const std::string aText = aStream.str();
  EXPECT_EQ(aText.find(','), std::string::npos) << aText;
  EXPECT_NE(aText.find("vertex 5.000000000000e-01 0.000000000000e+00"), std::string::npos) << aText;
  // The caller's locale is restored
  EXPECT_EQ(std::use_facet<std::numpunct<char>>(aStream.getloc()).decimal_point(), ',');
}

// Faces without triangulation are skipped; Write() still succeeds, so the count tells the caller
// that the file misses part of the shape.
TEST(StlAPI_WriterTest, Write_PartlyMeshed_ReportsSkippedFaces)
{
  const TopoDS_Shape aBox   = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
  int                aIndex = 0;
  for (TopExp_Explorer anExp(aBox, TopAbs_FACE); anExp.More(); anExp.Next(), ++aIndex)
  {
    if (aIndex < 3)
    {
      BRepMesh_IncrementalMesh(anExp.Current(), 0.1);
    }
  }
  StlAPI_Writer      aWriter;
  std::ostringstream aStream;
  EXPECT_TRUE(aWriter.Write(aBox, aStream));
  EXPECT_EQ(aWriter.NbSkippedFaces(), 3);

  BRepMesh_IncrementalMesh(aBox, 0.1);
  std::ostringstream aStream2;
  EXPECT_TRUE(aWriter.Write(aBox, aStream2));
  EXPECT_EQ(aWriter.NbSkippedFaces(), 0);
}

// File names are UTF-8 in OCCT; they were passed to std::ofstream, which uses the ANSI code page
// on Windows, so a Japanese or emoji name could not be written.
TEST(StlAPI_WriterTest, Write_Utf8FileName)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
  BRepMesh_IncrementalMesh(aBox, 0.1);
  for (const bool isAscii : {true, false})
  {
    const std::filesystem::path aPath =
      std::filesystem::temp_directory_path()
      / std::filesystem::u8path(isAscii ? "occt_stl_\xE6\x97\xA5\xE6\x9C\xAC_\xF0\x9F\x98\x80.stl"
                                        : "occt_stl_\xE6\x97\xA5\xE6\x9C\xAC_bin.stl");
    StlAPI_Writer aWriter;
    aWriter.ASCIIMode() = isAscii;
    EXPECT_TRUE(aWriter.Write(aBox, aPath.u8string().c_str())) << (isAscii ? "ascii" : "binary");
    // The ANSI path still creates a file, but under a mis-decoded name: check the real name exists
    // (file_size() with an error code returns -1 for a missing file, which would pass a > 0 check).
    std::error_code anError;
    EXPECT_TRUE(std::filesystem::exists(aPath, anError)) << (isAscii ? "ascii" : "binary");
    if (std::filesystem::exists(aPath, anError))
    {
      EXPECT_GT(std::filesystem::file_size(aPath), 0u);
    }
    std::filesystem::remove(aPath, anError);
  }
}

// A text STL file of several solids was read up to the end of the first solid when the file was
// smaller than the read buffer: the stream was already at its end, the other solids were in the
// buffer.
TEST(RWStlTest, ReadFile_AsciiSeveralSolids)
{
  const std::filesystem::path aPath =
    std::filesystem::temp_directory_path() / "occt_rwstl_several_solids.stl";
  {
    std::ofstream anOut(aPath, std::ios::binary);
    anOut << "solid a\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\n"
             "endloop\nendfacet\nendsolid a\n"
             "\n"
             "solid b\nfacet normal 1 0 0\nouter loop\nvertex 5 0 0\nvertex 5 1 0\nvertex 5 0 1\n"
             "endloop\nendfacet\nendsolid b\n"
             "solid c\nfacet normal 1 0 0\nouter loop\nvertex 9 0 0\nvertex 9 1 0\nvertex 9 0 1\n"
             "endloop\nendfacet\nendsolid c\n";
  }
  const std::string aFile = aPath.string();

  const occ::handle<Poly_Triangulation> aMesh = RWStl::ReadFile(aFile.c_str());
  ASSERT_FALSE(aMesh.IsNull());
  EXPECT_EQ(aMesh->NbTriangles(), 3);

  NCollection_Sequence<occ::handle<Poly_Triangulation>> aSolids;
  RWStl::ReadFile(aFile.c_str(), M_PI / 2.0, aSolids);
  EXPECT_EQ(aSolids.Length(), 3);

  {
    std::ifstream                         anIn(aPath, std::ios::binary);
    const occ::handle<Poly_Triangulation> aStreamMesh = RWStl::ReadStream(anIn);
    ASSERT_FALSE(aStreamMesh.IsNull());
    EXPECT_EQ(aStreamMesh->NbTriangles(), 3);
  }

  std::filesystem::remove(aPath);
}
