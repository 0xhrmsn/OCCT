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

#include <TDataStd_Integer.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <XmlLDrivers.hxx>

#include <sstream>
#include <string>

namespace
{

constexpr int THE_TAG   = 7;
constexpr int THE_VALUE = 42;

occ::handle<TDocStd_Application> xmlApplication()
{
  occ::handle<TDocStd_Application> anApp = new TDocStd_Application();
  XmlLDrivers::DefineFormat(anApp);
  return anApp;
}

//! XmlLOcaf document with three nested labels of tag THE_TAG; the deepest holds an integer.
std::string chainOfThree()
{
  occ::handle<TDocStd_Application> anApp = xmlApplication();
  occ::handle<TDocStd_Document>    aDoc;
  anApp->NewDocument("XmlLOcaf", aDoc);
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

//! The document of chainOfThree() with theNbAdded labels inserted above the chain.
std::string deepChain(const int theNbAdded)
{
  const std::string aData  = chainOfThree();
  const std::string anOpen = "<label tag=\"7\"";
  const size_t      aFirst = aData.find(anOpen);
  const size_t      aNext  = aFirst == std::string::npos ? aFirst : aData.find(anOpen, aFirst + 1);
  const size_t      anEnd  = aNext == std::string::npos ? aNext : aData.find("</label>", aNext);
  if (anEnd == std::string::npos)
  {
    ADD_FAILURE() << "layout of the document not recognized";
    return std::string();
  }
  const std::string aHeader = aData.substr(aFirst, aNext - aFirst);
  std::string       aHeaders;
  std::string       anEnds;
  for (int anIndex = 0; anIndex < theNbAdded; ++anIndex)
  {
    aHeaders += aHeader;
    anEnds += "</label>";
  }
  return aData.substr(0, aFirst) + aHeaders + aData.substr(aFirst, anEnd - aFirst) + anEnds
         + aData.substr(anEnd);
}

//! Opens theData; returns the depth of the integer in the label chain, -1 when the document is
//! not read and 0 when the integer is not found.
int openChain(const std::string& theData)
{
  occ::handle<TDocStd_Application> anApp = xmlApplication();
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

// Labels were read recursively, limited only by the depth of XML elements accepted by LDOMParser
// (1000): a document near that depth used most of a 1 MiB stack, and overflowed it in builds with
// larger frames (ASan), before the attributes of the deepest label were read.
TEST(XmlLDriversTest, Read_DeepLabels)
{
  EXPECT_EQ(openChain(deepChain(0)), 3);
  EXPECT_EQ(openChain(deepChain(296)), 299);
  EXPECT_EQ(openChain(deepChain(297)), -1);
  EXPECT_EQ(openChain(deepChain(900)), -1);
  EXPECT_EQ(openChain(deepChain(2000)), -1);
}
