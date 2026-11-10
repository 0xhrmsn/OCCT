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

#include <BinDrivers.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Message_ProgressIndicator.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <TNaming_Builder.hxx>
#include <TNaming_NamedShape.hxx>

#include <sstream>
#include <string>

namespace
{

//! Progress indicator of a user who cancels after theNbChecks checks.
class LateCancellingIndicator : public Message_ProgressIndicator
{
public:
  explicit LateCancellingIndicator(const int theNbChecks)
      : myNbChecks(theNbChecks)
  {
  }

  bool UserBreak() override { return --myNbChecks < 0; }

  void Show(const Message_ProgressScope&, const bool) override {}

private:
  int myNbChecks;
};

constexpr int THE_NB_LABELS = 200;

//! BinOcaf document of THE_NB_LABELS labels, each with a named shape.
std::string namedShapesDocument(const occ::handle<TDocStd_Application>& theApp)
{
  occ::handle<TDocStd_Document> aDoc;
  theApp->NewDocument("BinOcaf", aDoc);
  for (int anIndex = 1; anIndex <= THE_NB_LABELS; ++anIndex)
  {
    TNaming_Builder aBuilder(aDoc->Main().FindChild(anIndex, true));
    aBuilder.Generated(BRepPrimAPI_MakeBox(anIndex, 2.0, 3.0).Shape());
  }
  std::ostringstream anOut;
  EXPECT_EQ(theApp->SaveAs(aDoc, anOut), PCDM_SS_OK);
  theApp->Close(aDoc);
  return anOut.str();
}

} // namespace

// A reading stopped in the middle of the labels left the attributes already read in the
// relocation table of the driver, while their labels were released with the unread document: the
// next reading with the same driver destroyed them and used the freed labels.
TEST(BinDrivers_DocumentRetrievalDriverTest, Read_AfterCancelledRead)
{
  occ::handle<TDocStd_Application> anApp = new TDocStd_Application();
  BinDrivers::DefineFormat(anApp);
  const std::string aData = namedShapesDocument(anApp);

  for (const int aNbChecks : {50, 150, 300})
  {
    occ::handle<LateCancellingIndicator> aCancel = new LateCancellingIndicator(aNbChecks);
    occ::handle<TDocStd_Document>        aCancelled;
    std::istringstream                   anIn(aData);
    EXPECT_NE(anApp->Open(anIn, aCancelled, aCancel->Start()), PCDM_RS_OK) << aNbChecks;
    if (!aCancelled.IsNull())
    {
      anApp->Close(aCancelled);
    }
  }

  occ::handle<TDocStd_Document> aDoc;
  std::istringstream            anIn(aData);
  ASSERT_EQ(anApp->Open(anIn, aDoc), PCDM_RS_OK);
  int aNbNamed = 0;
  for (TDF_ChildIterator anIter(aDoc->Main()); anIter.More(); anIter.Next())
  {
    occ::handle<TNaming_NamedShape> aNamed;
    aNbNamed += anIter.Value().FindAttribute(TNaming_NamedShape::GetID(), aNamed) ? 1 : 0;
  }
  EXPECT_EQ(aNbNamed, THE_NB_LABELS);
  anApp->Close(aDoc);
}
