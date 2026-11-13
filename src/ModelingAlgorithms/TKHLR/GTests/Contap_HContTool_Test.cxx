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

#include <Contap_HContTool.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Geom_Plane.hxx>
#include <gp_Pln.hxx>

#include <gtest/gtest.h>

// The bounds of the sampled surface were kept in static variables set by NbSamplePoints and read
// by SamplePoint: HLR running in several threads, or two surfaces sampled alternately, took the
// sample points of one surface inside the bounds of the other.
TEST(Contap_HContToolTest, SamplePoint_UsesBoundsOfItsSurface)
{
  const occ::handle<Geom_Plane>          aPlane = new Geom_Plane(gp_Pln());
  const occ::handle<GeomAdaptor_Surface> aNear =
    new GeomAdaptor_Surface(aPlane, 0.0, 10.0, 0.0, 10.0);
  const occ::handle<GeomAdaptor_Surface> aFar =
    new GeomAdaptor_Surface(aPlane, 100.0, 200.0, 100.0, 200.0);

  const int aNbNear = Contap_HContTool::NbSamplePoints(aNear);
  Contap_HContTool::NbSamplePoints(aFar); // another surface sampled in between
  for (int i = 1; i <= aNbNear; ++i)
  {
    double aU = 0.0, aV = 0.0;
    Contap_HContTool::SamplePoint(aNear, i, aU, aV);
    EXPECT_GE(aU, 0.0) << "sample " << i;
    EXPECT_LE(aU, 10.0) << "sample " << i;
    EXPECT_GE(aV, 0.0) << "sample " << i;
    EXPECT_LE(aV, 10.0) << "sample " << i;
  }
  double aU = 0.0, aV = 0.0;
  Contap_HContTool::SamplePoint(aNear, 5, aU, aV);
  EXPECT_DOUBLE_EQ(aU, 5.0);
  EXPECT_DOUBLE_EQ(aV, 5.0);
}
