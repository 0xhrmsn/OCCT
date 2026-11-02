// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <BinLDrivers.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>

#include <gtest/gtest.h>

#include <filesystem>

// SaveAs() into a missing folder returned the uninitialized status of CDF_Store.
TEST(TDocStd_ApplicationTest, SaveAs_MissingFolder_WriteFailure)
{
  occ::handle<TDocStd_Application> anApp = new TDocStd_Application();
  BinLDrivers::DefineFormat(anApp);
  occ::handle<TDocStd_Document> aDoc;
  anApp->NewDocument("BinLOcaf", aDoc);
  ASSERT_FALSE(aDoc.IsNull());

  const std::filesystem::path aFolder =
    std::filesystem::temp_directory_path() / "occt_saveas_missing_folder";
  ASSERT_FALSE(std::filesystem::exists(aFolder));
  const std::string aPath = (aFolder / "doc.cbfl").u8string();
  EXPECT_EQ(anApp->SaveAs(aDoc, TCollection_ExtendedString(aPath.c_str(), true)),
            PCDM_SS_WriteFailure);
  anApp->Close(aDoc);
}