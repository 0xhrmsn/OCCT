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

#include <Message_Messenger.hxx>
#include <Storage_HeaderData.hxx>
#include <TopLoc_Location.hxx>
#include <XmlMXCAFDoc_LocationDriver.hxx>
#include <XmlObjMgt_Document.hxx>
#include <XmlObjMgt_Element.hxx>
#include <XmlObjMgt_RRelocationTable.hxx>

#include <vector>

namespace
{
//! Reads the location of format version 5 held by a "location" element with theAttributes.
bool readVersion5(const std::vector<std::pair<const char*, int>>& theAttributes,
                  XmlObjMgt_RRelocationTable&                     theTable,
                  TopLoc_Location&                                theLoc)
{
  XmlObjMgt_Document aDoc    = XmlObjMgt_Document::createDocument("document");
  XmlObjMgt_Element  aParent = aDoc.getDocumentElement();
  XmlObjMgt_Element  aLoc    = aDoc.createElement("location");
  for (const auto& anAttribute : theAttributes)
  {
    aLoc.setAttribute(anAttribute.first, anAttribute.second);
  }
  aParent.appendChild(aLoc);

  occ::handle<Storage_HeaderData> aHeader = new Storage_HeaderData();
  aHeader->SetStorageVersion(5);
  theTable.SetHeaderData(aHeader);
  occ::handle<XmlMXCAFDoc_LocationDriver> aDriver =
    new XmlMXCAFDoc_LocationDriver(new Message_Messenger());
  return aDriver->Translate(aParent, theLoc, theTable);
}
} // namespace

// The datums share the identifiers of the attributes of the document: a datum identifier naming
// another object gave a null datum, then a null dereference. A missing power was read
// uninitialized.
TEST(XmlMXCAFDoc_LocationDriverTest, Read_DamagedItem)
{
  {
    XmlObjMgt_RRelocationTable aTable;
    aTable.Bind(7, new Standard_Transient());
    TopLoc_Location aLoc;
    EXPECT_FALSE(readVersion5({{"power", 1}, {"datum", 7}}, aTable, aLoc));
  }
  {
    XmlObjMgt_RRelocationTable aTable;
    TopLoc_Location            aLoc;
    EXPECT_FALSE(readVersion5({{"datum", 7}}, aTable, aLoc));
  }
}
