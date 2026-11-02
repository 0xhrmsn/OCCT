// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <BRepPrimAPI_MakeBox.hxx>
#include <NCollection_Sequence.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TDataStd_NamedData.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <gtest/gtest.h>

#include <sstream>

// The writer stores string metadata as UTF-8; the reader converted it back as Latin-1.
TEST(STEPCAFControl_MetadataTest, StringMetadata_NonAscii_RoundTrip)
{
  occ::handle<TDocStd_Document> aDoc;
  XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aDoc);
  const TDF_Label aLabel = XCAFDoc_DocumentTool::ShapeTool(aDoc->Main())
                             ->AddShape(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape());
  // "PN-123/é 日本"
  const TCollection_ExtendedString aValue("PN-123/\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC", true);
  TDataStd_NamedData::Set(aLabel)->SetString("PART_NUMBER", aValue);
  TDataStd_NamedData::Set(aLabel)->SetString("ProductID", "PRD-0001");

  STEPCAFControl_Writer aWriter;
  ASSERT_TRUE(aWriter.Transfer(aDoc));
  std::ostringstream anOut;
  ASSERT_EQ(aWriter.WriteStream(anOut), IFSelect_RetDone);

  STEPCAFControl_Reader aReader;
  aReader.SetMetaMode(true);
  std::istringstream anIn(anOut.str());
  ASSERT_EQ(aReader.ReadStream("metadata.stp", anIn), IFSelect_RetDone);
  occ::handle<TDocStd_Document> aRead;
  XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aRead);
  ASSERT_TRUE(aReader.Transfer(aRead));

  NCollection_Sequence<TDF_Label> aFree;
  XCAFDoc_DocumentTool::ShapeTool(aRead->Main())->GetFreeShapes(aFree);
  ASSERT_EQ(aFree.Length(), 1);
  occ::handle<TDataStd_NamedData> aData;
  ASSERT_TRUE(aFree(1).FindAttribute(TDataStd_NamedData::GetID(), aData));
  ASSERT_TRUE(aData->HasString("PART_NUMBER"));
  EXPECT_TRUE(aData->GetString("PART_NUMBER") == aValue);
  EXPECT_TRUE(aData->GetString("ProductID") == "PRD-0001");
}