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
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Datum3D.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <cstring>

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

namespace
{

constexpr int THE_TAG = 7;

//! Integer at the byte theOffset of theData.
int intAt(const std::string& theData, const size_t theOffset)
{
  int aValue = 0;
  std::memcpy(&aValue, theData.data() + theOffset, sizeof(int));
  return aValue;
}

//! Compounds nested theDepth deep around a vertex placed by a location of theNbItems items.
TopoDS_Shape nestedShape(const int theDepth, const int theNbItems)
{
  TopLoc_Location aLoc;
  for (int anItem = 1; anItem <= theNbItems; ++anItem)
  {
    gp_Trsf aTrsf;
    aTrsf.SetTranslation(gp_Vec(anItem % 2 == 0 ? 1.0 : -1.0, 0.0, 0.001));
    aLoc = aLoc * TopLoc_Location(new TopLoc_Datum3D(aTrsf));
  }
  TopoDS_Shape aShape = BRepBuilderAPI_MakeVertex(gp_Pnt(1.0, 2.0, 3.0)).Shape().Moved(aLoc);
  BRep_Builder aBuilder;
  for (int aLevel = 0; aLevel < theDepth; ++aLevel)
  {
    TopoDS_Compound aCompound;
    aBuilder.MakeCompound(aCompound);
    aBuilder.Add(aCompound, aShape);
    aShape = aCompound;
  }
  return aShape;
}

//! BinOcaf document: three nested labels of tag THE_TAG, the deepest holds theShape; then
//! theNbAdded labels inserted above the chain (header and end marker of its outer label repeated).
std::string deepDocument(const occ::handle<TDocStd_Application>& theApp,
                         const TopoDS_Shape&                     theShape,
                         const int                               theNbAdded)
{
  occ::handle<TDocStd_Document> aDoc;
  theApp->NewDocument("BinOcaf", aDoc);
  TDF_Label aLabel = aDoc->GetData()->Root();
  for (int aLevel = 0; aLevel < 3; ++aLevel)
  {
    aLabel = aLabel.FindChild(THE_TAG, true);
  }
  TNaming_Builder(aLabel).Generated(theShape);
  std::ostringstream anOut;
  EXPECT_EQ(theApp->SaveAs(aDoc, anOut), PCDM_SS_OK);
  theApp->Close(aDoc);
  const std::string aData = anOut.str();

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
  const std::string anEnd    = std::string("\xFE\xFF\xFF\xFF", 4);
  const size_t      anEndPos = aData.find(anEnd + anEnd + anEnd + anEnd);
  if (aHeaderPos == std::string::npos || anEndPos == std::string::npos || anEndPos < aHeaderPos)
  {
    ADD_FAILURE() << "layout of the document not recognized";
    return std::string();
  }
  std::string aHeaders, anEnds;
  for (int anIndex = 0; anIndex < theNbAdded; ++anIndex)
  {
    aHeaders += aData.substr(aHeaderPos, aHeaderSize);
    anEnds += anEnd;
  }
  return aData.substr(0, aHeaderPos) + aHeaders + aData.substr(aHeaderPos, anEndPos - aHeaderPos)
         + anEnds + aData.substr(anEndPos);
}

} // namespace

