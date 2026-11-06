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

#include <Geom2d_BSplineCurve.hxx>
#include <Geom2dAPI_Interpolate.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <Geom2dConvert_ApproxArcsSegments.hxx>
#include <gp_Trsf2d.hxx>

#include <gtest/gtest.h>

// A very short free-form curve (length about 1e-4) gave a result with a null element, which a
// caller using the result dereferences.
TEST(Geom2dConvert_ApproxArcsSegmentsTest, VeryShortCurve_NoNullElement)
{
  const double                               aY[8]   = {11.948190990956064,
                                                        5.0672367371605347,
                                                        10.362668392585572,
                                                        -1.8398861140650649,
                                                        -9.7117371500106344,
                                                        6.946639866651303,
                                                        -0.66062528641755769,
                                                        -4.3908095148768744};
  occ::handle<NCollection_HArray1<gp_Pnt2d>> aPoints = new NCollection_HArray1<gp_Pnt2d>(1, 8);
  for (int i = 0; i < 8; ++i)
  {
    aPoints->SetValue(i + 1, gp_Pnt2d(i * 12.0, aY[i]));
  }
  Geom2dAPI_Interpolate anInterp(aPoints, false, 1.0e-9);
  anInterp.Perform();
  ASSERT_TRUE(anInterp.IsDone());
  occ::handle<Geom2d_BSplineCurve> aCurve = anInterp.Curve();
  gp_Trsf2d                        aScale;
  aScale.SetScale(gp_Pnt2d(0.0, 0.0), 1.0e-6);
  aCurve->Transform(aScale);

  for (const double aTol : {1.0e-1, 1.0e-3, 1.0e-5})
  {
    const Geom2dAdaptor_Curve aAdaptor(aCurve, aCurve->FirstParameter(), aCurve->LastParameter());
    const NCollection_Sequence<occ::handle<Geom2d_Curve>> aResult =
      Geom2dConvert_ApproxArcsSegments(aAdaptor, aTol, 1.0e-2).GetResult();
    EXPECT_FALSE(aResult.IsEmpty()) << "tolerance " << aTol;
    for (int i = 1; i <= aResult.Length(); ++i)
    {
      EXPECT_FALSE(aResult(i).IsNull())
        << "tolerance " << aTol << ", element " << i << " of " << aResult.Length();
    }
  }
}
