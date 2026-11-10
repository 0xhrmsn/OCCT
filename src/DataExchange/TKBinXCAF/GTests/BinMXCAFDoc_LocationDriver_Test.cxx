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

#include <BinMNaming_NamedShapeDriver.hxx>
#include <BinMXCAFDoc_LocationDriver.hxx>
#include <BinObjMgt_Persistent.hxx>
#include <BinObjMgt_RRelocationTable.hxx>
#include <Message_Messenger.hxx>
#include <Storage_HeaderData.hxx>
#include <TDocStd_FormatVersion.hxx>
#include <TopLoc_Datum3D.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

namespace
{

//! Relocation table of a document of the format version theVersion.
void initTable(BinObjMgt_RRelocationTable& theTable, const int theVersion)
{
  occ::handle<Storage_HeaderData> aHeader = new Storage_HeaderData();
  aHeader->SetStorageVersion(theVersion);
  theTable.SetHeaderData(aHeader);
}

//! Location of theNbItems items as written by the format version 5: the first item stores a
//! translation datum, the others refer to it.
void putVersion5Location(BinObjMgt_Persistent& thePersistent, const int theNbItems)
{
  for (int anItem = 1; anItem <= theNbItems; ++anItem)
  {
    thePersistent.PutInteger(1); // item id
    thePersistent.PutInteger(1); // power
    if (anItem == 1)
    {
      thePersistent.PutInteger(-1); // the datum follows
      thePersistent.PutInteger(7);  // datum id
      thePersistent.PutReal(1.0);   // scale factor
      thePersistent.PutInteger(static_cast<int>(gp_Translation));
      for (int anIndex = 0; anIndex < 9; ++anIndex)
      {
        thePersistent.PutReal(anIndex % 4 == 0 ? 1.0 : 0.0);
      }
      thePersistent.PutReal(1.0);
      thePersistent.PutReal(2.0);
      thePersistent.PutReal(3.0);
    }
    else
    {
      thePersistent.PutInteger(1); // the datum is already read
      thePersistent.PutInteger(7);
    }
  }
  thePersistent.PutInteger(0);
}

//! Reads a location of format version 5 made of theNbItems items.
bool readVersion5Location(const int theNbItems, TopLoc_Location& theLoc)
{
  BinObjMgt_Persistent aPersistent;
  const int            aStart = aPersistent.Position();
  putVersion5Location(aPersistent, theNbItems);
  aPersistent.SetPosition(aStart);

  BinObjMgt_RRelocationTable aTable;
  initTable(aTable, 5);
  occ::handle<BinMXCAFDoc_LocationDriver> aDriver =
    new BinMXCAFDoc_LocationDriver(new Message_Messenger());
  return aDriver->Translate(aPersistent, theLoc, aTable);
}

} // namespace

// The items of a location were read recursively, one level per item of a few bytes: a damaged or
// crafted document overflowed the stack.
TEST(BinMXCAFDoc_LocationDriverTest, Read_LongItemChain)
{
  TopLoc_Location aLoc;
  ASSERT_TRUE(readVersion5Location(3, aLoc));
  const gp_XYZ aMoved = aLoc.Transformation().TranslationPart();
  EXPECT_NEAR((aMoved - gp_XYZ(3.0, 6.0, 9.0)).Modulus(), 0.0, 1e-12);

  ASSERT_TRUE(readVersion5Location(1000, aLoc));
  EXPECT_FALSE(readVersion5Location(1001, aLoc));
  EXPECT_FALSE(readVersion5Location(200000, aLoc));
}