// Guard of the combined stack budget on a 1 MiB stack: the deepest labels accepted (299) hold, at
// the deepest one, a shape near the nesting limit of the shape reader with a long location.
TEST(BinDrivers_DocumentRetrievalDriverTest, Read_DeepestAcceptedDocument)
{
#if defined(__SANITIZE_ADDRESS__)
  // the limits are set for the frames of a release build: the frames of AddressSanitizer (red
  // zones) are much larger and the deepest accepted document does not fit in 1 MiB
  GTEST_SKIP() << "stack budget of a release build";
#endif
  occ::handle<TDocStd_Application> anApp = new TDocStd_Application();
  BinDrivers::DefineFormat(anApp);
  const std::string aData = deepDocument(anApp, nestedShape(190, 900), 296);

  occ::handle<TDocStd_Document> aDoc;
  std::istringstream            anIn(aData);
  ASSERT_EQ(anApp->Open(anIn, aDoc), PCDM_RS_OK);
  TDF_Label aLabel = aDoc->GetData()->Root();
  int       aDepth = 0;
  for (TDF_Label aChild = aLabel.FindChild(THE_TAG, false); !aChild.IsNull();
       aChild           = aLabel.FindChild(THE_TAG, false))
  {
    aLabel = aChild;
    ++aDepth;
  }
  EXPECT_EQ(aDepth, 299);
  occ::handle<TNaming_NamedShape> aNamed;
  ASSERT_TRUE(aLabel.FindAttribute(TNaming_NamedShape::GetID(), aNamed));
  TopExp_Explorer anExp(aNamed->Get(), TopAbs_VERTEX);
  ASSERT_TRUE(anExp.More());
  EXPECT_NEAR(anExp.Current().Location().Transformation().TranslationPart().Z(), 0.9, 1e-9);
  anApp->Close(aDoc);
}

// The layout of the format version 2 is not written: SaveAs() reported success and wrote a
// document that could not be read (PCDM_RS_FormatFailure).
TEST(BinDrivers_DocumentRetrievalDriverTest, SaveVersion2_Rejected)
{
  occ::handle<TDocStd_Application> anApp = new TDocStd_Application();
  BinDrivers::DefineFormat(anApp);
  occ::handle<TDocStd_Document> aDoc;
  anApp->NewDocument("BinOcaf", aDoc);
  TNaming_Builder aBuilder(aDoc->Main().FindChild(1, true));
  aBuilder.Generated(BRepPrimAPI_MakeBox(1.0, 2.0, 3.0).Shape());
  aDoc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_2);
  std::ostringstream anOut;
  EXPECT_NE(anApp->SaveAs(aDoc, anOut), PCDM_SS_OK);
  anApp->Close(aDoc);
}

// The shape set of the named shape driver is kept between documents. A document of version 12
// (shapes written in the attributes) saved after one of an older version (shape section) was
// written with the set of the older version and could not be read (PCDM_RS_DriverFailure).
TEST(BinDrivers_DocumentRetrievalDriverTest, SaveVersionsInTurn_AllReadable)
{
  occ::handle<TDocStd_Application> anApp = new TDocStd_Application();
  BinDrivers::DefineFormat(anApp);
  const TDocStd_FormatVersion aVersions[] = {TDocStd_FormatVersion_VERSION_11,
                                             TDocStd_FormatVersion_VERSION_12,
                                             TDocStd_FormatVersion_VERSION_11,
                                             TDocStd_FormatVersion_VERSION_12};
  std::string                 aData[4];
  for (int i = 0; i < 4; ++i)
  {
    occ::handle<TDocStd_Document> aDoc;
    anApp->NewDocument("BinOcaf", aDoc);
    TNaming_Builder aBuilder(aDoc->Main().FindChild(1, true));
    aBuilder.Generated(BRepPrimAPI_MakeBox(1.0 + i, 2.0, 3.0).Shape());
    aDoc->ChangeStorageFormatVersion(aVersions[i]);
    std::ostringstream anOut;
    EXPECT_EQ(anApp->SaveAs(aDoc, anOut), PCDM_SS_OK) << i;
    anApp->Close(aDoc);
    aData[i] = anOut.str();
  }
  for (int i = 0; i < 4; ++i)
  {
    occ::handle<TDocStd_Document> aDoc;
    std::istringstream            anIn(aData[i]);
    ASSERT_EQ(anApp->Open(anIn, aDoc), PCDM_RS_OK) << i;
    occ::handle<TNaming_NamedShape> aNamed;
    ASSERT_TRUE(aDoc->Main().FindChild(1, false).FindAttribute(TNaming_NamedShape::GetID(), aNamed))
      << i;
    int aNbFaces = 0;
    for (TopExp_Explorer anExp(aNamed->Get(), TopAbs_FACE); anExp.More(); anExp.Next())
    {
      ++aNbFaces;
    }
    EXPECT_EQ(aNbFaces, 6) << i;
    anApp->Close(aDoc);
  }
}
