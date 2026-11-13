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

#include <Geom2d_Curve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <GeomProjLib.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Ellipse.hxx>
#include <gp_Ax3.hxx>
#include <gp_Elips.hxx>
#include <NCollection_Array1.hxx>

#include <gtest/gtest.h>

#include <cmath>

namespace
{
//! Largest distance between the 3D curve and its curve on the surface at the same parameter.
double deviation(const occ::handle<Geom_Curve>&   theCurve,
                 const occ::handle<Geom2d_Curve>& thePCurve,
                 const occ::handle<Geom_Surface>& theSurface,
                 const double                     theFirst,
                 const double                     theLast)
{
  double aMax = 0.0;
  for (int i = 0; i <= 400; ++i)
  {
    const double   aPar = theFirst + (theLast - theFirst) * i / 400.0;
    const gp_Pnt2d aUV  = thePCurve->Value(aPar);
    aMax = std::max(aMax, theSurface->Value(aUV.X(), aUV.Y()).Distance(theCurve->Value(aPar)));
  }
  return aMax;
}
} // namespace

// The tolerance of an approximated curve on a surface was the error at the fitting points,
// converted from 2D to 3D with an assumed ratio: for a helix across the seam of a cylinder,
// 1e-7 was reported and the curve on the surface was farther from the 3D curve.
TEST(ProjLib_ComputeApproxTest, Curve2d_ToleranceCoversDeviation)
{
  const occ::handle<Geom_CylindricalSurface> aCylinder =
    new Geom_CylindricalSurface(gp_Ax3(), 10.0);

  // helix across the seam, approximated by a B-spline then projected on the cylinder
  NCollection_Array1<gp_Pnt> aPoints(1, 40);
  for (int i = 1; i <= 40; ++i)
  {
    const double aT = -M_PI + 2.0 * M_PI * (i - 1) / 39.0;
    aPoints(i)      = gp_Pnt(10.0 * std::cos(aT), 10.0 * std::sin(aT), 2.0 * aT);
  }
  const occ::handle<Geom_Curve> aHelix =
    GeomProjLib::Project(GeomAPI_PointsToBSpline(aPoints, 3, 8, GeomAbs_C2, 1.0e-9).Curve(),
                         aCylinder);
  ASSERT_FALSE(aHelix.IsNull());

  // ellipse cut by the plane z = 0.5 x: on the cylinder, across the seam
  const gp_Ax2 anEllipseAxes(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(-0.5, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.5));
  const occ::handle<Geom_Curve> anEllipse =
    new Geom_Ellipse(gp_Elips(anEllipseAxes, 10.0 * std::sqrt(1.25), 10.0));

  for (const occ::handle<Geom_Curve>& aCurve : {aHelix, anEllipse})
  {
    const double aFirst = aCurve->FirstParameter();
    const double aLast  = aCurve == anEllipse ? 2.0 * M_PI : aCurve->LastParameter();
    double       aTol   = 1.0e-7;
    const occ::handle<Geom2d_Curve> aPCurve =
      GeomProjLib::Curve2d(aCurve, aFirst, aLast, aCylinder, aTol);
    ASSERT_FALSE(aPCurve.IsNull());
    EXPECT_LE(deviation(aCurve, aPCurve, aCylinder, aFirst, aLast), aTol * (1.0 + 1.0e-9))
      << (aCurve == anEllipse ? "ellipse" : "helix") << " tolerance " << aTol;
  }
}
