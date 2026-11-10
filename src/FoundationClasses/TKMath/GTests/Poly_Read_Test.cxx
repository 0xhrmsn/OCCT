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

#include <Poly.hxx>
#include <Poly_Polygon2D.hxx>
#include <Poly_Polygon3D.hxx>
#include <Poly_Triangulation.hxx>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

// The keyword of a triangulation or a polygon was read into a buffer of 100 bytes without a
// limit: a longer word in a damaged or crafted stream was written past the end of the buffer.
TEST(PolyReadTest, LongKeyword)
{
  const std::string aWord = std::string(300, 'P') + " 1 1\n";
  {
    std::istringstream anIn(aWord);
    EXPECT_TRUE(Poly::ReadTriangulation(anIn).IsNull());
  }
  {
    std::istringstream anIn(aWord);
    EXPECT_TRUE(Poly::ReadPolygon3D(anIn).IsNull());
  }
  {
    std::istringstream anIn(aWord);
    EXPECT_TRUE(Poly::ReadPolygon2D(anIn).IsNull());
  }
}

// Guard: a triangulation written by Poly::Write is read back.
TEST(PolyReadTest, WriteRead_Triangulation)
{
  occ::handle<Poly_Triangulation> aTri = new Poly_Triangulation(3, 1, false);
  aTri->SetNode(1, gp_Pnt(0.0, 0.0, 0.0));
  aTri->SetNode(2, gp_Pnt(1.0, 0.0, 0.0));
  aTri->SetNode(3, gp_Pnt(0.0, 1.0, 0.0));
  aTri->SetTriangle(1, Poly_Triangle(1, 2, 3));

  std::stringstream aStream;
  Poly::Write(aTri, aStream, true);
  const occ::handle<Poly_Triangulation> aRead = Poly::ReadTriangulation(aStream);
  ASSERT_FALSE(aRead.IsNull());
  EXPECT_EQ(aRead->NbNodes(), 3);
  EXPECT_EQ(aRead->NbTriangles(), 1);
}