// Guard: a location composed of different items, written and read by the driver, keeps the order
// of its items.
TEST(BinMXCAFDoc_LocationDriverTest, WriteRead_ComposedLocation)
{
  gp_Trsf aTranslation;
  aTranslation.SetTranslation(gp_Vec(10.0, 0.0, 0.0));
  gp_Trsf aRotation;
  aRotation.SetRotation(gp::OZ(), 0.5);
  gp_Trsf aScale;
  aScale.SetScale(gp::Origin(), 2.0);
  const TopLoc_Location aLoc = TopLoc_Location(new TopLoc_Datum3D(aTranslation))
                               * TopLoc_Location(new TopLoc_Datum3D(aRotation)).Powered(3)
                               * TopLoc_Location(new TopLoc_Datum3D(aScale));

  occ::handle<Message_Messenger>           aMessenger = new Message_Messenger();
  occ::handle<BinMNaming_NamedShapeDriver> aNSDriver  = new BinMNaming_NamedShapeDriver(aMessenger);
  occ::handle<BinMXCAFDoc_LocationDriver>  aDriver    = new BinMXCAFDoc_LocationDriver(aMessenger);
  aDriver->SetNSDriver(aNSDriver);

  BinObjMgt_Persistent                                    aPersistent;
  const int                                               aStart = aPersistent.Position();
  NCollection_IndexedMap<occ::handle<Standard_Transient>> aWriteMap;
  aDriver->Translate(aLoc, aPersistent, aWriteMap);
  aPersistent.SetPosition(aStart);

  BinObjMgt_RRelocationTable aTable;
  initTable(aTable, TDocStd_FormatVersion_VERSION_11);
  TopLoc_Location aRead;
  ASSERT_TRUE(aDriver->Translate(aPersistent, aRead, aTable));

  const gp_Trsf aExpected = aLoc.Transformation();
  const gp_Trsf aResult   = aRead.Transformation();
  for (int aRow = 1; aRow <= 3; ++aRow)
  {
    for (int aCol = 1; aCol <= 4; ++aCol)
    {
      EXPECT_NEAR(aResult.Value(aRow, aCol), aExpected.Value(aRow, aCol), 1e-12)
        << aRow << " " << aCol;
    }
  }
}

// A location cut in the middle of an item, or an item whose datum identifier names another
// object of the document, gave a wrong location (uninitialized values) or a null datum.
TEST(BinMXCAFDoc_LocationDriverTest, Read_DamagedItem)
{
  occ::handle<BinMXCAFDoc_LocationDriver> aDriver =
    new BinMXCAFDoc_LocationDriver(new Message_Messenger());

  // cut after four values of the matrix of the datum
  {
    BinObjMgt_Persistent aPersistent;
    const int            aStart = aPersistent.Position();
    aPersistent.PutInteger(1);
    aPersistent.PutInteger(1);
    aPersistent.PutInteger(-1);
    aPersistent.PutInteger(7);
    aPersistent.PutReal(1.0);
    aPersistent.PutInteger(static_cast<int>(gp_Translation));
    for (int anIndex = 0; anIndex < 4; ++anIndex)
    {
      aPersistent.PutReal(0.0);
    }
    aPersistent.SetPosition(aStart);
    BinObjMgt_RRelocationTable aTable;
    initTable(aTable, 5);
    TopLoc_Location aLoc;
    EXPECT_FALSE(aDriver->Translate(aPersistent, aLoc, aTable));
  }

  // the datum identifier names an object that is not a datum
  {
    BinObjMgt_Persistent aPersistent;
    const int            aStart = aPersistent.Position();
    aPersistent.PutInteger(1);
    aPersistent.PutInteger(1);
    aPersistent.PutInteger(1); // the datum is already read
    aPersistent.PutInteger(7);
    aPersistent.PutInteger(0);
    aPersistent.SetPosition(aStart);
    BinObjMgt_RRelocationTable aTable;
    initTable(aTable, 5);
    aTable.Bind(7, new Standard_Transient());
    TopLoc_Location aLoc;
    EXPECT_FALSE(aDriver->Translate(aPersistent, aLoc, aTable));
  }

  // an empty location is the identity
  {
    BinObjMgt_Persistent aPersistent;
    const int            aStart = aPersistent.Position();
    aPersistent.PutInteger(0);
    aPersistent.SetPosition(aStart);
    BinObjMgt_RRelocationTable aTable;
    initTable(aTable, 5);
    TopLoc_Location aLoc;
    EXPECT_TRUE(aDriver->Translate(aPersistent, aLoc, aTable));
    EXPECT_TRUE(aLoc.IsIdentity());
  }
}
