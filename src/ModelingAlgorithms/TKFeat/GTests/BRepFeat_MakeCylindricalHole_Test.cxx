// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <BRepFeat_MakeCylindricalHole.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS_Shape.hxx>

#include <gtest/gtest.h>

#include <cmath>

namespace
{
// Plate 100 x 100 x 10 centred on the Z axis.
TopoDS_Shape plate()
{
  return BRepPrimAPI_MakeBox(gp_Pnt(-50.0, -50.0, 0.0), 100.0, 100.0, 10.0).Shape();
}
} // namespace

TEST(BRepFeat_MakeCylindricalHoleTest, Perform_HoleInPlate_RemovesCylinder)
{
  BRepFeat_MakeCylindricalHole aHole;
  aHole.Init(plate(), gp_Ax1(gp_Pnt(0.0, 0.0, 20.0), gp_Dir(0.0, 0.0, -1.0)));
  aHole.Perform(3.0);
  aHole.Build();
  ASSERT_EQ(aHole.Status(), BRepFeat_NoError);
  GProp_GProps aProps;
  BRepGProp::VolumeProperties(aHole.Shape(), aProps);
  EXPECT_NEAR(aProps.Mass(), 100000.0 - M_PI * 9.0 * 10.0, 1.0e-6);
}

// A hole wider than the plate removed the whole plate and was reported without error.
TEST(BRepFeat_MakeCylindricalHoleTest, Perform_HoleWiderThanShape_ReportsEmptyResult)
{
  BRepFeat_MakeCylindricalHole aHole;
  aHole.Init(plate(), gp_Ax1(gp_Pnt(0.0, 0.0, 20.0), gp_Dir(0.0, 0.0, -1.0)));
  aHole.Perform(80.0);
  aHole.Build();
  EXPECT_EQ(aHole.Status(), BRepFeat_EmptyResult);
}
