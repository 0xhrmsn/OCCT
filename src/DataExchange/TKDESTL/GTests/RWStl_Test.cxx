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
#include <gp_Pnt.hxx>

#include <gtest/gtest.h>

#include <filesystem>
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
