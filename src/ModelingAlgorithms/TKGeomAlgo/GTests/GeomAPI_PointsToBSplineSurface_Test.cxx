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

#include <GeomAPI_PointsToBSplineSurface.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>

#include <Geom_BSplineSurface.hxx>
#include <GeomAbs_Shape.hxx>
#include <NCollection_Array2.hxx>
#include <Precision.hxx>
#include <StdFail_NotDone.hxx>

#include <gtest/gtest.h>

#include <cmath>

TEST(GeomAPI_PointsToBSplineSurface_Test, FailedDegenerateRebuildResetsDoneState)
{
  NCollection_Array2<double> aValidZPoints(1, 2, 1, 2);
  aValidZPoints(1, 1) = 0.0;
  aValidZPoints(1, 2) = 1.0;
  aValidZPoints(2, 1) = 1.0;
  aValidZPoints(2, 2) = 0.0;

  GeomAPI_PointsToBSplineSurface anApprox;
  EXPECT_NO_THROW(
    anApprox.Init(aValidZPoints, 0.0, 1.0, 0.0, 1.0, 3, 5, GeomAbs_C2, Precision::Confusion()));
  ASSERT_TRUE(anApprox.IsDone());

  NCollection_Array2<double> aZPoints(1, 3, 1, 1);
  aZPoints(1, 1) = 0.0;
  aZPoints(2, 1) = 1.0;
  aZPoints(3, 1) = 0.0;

  EXPECT_THROW(
    anApprox.Init(aZPoints, 0.0, 1.0, 0.0, 0.0, 3, 5, GeomAbs_C2, Precision::Confusion()),
    StdFail_NotDone);
  EXPECT_FALSE(anApprox.IsDone());
}

// The surface is approximated in two passes (the rows of points, then the poles of the rows) and
// their errors add up: with each pass taking Tol3D, the surface was up to 1.8 * Tol3D away from
// the points while the approximation was done.
TEST(GeomAPI_PointsToBSplineSurface_Test, TwoPasses_SurfaceWithinTol3D)
{
  const int                  aNb = 50;
  const double               aDx = M_PI / (aNb - 1) * 10.0;
  NCollection_Array2<gp_Pnt> aPoints(1, aNb, 1, aNb);
  for (int i = 1; i <= aNb; ++i)
  {
    for (int j = 1; j <= aNb; ++j)
    {
      const double aZ =
        std::sin((i - 1) * M_PI / (aNb - 1)) * std::cos((j - 1) * M_PI / (aNb - 1)) * 10.0;
      aPoints(i, j) = gp_Pnt((i - 1) * aDx, (j - 1) * aDx, aZ);
    }
  }
  const double aTol3D = 1.0e-4;
  for (const int aDegMax : {3, 5})
  {
    GeomAPI_PointsToBSplineSurface anApprox;
    anApprox.Init(aPoints, 3, aDegMax, GeomAbs_C2, aTol3D);
    ASSERT_TRUE(anApprox.IsDone());
    double aMaxDist = 0.0;
    for (int i = 1; i <= aNb; ++i)
    {
      for (int j = 1; j <= aNb; ++j)
      {
        GeomAPI_ProjectPointOnSurf aProj(aPoints(i, j), anApprox.Surface());
        ASSERT_GT(aProj.NbPoints(), 0);
        aMaxDist = std::max(aMaxDist, aProj.LowerDistance());
      }
    }
    EXPECT_LE(aMaxDist, aTol3D * (1.0 + 1.0e-7)) << "degree max " << aDegMax;
  }
}
