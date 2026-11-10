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

#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Trsf.hxx>
#include <gp_Trsf2d.hxx>
#include <Standard_ConstructionError.hxx>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

// A direction of NaN or infinite coordinates, like a null one, cannot be normalized; it was
// accepted and gave NaN coordinates. A rotation by a NaN angle gave a matrix of NaN.
TEST(gp_NonFiniteTest, DirectionAndRotation)
{
  const double aNaN  = std::numeric_limits<double>::quiet_NaN();
  const double anInf = std::numeric_limits<double>::infinity();

  EXPECT_THROW(gp_Dir(aNaN, 0.0, 0.0), Standard_ConstructionError);
  EXPECT_THROW(gp_Dir(anInf, 0.0, 0.0), Standard_ConstructionError);
  EXPECT_THROW(gp_Dir(0.0, 0.0, 0.0), Standard_ConstructionError);
  EXPECT_THROW(gp_Dir2d(aNaN, 1.0), Standard_ConstructionError);
  gp_Dir aDir;
  EXPECT_THROW(aDir.SetCoord(1.0, aNaN, 0.0), Standard_ConstructionError);

  gp_Trsf aTrsf;
  EXPECT_THROW(aTrsf.SetRotation(gp_Ax1(), aNaN), Standard_ConstructionError);
  EXPECT_THROW(aTrsf.SetRotation(gp_Ax1(), anInf), Standard_ConstructionError);
  gp_Trsf2d aTrsf2d;
  EXPECT_THROW(aTrsf2d.SetRotation(gp_Pnt2d(), aNaN), Standard_ConstructionError);

  // guard: finite values
  EXPECT_NEAR(gp_Dir(3.0, 0.0, 4.0).Z(), 0.8, 1e-15);
  EXPECT_NEAR(gp_Dir(1e150, 1e150, 0.0).X(), std::sqrt(0.5), 1e-15);
  aTrsf.SetRotation(gp_Ax1(), 0.5);
  EXPECT_NEAR(aTrsf.Value(1, 1), std::cos(0.5), 1e-15);
}
