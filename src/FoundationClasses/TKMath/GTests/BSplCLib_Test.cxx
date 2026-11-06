// Copyright (c) 2025 OPEN CASCADE SAS
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

#include <gtest/gtest.h>

#include <BSplCLib.hxx>
#include <BSplCLib_EvaluatorFunction.hxx>
#include <NCollection_Array1.hxx>
#include <gp_Pnt2d.hxx>

TEST(BSplCLibTest, UnitWeights_SmallSize_ReturnsNonOwning)
{
  const int                        aNbElems = 10;
  const NCollection_Array1<double> aWeights = BSplCLib::UnitWeights(aNbElems);

  EXPECT_EQ(aWeights.Lower(), 1);
  EXPECT_EQ(aWeights.Upper(), aNbElems);
  EXPECT_EQ(aWeights.Length(), aNbElems);
  EXPECT_FALSE(aWeights.IsDeletable());

  for (int i = 1; i <= aNbElems; ++i)
  {
    EXPECT_DOUBLE_EQ(aWeights(i), 1.0);
  }
}

TEST(BSplCLibTest, UnitWeights_MaxSize_ReturnsNonOwning)
{
  const int                        aNbElems = BSplCLib::MaxUnitWeightsSize();
  const NCollection_Array1<double> aWeights = BSplCLib::UnitWeights(aNbElems);

  EXPECT_EQ(aWeights.Length(), aNbElems);
  EXPECT_FALSE(aWeights.IsDeletable());

  EXPECT_DOUBLE_EQ(aWeights(1), 1.0);
  EXPECT_DOUBLE_EQ(aWeights(aNbElems), 1.0);
}

TEST(BSplCLibTest, UnitWeights_OverMaxSize_ReturnsOwning)
{
  const int                        aNbElems = BSplCLib::MaxUnitWeightsSize() + 1;
  const NCollection_Array1<double> aWeights = BSplCLib::UnitWeights(aNbElems);

  EXPECT_EQ(aWeights.Lower(), 1);
  EXPECT_EQ(aWeights.Upper(), aNbElems);
  EXPECT_EQ(aWeights.Length(), aNbElems);
  EXPECT_TRUE(aWeights.IsDeletable());

  for (int i = 1; i <= aNbElems; ++i)
  {
    EXPECT_DOUBLE_EQ(aWeights(i), 1.0);
  }
}

TEST(BSplCLibTest, UnitWeights_SingleElement)
{
  const NCollection_Array1<double> aWeights = BSplCLib::UnitWeights(1);

  EXPECT_EQ(aWeights.Length(), 1);
  EXPECT_FALSE(aWeights.IsDeletable());
  EXPECT_DOUBLE_EQ(aWeights(1), 1.0);
}

TEST(BSplCLibTest, MaxUnitWeightsSize_IsPositive)
{
  EXPECT_GT(BSplCLib::MaxUnitWeightsSize(), 0);
  EXPECT_EQ(BSplCLib::MaxUnitWeightsSize(), 2049);
}

namespace
{
//! Evaluator returning theScale * t (the reparameterisation) or theScale (the multiplier).
class BSplCLib_ScaleFunction : public BSplCLib_EvaluatorFunction
{
public:
  BSplCLib_ScaleFunction(const double theScale, const bool theIsLinear)
      : myScale(theScale),
        myIsLinear(theIsLinear)
  {
  }

  void Evaluate(const int,
                const double*,
                const double theParameter,
                double&      theResult,
                int&         theErrorCode) const override
  {
    theResult    = myIsLinear ? myScale * theParameter : myScale;
    theErrorCode = 0;
  }

private:
  double myScale;
  bool   myIsLinear;
};

//! Point of the cubic Bezier curve thePoles at theU (also outside [0, 1]).
gp_Pnt2d cubicBezierPoint(const NCollection_Array1<gp_Pnt2d>& thePoles, const double theU)
{
  const double aV    = 1.0 - theU;
  const double aB[4] = {aV * aV * aV,
                        3.0 * theU * aV * aV,
                        3.0 * theU * theU * aV,
                        theU * theU * theU};
  gp_XY        aXY(0.0, 0.0);
  for (int i = 0; i < 4; ++i)
  {
    aXY += aB[i] * thePoles(thePoles.Lower() + i).XY();
  }
  return gp_Pnt2d(aXY);
}

NCollection_Array1<double> bezierFlatKnots(const double theLast)
{
  NCollection_Array1<double> aKnots(1, 8);
  for (int i = 1; i <= 4; ++i)
  {
    aKnots(i)     = 0.0;
    aKnots(i + 4) = theLast;
  }
  return aKnots;
}

NCollection_Array1<gp_Pnt2d> cubicPoles()
{
  NCollection_Array1<gp_Pnt2d> aPoles(1, 4);
  aPoles(1) = gp_Pnt2d(0.0, 0.0);
  aPoles(2) = gp_Pnt2d(1.0, 2.0);
  aPoles(3) = gp_Pnt2d(3.0, -1.0);
  aPoles(4) = gp_Pnt2d(4.0, 1.0);
  return aPoles;
}
} // namespace

// FunctionReparameterise() and FunctionMultiply() passed one int as the two-slot extrapolation mode
// of Eval(), which read the second slot out of bounds for parameters past the last knot (ASan
// stack-buffer-overflow in Draw bugs/modalg_1/bug10435_1: fillet -> SameParameter -> ConcatC1).
// Past the last knot the curve must continue with its full degree.
TEST(BSplCLibTest, FunctionReparameterise_PastLastKnot_KeepsFullDegree)
{
  const NCollection_Array1<gp_Pnt2d> aPoles = cubicPoles();
  const NCollection_Array1<double>   aKnots = bezierFlatKnots(1.0);
  const BSplCLib_ScaleFunction       aReparam(1.25, true);
  NCollection_Array1<gp_Pnt2d>       aNewPoles(1, 4);
  int                                aStatus = -1;
  BSplCLib::FunctionReparameterise(aReparam, 3, aKnots, aPoles, aKnots, 3, aNewPoles, aStatus);
  ASSERT_EQ(aStatus, 0);
  for (int i = 0; i <= 8; ++i)
  {
    const double aT = i / 8.0;
    EXPECT_LT(cubicBezierPoint(aNewPoles, aT).Distance(cubicBezierPoint(aPoles, 1.25 * aT)),
              1.0e-12)
      << "t = " << aT;
  }
}

TEST(BSplCLibTest, FunctionMultiply_PastLastKnot_KeepsFullDegree)
{
  const NCollection_Array1<gp_Pnt2d> aPoles = cubicPoles();
  const BSplCLib_ScaleFunction       aFactor(2.0, false);
  NCollection_Array1<gp_Pnt2d>       aNewPoles(1, 4);
  int                                aStatus = -1;
  BSplCLib::FunctionMultiply(aFactor,
                             3,
                             bezierFlatKnots(1.0),
                             aPoles,
                             bezierFlatKnots(1.25),
                             3,
                             aNewPoles,
                             aStatus);
  ASSERT_EQ(aStatus, 0);
  for (int i = 0; i <= 8; ++i)
  {
    const double   aT = i / 8.0;
    const gp_Pnt2d anExpected(2.0 * cubicBezierPoint(aPoles, 1.25 * aT).XY());
    EXPECT_LT(cubicBezierPoint(aNewPoles, aT).Distance(anExpected), 1.0e-12) << "t = " << aT;
  }
}
