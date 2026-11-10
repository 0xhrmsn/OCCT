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

#include <gtest/gtest.h>

#include <BinLDrivers.hxx>
#include <TDataStd_Integer.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>

#include <cstring>
#include <sstream>
#include <string>

namespace
{

constexpr int THE_TAG   = 7;
constexpr int THE_VALUE = 42;

//! Application able to read and write BinLOcaf documents.
occ::handle<TDocStd_Application> binApplication()
{
  occ::handle<TDocStd_Application> anApp = new TDocStd_Application();
  BinLDrivers::DefineFormat(anApp);
  return anApp;
}

//! Reads the integer at the byte theOffset of theData.
int intAt(const std::string& theData, const size_t theOffset)
{
  int aValue = 0;
  std::memcpy(&aValue, theData.data() + theOffset, sizeof(int));
  return aValue;
}

//! BinLOcaf document with three nested labels of tag THE_TAG under the root; the deepest holds an
//! integer attribute.
std::string chainOfThree()
{
  occ::handle<TDocStd_Application> anApp = binApplication();
  occ::handle<TDocStd_Document>    aDoc;
  anApp->NewDocument("BinLOcaf", aDoc);
  TDF_Label aLabel = aDoc->GetData()->Root();
  for (int aLevel = 0; aLevel < 3; ++aLevel)
  {
    aLabel = aLabel.FindChild(THE_TAG, true);
  }
  TDataStd_Integer::Set(aLabel, THE_VALUE);

  std::ostringstream anOut;
  EXPECT_EQ(anApp->SaveAs(aDoc, anOut), PCDM_SS_OK);
  anApp->Close(aDoc);
  return anOut.str();
}

//! The document of chainOfThree() with theNbAdded labels inserted above the chain: the header of
//! the outer label of the chain (its tag, size and end of attributes) and its end marker are
//! repeated. The sizes of the labels are only used by partial reading.
std::string deepChain(const int theNbAdded)
{
  const std::string aData = chainOfThree();

  // header of the two outer labels of the chain, which have no attribute
  size_t aHeaderSize = 0;
  size_t aHeaderPos  = std::string::npos;
  for (size_t aPos = 0; aPos + 32 <= aData.size() && aHeaderPos == std::string::npos; ++aPos)
  {
    for (const size_t aSize : {size_t(8), size_t(16)})
    {
      if (intAt(aData, aPos) == THE_TAG && intAt(aData, aPos + aSize - 4) == -1
          && intAt(aData, aPos + aSize) == THE_TAG && intAt(aData, aPos + 2 * aSize - 4) == -1)
      {
        aHeaderPos  = aPos;
        aHeaderSize = aSize;
        break;
      }
    }
  }
  // end markers of the deepest label, the two outer ones and the root
  const std::string anEnd    = std::string("\xFE\xFF\xFF\xFF", 4);
  const size_t      anEndPos = aData.find(anEnd + anEnd + anEnd + anEnd);
  if (aHeaderPos == std::string::npos || anEndPos == std::string::npos || anEndPos < aHeaderPos)
  {
    ADD_FAILURE() << "layout of the document not recognized";
    return std::string();
  }

  std::string aHeaders;
  std::string anEnds;
  for (int anIndex = 0; anIndex < theNbAdded; ++anIndex)
  {
    aHeaders += aData.substr(aHeaderPos, aHeaderSize);
    anEnds += anEnd;
  }
  return aData.substr(0, aHeaderPos) + aHeaders + aData.substr(aHeaderPos, anEndPos - aHeaderPos)
         + anEnds + aData.substr(anEndPos);
}

//! Opens theData; returns the depth of the integer attribute in the label chain, -1 when the
//! document is not read and 0 when the attribute is not found.
int openChain(const std::string& theData)
{
  occ::handle<TDocStd_Application> anApp = binApplication();
  occ::handle<TDocStd_Document>    aDoc;
  std::istringstream               anIn(theData);
  if (anApp->Open(anIn, aDoc) != PCDM_RS_OK || aDoc.IsNull())
  {
    return -1;
  }
  int       aDepth = 0;
  TDF_Label aLabel = aDoc->GetData()->Root();
  for (;;)
  {
    const TDF_Label aChild = aLabel.FindChild(THE_TAG, false);
    if (aChild.IsNull())
    {
      break;
    }
    aLabel = aChild;
    ++aDepth;
  }
  occ::handle<TDataStd_Integer> anInt;
  const bool                    isFound =
    aLabel.FindAttribute(TDataStd_Integer::GetID(), anInt) && anInt->Get() == THE_VALUE;
  anApp->Close(aDoc);
  return isFound ? aDepth : 0;
}

} // namespace

// Labels were read recursively, one level per nested label of a few bytes: a damaged or crafted
// document overflowed the stack.
TEST(BinLDrivers_DocumentRetrievalDriverTest, Read_DeepLabels)
{
  EXPECT_EQ(openChain(deepChain(0)), 3);
  EXPECT_EQ(openChain(deepChain(10)), 13);
  EXPECT_EQ(openChain(deepChain(296)), 299);
  EXPECT_EQ(openChain(deepChain(297)), -1);
  EXPECT_EQ(openChain(deepChain(2000)), -1);
  EXPECT_EQ(openChain(deepChain(200000)), -1);
}
