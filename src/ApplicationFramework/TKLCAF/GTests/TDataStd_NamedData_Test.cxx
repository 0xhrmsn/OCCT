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

#include <NCollection_HArray1.hxx>
#include <TDataStd_NamedData.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <TDF_Label.hxx>

#include <gtest/gtest.h>

namespace
{
occ::handle<NCollection_HArray1<int>> intArray(const int theValue)
{
  occ::handle<NCollection_HArray1<int>> anArr = new NCollection_HArray1<int>(1, 2);
  anArr->SetValue(1, theValue);
  anArr->SetValue(2, -theValue);
  return anArr;
}

occ::handle<NCollection_HArray1<double>> realArray(const double theValue)
{
  occ::handle<NCollection_HArray1<double>> anArr = new NCollection_HArray1<double>(1, 1);
  anArr->SetValue(1, theValue);
  return anArr;
}

// Expects the state after the first command: one value of each category, keyed "a".
void expectFirstState(const occ::handle<TDataStd_NamedData>& theND)
{
  EXPECT_TRUE(theND->HasInteger("a"));
  EXPECT_EQ(theND->GetInteger("a"), 1);
  EXPECT_FALSE(theND->HasInteger("b"));
  EXPECT_FALSE(theND->HasReals());
  EXPECT_FALSE(theND->HasStrings());
  EXPECT_FALSE(theND->HasBytes());
  ASSERT_TRUE(theND->HasArrayOfIntegers("a"));
  EXPECT_EQ(theND->GetArrayOfIntegers("a")->Value(1), 1);
  EXPECT_FALSE(theND->HasArrayOfIntegers("b"));
  EXPECT_FALSE(theND->HasArraysOfReals());
}
} // namespace

// Undo and redo must restore every category exactly: values added to a category that was empty,
// and new keys of the array categories, remained after undo (and then also after redo).
TEST(TDataStd_NamedData_Test, UndoRedo_RestoresEveryCategory)
{
  occ::handle<TDocStd_Application> anApp = new TDocStd_Application();
  occ::handle<TDocStd_Document>    aDoc;
  anApp->NewDocument("BinOcaf", aDoc);
  aDoc->SetUndoLimit(10);
  const TDF_Label aLab = aDoc->Main().FindChild(1);

  aDoc->OpenCommand();
  occ::handle<TDataStd_NamedData> aND = TDataStd_NamedData::Set(aLab);
  aND->SetInteger("a", 1);
  aND->SetArrayOfIntegers("a", intArray(1));
  aDoc->CommitCommand();

  aDoc->OpenCommand();
  aND->SetInteger("a", 2);
  aND->SetInteger("b", 3);
  aND->SetReal("r", 2.5);
  aND->SetString("s", "c");
  aND->SetByte("y", 7);
  aND->SetArrayOfIntegers("b", intArray(5));
  aND->SetArrayOfReals("r", realArray(0.5));
  aDoc->CommitCommand();

  ASSERT_TRUE(aDoc->Undo());
  ASSERT_TRUE(aLab.FindAttribute(TDataStd_NamedData::GetID(), aND));
  expectFirstState(aND);

  ASSERT_TRUE(aDoc->Undo());
  EXPECT_FALSE(aLab.FindAttribute(TDataStd_NamedData::GetID(), aND));

  ASSERT_TRUE(aDoc->Redo());
  ASSERT_TRUE(aLab.FindAttribute(TDataStd_NamedData::GetID(), aND));
  expectFirstState(aND);

  ASSERT_TRUE(aDoc->Redo());
  ASSERT_TRUE(aLab.FindAttribute(TDataStd_NamedData::GetID(), aND));
  EXPECT_EQ(aND->GetInteger("a"), 2);
  EXPECT_EQ(aND->GetInteger("b"), 3);
  EXPECT_EQ(aND->GetReal("r"), 2.5);
  EXPECT_EQ(aND->GetString("s"), TCollection_ExtendedString("c"));
  EXPECT_EQ(aND->GetByte("y"), 7);
  ASSERT_TRUE(aND->HasArrayOfIntegers("b"));
  EXPECT_EQ(aND->GetArrayOfIntegers("b")->Value(2), -5);
  ASSERT_TRUE(aND->HasArrayOfReals("r"));
  EXPECT_EQ(aND->GetArrayOfReals("r")->Value(1), 0.5);
}

// A category emptied by a command must come back on undo.
TEST(TDataStd_NamedData_Test, Undo_RestoresClearedCategory)
{
  occ::handle<TDocStd_Application> anApp = new TDocStd_Application();
  occ::handle<TDocStd_Document>    aDoc;
  anApp->NewDocument("BinOcaf", aDoc);
  aDoc->SetUndoLimit(10);
  const TDF_Label aLab = aDoc->Main().FindChild(1);

  aDoc->OpenCommand();
  occ::handle<TDataStd_NamedData> aND = TDataStd_NamedData::Set(aLab);
  aND->SetReal("r", 2.5);
  aDoc->CommitCommand();

  aDoc->OpenCommand();
  aND->ChangeReals(NCollection_DataMap<TCollection_ExtendedString, double>());
  aDoc->CommitCommand();
  EXPECT_FALSE(aND->HasReals());

  ASSERT_TRUE(aDoc->Undo());
  ASSERT_TRUE(aLab.FindAttribute(TDataStd_NamedData::GetID(), aND));
  ASSERT_TRUE(aND->HasReal("r"));
  EXPECT_EQ(aND->GetReal("r"), 2.5);
}
