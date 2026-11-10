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

#include <BRep_Builder.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepTools.hxx>
#include <BinTools.hxx>
#include <Message_ProgressIndicator.hxx>
#include <TopoDS_Shape.hxx>

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>
#include <string>

namespace
{

//! Progress indicator of a user who cancels at once.
class CancellingIndicator : public Message_ProgressIndicator
{
public:
  bool UserBreak() override { return true; }

  void Show(const Message_ProgressScope&, const bool) override {}
};

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

  bool HasCancelled() const { return myNbChecks < 0; }

private:
  int myNbChecks;
};

std::string tempFile(const char* theName)
{
  return (std::filesystem::temp_directory_path() / theName).string();
}

} // namespace

// A cancelled writing or reading of a BREP file reported success for a truncated file or an
// incomplete shape.
TEST(BRepTools_UserBreakTest, CancelledFileWriteRead)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  const std::string  aBin = tempFile("occt_userbreak.bbrep");
  const std::string  aTxt = tempFile("occt_userbreak.brep");

  occ::handle<CancellingIndicator> aCancel = new CancellingIndicator();
  EXPECT_FALSE(BinTools::Write(aBox, aBin.c_str(), aCancel->Start()));
  EXPECT_FALSE(BRepTools::Write(aBox, aTxt.c_str(), aCancel->Start()));

  // complete files, read with cancelling
  ASSERT_TRUE(BinTools::Write(aBox, aBin.c_str()));
  ASSERT_TRUE(BRepTools::Write(aBox, aTxt.c_str()));
  TopoDS_Shape aRead;
  EXPECT_FALSE(BinTools::Read(aRead, aBin.c_str(), aCancel->Start()));
  EXPECT_TRUE(aRead.IsNull());
  BRep_Builder aBuilder;
  EXPECT_FALSE(BRepTools::Read(aRead, aTxt.c_str(), aBuilder, aCancel->Start()));
  EXPECT_TRUE(aRead.IsNull());

  // guard: without cancelling
  EXPECT_TRUE(BinTools::Read(aRead, aBin.c_str()));
  EXPECT_FALSE(aRead.IsNull());
  aRead.Nullify();
  EXPECT_TRUE(BRepTools::Read(aRead, aTxt.c_str(), aBuilder));
  EXPECT_FALSE(aRead.IsNull());

  std::filesystem::remove(aBin);
  std::filesystem::remove(aTxt);
}

// A stream reading cancelled in the middle of the tables went on assembling the shape from them
// and threw NCollection_IndexedMap::FindKey.
TEST(BRepTools_UserBreakTest, CancelledStreamRead)
{
  const TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  std::ostringstream aBinOut;
  BinTools::Write(aBox, aBinOut);
  std::ostringstream aTxtOut;
  BRepTools::Write(aBox, aTxtOut);

  for (const int aNbChecks : {0, 1, 2, 3, 5, 8, 13, 21, 1000})
  {
    occ::handle<LateCancellingIndicator> aCancel = new LateCancellingIndicator(aNbChecks);
    TopoDS_Shape                         aRead;
    std::istringstream                   aBinIn(aBinOut.str());
    EXPECT_NO_THROW(BinTools::Read(aRead, aBinIn, aCancel->Start())) << aNbChecks;
    EXPECT_EQ(aRead.IsNull(), aCancel->HasCancelled()) << aNbChecks;

    aCancel = new LateCancellingIndicator(aNbChecks);
    BRep_Builder       aBuilder;
    std::istringstream aTxtIn(aTxtOut.str());
    aRead.Nullify();
    EXPECT_NO_THROW(BRepTools::Read(aRead, aTxtIn, aBuilder, aCancel->Start())) << aNbChecks;
    EXPECT_EQ(aRead.IsNull(), aCancel->HasCancelled()) << aNbChecks;
  }
}

// A binary BREP written to a file that cannot be created used the null stream before checking it.
TEST(BRepTools_UserBreakTest, WriteToMissingFolder)
{
  const TopoDS_Shape aBox    = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  const std::string  aFolder = tempFile("occt_missing_folder_for_brep");
  ASSERT_FALSE(std::filesystem::exists(aFolder));
  EXPECT_FALSE(BinTools::Write(aBox, (aFolder + "/box.bbrep").c_str()));
  EXPECT_FALSE(BRepTools::Write(aBox, (aFolder + "/box.brep").c_str()));
}
