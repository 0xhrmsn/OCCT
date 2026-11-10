// Created on: 2005-05-17
// Created by: Eugeny NAPALKOV
// Copyright (c) 2005-2014 OPEN CASCADE SAS
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

#include <BinMDataStd.hxx>
#include <BinMXCAFDoc_LocationDriver.hxx>
#include <BinObjMgt_Persistent.hxx>
#include <BinTools_LocationSet.hxx>
#include <BinTools_ShapeReader.hxx>
#include <BinTools_ShapeWriter.hxx>
#include <Message_Messenger.hxx>
#include <gp_Mat.hxx>
#include <gp_Trsf.hxx>
#include <gp_XYZ.hxx>
#include <Standard_Type.hxx>
#include <TDF_Attribute.hxx>
#include <TopLoc_Datum3D.hxx>
#include <TopLoc_Location.hxx>
#include <XCAFDoc_Location.hxx>
#include <NCollection_Vector.hxx>

IMPLEMENT_STANDARD_RTTIEXT(BinMXCAFDoc_LocationDriver, BinMDF_ADriver)

namespace
{
//! Maximal number of items of a location read from a document. Composing and destroying a
//! location recurse once per item.
constexpr int THE_MAX_LOCATION_ITEMS = 1000;
} // namespace

// #include <Precision.hxx>
//=================================================================================================

BinMXCAFDoc_LocationDriver::BinMXCAFDoc_LocationDriver(
  const occ::handle<Message_Messenger>& theMsgDriver)
    : BinMDF_ADriver(theMsgDriver, STANDARD_TYPE(XCAFDoc_Location)->Name())
{
}

//=================================================================================================

occ::handle<TDF_Attribute> BinMXCAFDoc_LocationDriver::NewEmpty() const
{
  return new XCAFDoc_Location();
}

//=================================================================================================

bool BinMXCAFDoc_LocationDriver::Paste(const BinObjMgt_Persistent&       theSource,
                                       const occ::handle<TDF_Attribute>& theTarget,
                                       BinObjMgt_RRelocationTable&       theRelocTable) const
{
  occ::handle<XCAFDoc_Location> anAtt = occ::down_cast<XCAFDoc_Location>(theTarget);
  TopLoc_Location               aLoc;
  bool                          aRes = Translate(theSource, aLoc, theRelocTable);
  anAtt->Set(aLoc);
  return aRes;
}

//=================================================================================================

void BinMXCAFDoc_LocationDriver::Paste(
  const occ::handle<TDF_Attribute>&                        theSource,
  BinObjMgt_Persistent&                                    theTarget,
  NCollection_IndexedMap<occ::handle<Standard_Transient>>& theRelocTable) const
{
  occ::handle<XCAFDoc_Location> anAtt = occ::down_cast<XCAFDoc_Location>(theSource);
  TopLoc_Location               aLoc  = anAtt->Get();
  Translate(aLoc, theTarget, theRelocTable);
}

//=================================================================================================

bool BinMXCAFDoc_LocationDriver::Translate(const BinObjMgt_Persistent& theSource,
                                           TopLoc_Location&            theLoc,
                                           BinObjMgt_RRelocationTable& theMap) const
{
  if (!myNSDriver.IsNull() && myNSDriver->IsQuickPart())
  {
    BinTools_IStream aDirectStream(*(const_cast<BinObjMgt_Persistent*>(&theSource)->GetIStream()));
    BinTools_ShapeReader* aReader = static_cast<BinTools_ShapeReader*>(myNSDriver->ShapeSet(true));
    theLoc                        = *(aReader->ReadLocation(aDirectStream));
    return true;
  }

  // The items are read in a loop: reading them recursively let a damaged or crafted document,
  // a few bytes per item, overflow the stack. A location written by OCCT has one item per
  // elementary location it is composed of.
  NCollection_Vector<TopLoc_Location> anItems;
  for (;;)
  {
    int anId = 0;
    theSource >> anId;
    // at the end of the data the value is left unchanged: a truncated location is not an identity
    if (theSource.IsError())
    {
      return false;
    }
    if (anId == 0)
    {
      break;
    }
    if (anItems.Length() >= THE_MAX_LOCATION_ITEMS)
    {
      return false;
    }
    occ::handle<TopLoc_Datum3D> aDatum;
    int                         aPower = 0;
    if (!readItem(theSource, anId, theMap, aDatum, aPower))
    {
      return false;
    }
    anItems.Append(TopLoc_Location(aDatum).Powered(aPower));
  }

  // compose as the recursive reading did: the item read first is applied last
  TopLoc_Location aLoc;
  for (int anIndex = anItems.Upper(); anIndex >= anItems.Lower(); --anIndex)
  {
    aLoc = aLoc * anItems.Value(anIndex);
  }
  theLoc = aLoc;
  return true;
}

