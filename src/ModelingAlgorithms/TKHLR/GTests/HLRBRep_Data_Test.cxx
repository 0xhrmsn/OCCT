// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <BRep_Builder.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <gtest/gtest.h>

namespace
{
int nbEdges(const TopoDS_Shape& theShape)
{
  int aNb = 0;
  for (TopExp_Explorer anExp(theShape, TopAbs_EDGE); anExp.More(); anExp.Next())
  {
    ++aNb;
  }
  return aNb;
}
} // namespace

// The rejection table keeps the 2D intersections of each edge in rows of 8 entries. When a row was
// exactly full, the sort loop read one entry past its end before checking the row size (visible
// under AddressSanitizer). A long bar in front of 12 cylinders gives each horizontal bar edge 12
// crossings with the cylinder seams in the projection.
TEST(HLRBRep_DataTest, FullRejectionRow_NoOverread)
{
  BRep_Builder    aBuilder;
  TopoDS_Compound aCompound;
  aBuilder.MakeCompound(aCompound);
  aBuilder.Add(aCompound, BRepPrimAPI_MakeBox(gp_Pnt(-5.0, 0.0, 4.0), 130.0, 2.0, 2.0).Shape());
  for (int i = 0; i < 12; ++i)
  {
    aBuilder.Add(aCompound,
                 BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(i * 10.0, 10.0, 0.0), gp::DZ()), 3.0, 10.0)
                   .Shape());
  }

  occ::handle<HLRBRep_Algo> anAlgo = new HLRBRep_Algo();
  anAlgo->Add(aCompound);
  anAlgo->Projector(HLRAlgo_Projector(gp_Ax2(gp::Origin(), gp_Dir(0.0, -1.0, 0.0))));
  anAlgo->Update();
  anAlgo->Hide();

  HLRBRep_HLRToShape aToShape(anAlgo);
  const TopoDS_Shape aVisible = aToShape.VCompound();
  const TopoDS_Shape aHidden  = aToShape.HCompound();
  ASSERT_FALSE(aVisible.IsNull());
  ASSERT_FALSE(aHidden.IsNull());
  // The bar hides part of every cylinder seam: both visible and hidden sharp edges exist.
  EXPECT_GT(nbEdges(aVisible), 0);
  EXPECT_GT(nbEdges(aHidden), 0);
}
