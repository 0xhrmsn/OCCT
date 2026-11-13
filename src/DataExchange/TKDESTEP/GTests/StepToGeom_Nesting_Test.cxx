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

#include <BRepPrimAPI_MakeBox.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>

#include <gtest/gtest.h>

#include <regex>
#include <sstream>
#include <string>

namespace
{
//! STEP file of a box in which the curve of one edge is replaced by a chain of theDepth trimmed
//! curves; with theIsCycle the last one is its own basis curve.
std::string boxWithNestedCurve(const int theDepth, const bool theIsCycle)
{
  STEPControl_Writer aWriter;
  aWriter.Transfer(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape(), STEPControl_AsIs);
  std::ostringstream anOut;
  aWriter.WriteStream(anOut);
  std::string aText = anOut.str();

  const std::regex anEdgeRe("EDGE_CURVE\\('[^']*',\\s*#(\\d+),\\s*#(\\d+),\\s*#(\\d+)");
  std::smatch      aMatch;
  if (!std::regex_search(aText, aMatch, anEdgeRe))
  {
    return std::string();
  }
  const std::string aCurveRef = aMatch[3].str();
  const size_t      aCurvePos = aMatch.position(3);
  int               aMaxId    = 0;
  const std::regex  anIdRe("#(\\d+)\\s*=");
  for (std::sregex_iterator anIt(aText.begin(), aText.end(), anIdRe), anEnd; anIt != anEnd;
       ++anIt)
  {
    aMaxId = std::max(aMaxId, std::stoi((*anIt)[1].str()));
  }
  std::string aChain;
  int         aPrev = std::stoi(aCurveRef);
  int         anId  = aMaxId;
  for (int i = 0; i < theDepth; ++i)
  {
    ++anId;
    const int aBasis = (theIsCycle && i == theDepth - 1) ? anId : aPrev;
    aChain += "#" + std::to_string(anId) + " = TRIMMED_CURVE('',#" + std::to_string(aBasis)
              + ",(PARAMETER_VALUE(0.)),(PARAMETER_VALUE(10.)),.T.,.PARAMETER.);\n";
    aPrev = anId;
  }
  aText.replace(aCurvePos, aCurveRef.size(), std::to_string(anId));
  aText.insert(aText.find("ENDSEC;", aText.find("DATA;")), aChain);
  return aText;
}
} // namespace

// The conversion of nested curves (MakeCurve, MakeCurve2d, MakeSurface) recursed without limit:
// 10000 nested trimmed curves in a 1 MB file, or one trimmed curve being its own basis, exhausted
// the stack and the process crashed. Too deep a nesting now gives no curve for that edge.
TEST(StepToGeom_NestingTest, NestedOrCyclicTrimmedCurve_NoStackOverflow)
{
  for (const auto& [aDepth, isCycle] : {std::pair<int, bool>{5, false},
                                        std::pair<int, bool>{10000, false},
                                        std::pair<int, bool>{1, true}})
  {
    const std::string aText = boxWithNestedCurve(aDepth, isCycle);
    ASSERT_FALSE(aText.empty());
    STEPControl_Reader aReader;
    std::istringstream anIn(aText);
    ASSERT_EQ(aReader.ReadStream("nested.stp", anIn), IFSelect_RetDone);
    EXPECT_NO_THROW(aReader.TransferRoots()) << "depth " << aDepth << " cycle " << isCycle;
    if (aDepth == 5)
    {
      // a short legal nesting is still converted: the box is complete
      EXPECT_EQ(aReader.NbShapes(), 1);
    }
  }
}