//=================================================================================================

bool BinMXCAFDoc_LocationDriver::readItem(const BinObjMgt_Persistent&  theSource,
                                          const int                    theId,
                                          BinObjMgt_RRelocationTable&  theMap,
                                          occ::handle<TopLoc_Datum3D>& theDatum,
                                          int&                         thePower) const
{
  int aFileVer = theMap.GetHeaderData()->StorageVersion().IntegerValue();
  if (aFileVer >= TDocStd_FormatVersion_VERSION_6 && myNSDriver.IsNull())
  {
    return false;
  }

  int                         aPower(0);
  occ::handle<TopLoc_Datum3D> aDatum;

  if (aFileVer >= TDocStd_FormatVersion_VERSION_6)
  {
    const TopLoc_Location& aLoc = myNSDriver->GetShapesLocations().Location(theId);
    aPower                      = aLoc.FirstPower();
    aDatum                      = aLoc.FirstDatum();
  }
  else
  {
    theSource >> aPower;

    int aDatumID   = -1;
    int aReadDatum = -1;
    theSource >> aReadDatum;
    theSource >> aDatumID;
    if (aReadDatum != -1)
    {
      if (theMap.IsBound(aDatumID))
      {
        aDatum = occ::down_cast<TopLoc_Datum3D>(theMap.Find(aDatumID));
      }
      // the datums share the identifiers of the attributes: another object is not a datum
      if (aDatum.IsNull())
      {
        return false;
      }
    }
    else
    {
      // read the datum's transformation
      gp_Trsf aTrsf;

      double aScaleFactor = 1.0;
      theSource >> aScaleFactor;
      aTrsf.SetScaleFactor(aScaleFactor);

      int aForm = 0;
      theSource >> aForm;
      aTrsf.SetForm((gp_TrsfForm)aForm);

      int     R, C;
      gp_Mat& aMat = (gp_Mat&)aTrsf.HVectorialPart();
      for (R = 1; R <= 3; R++)
      {
        for (C = 1; C <= 3; C++)
        {
          double aVal = 0.0;
          theSource >> aVal;
          aMat.SetValue(R, C, aVal);
        }
      }

      double x = 0.0, y = 0.0, z = 0.0;
      theSource >> x >> y >> z;
      gp_XYZ& aLoc = (gp_XYZ&)aTrsf.TranslationPart();
      aLoc.SetX(x);
      aLoc.SetY(y);
      aLoc.SetZ(z);

      aDatum = new TopLoc_Datum3D(aTrsf);
      theMap.Bind(aDatumID, aDatum);
    }
  }

  if (theSource.IsError())
  {
    // the item is truncated
    return false;
  }
  theDatum = aDatum;
  thePower = aPower;
  return true;
}

//=================================================================================================

void BinMXCAFDoc_LocationDriver::Translate(
  const TopLoc_Location&                                   theLoc,
  BinObjMgt_Persistent&                                    theTarget,
  NCollection_IndexedMap<occ::handle<Standard_Transient>>& theMap) const
{
  if (!myNSDriver.IsNull() && myNSDriver->IsQuickPart())
  { // write directly to the stream
    Standard_OStream*     aDirectStream = theTarget.GetOStream();
    BinTools_ShapeWriter* aWriter = static_cast<BinTools_ShapeWriter*>(myNSDriver->ShapeSet(false));
    BinTools_OStream      aStream(*aDirectStream);
    aWriter->WriteLocation(aStream, theLoc);
    return;
  }
  if (theLoc.IsIdentity())
  {
    theTarget.PutInteger(0);
    return;
  }

  // The location is not identity
  if (myNSDriver.IsNull())
  {
#ifdef OCCT_DEBUG
    std::cout << "NamedShape Driver is NULL\n";
#endif
    return;
  }

  int anId = myNSDriver->GetShapesLocations().Add(theLoc);
  theTarget << anId;

  // In earlier version of this driver a datums from location stored in
  // the relocation table, but now it's not necessary
  // (try to uncomment it if some problems appear)
  /*
  occ::handle<TopLoc_Datum3D> aDatum = theLoc.FirstDatum();

  if(!theMap.Contains(aDatum)) {
    theMap.Add(aDatum);
  }
  */

  Translate(theLoc.NextLocation(), theTarget, theMap);
}
