// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <Poly_CoherentTriangulation.hxx>
#include <Poly_Connect.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_OutOfRange.hxx>
#include <gp_Pnt.hxx>

#include <gtest/gtest.h>

namespace
{
// Four nodes, two triangles sharing the edge 1-2; theBad replaces one node of the second triangle.
occ::handle<Poly_Triangulation> twoTriangles(const int theBad)
{
  occ::handle<Poly_Triangulation> aTri = new Poly_Triangulation(4, 2, false);
  aTri->SetNode(1, gp_Pnt(0.0, 0.0, 0.0));
  aTri->SetNode(2, gp_Pnt(1.0, 0.0, 0.0));
  aTri->SetNode(3, gp_Pnt(0.0, 1.0, 0.0));
  aTri->SetNode(4, gp_Pnt(0.0, -1.0, 0.0));
  aTri->SetTriangle(1, Poly_Triangle(1, 2, 3));
  aTri->SetTriangle(2, Poly_Triangle(2, 1, theBad));
  return aTri;
}
} // namespace

TEST(Poly_ConnectTest, Load_ValidTriangulation_FindsNeighbour)
{
  Poly_Connect aConnect(twoTriangles(4));
  int          aT1 = 0, aT2 = 0, aT3 = 0;
  aConnect.Triangles(1, aT1, aT2, aT3);
  EXPECT_EQ(aT1 + aT2 + aT3, 2);
}

TEST(Poly_ConnectTest, Load_NodeIndexAboveRange_Throws)
{
  EXPECT_THROW(Poly_Connect aConnect(twoTriangles(5)), Standard_OutOfRange);
}

TEST(Poly_ConnectTest, Load_NodeIndexZeroOrNegative_Throws)
{
  EXPECT_THROW(Poly_Connect aConnect(twoTriangles(0)), Standard_OutOfRange);
  EXPECT_THROW(Poly_Connect aConnect(twoTriangles(-3)), Standard_OutOfRange);
}

TEST(Poly_CoherentTriangulationTest, AddTriangle_NodeIndexOutOfRange_Rejected)
{
  occ::handle<Poly_CoherentTriangulation> aCoh = new Poly_CoherentTriangulation(twoTriangles(4));
  ASSERT_EQ(aCoh->NNodes(), 4);
  EXPECT_EQ(aCoh->NTriangles(), 2);
  EXPECT_EQ(aCoh->AddTriangle(0, 1, 4), nullptr);
  EXPECT_EQ(aCoh->AddTriangle(0, 1, -1), nullptr);
  EXPECT_NE(aCoh->AddTriangle(0, 2, 3), nullptr);
  EXPECT_EQ(aCoh->NTriangles(), 3);
}

TEST(Poly_CoherentTriangulationTest, Construct_NodeIndexAboveRange_TriangleSkipped)
{
  occ::handle<Poly_CoherentTriangulation> aCoh = new Poly_CoherentTriangulation(twoTriangles(9));
  EXPECT_EQ(aCoh->NTriangles(), 1);
  aCoh->ComputeLinks();
  EXPECT_EQ(aCoh->NLinks(), 3);
}