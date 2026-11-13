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

#include <BRepPrimAPI_MakeBox.hxx>
#include <DESTEP_Parameters.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDimTolObjects_DimensionObject.hxx>
#include <XCAFDimTolObjects_DatumObject.hxx>
#include <XCAFDimTolObjects_GeomToleranceObject.hxx>
#include <XCAFDoc_Datum.hxx>
#include <XCAFDoc_GeomTolerance.hxx>
#include <XCAFDoc_Dimension.hxx>
#include <XCAFDoc_DimTolTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <sstream>

// The maximum tolerance modifier is a length: it was converted as an angle with the plane angle
// factor of a length unit (0), so it was always read as 0 and lost on the next write. In a
// document without dimensions the writer also labelled the tolerance values and the values of the
// datum modifiers (in mm) as metres: 0.1 was read back as 100.
TEST(STEPCAFControl_GeomToleranceTest, MaxValueModifier_ValueKept)
{
  for (const bool isWithDatum : {false, true})
  {
    occ::handle<TDocStd_Document> aDoc;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aDoc);
    const occ::handle<XCAFDoc_ShapeTool> aShapeTool =
      XCAFDoc_DocumentTool::ShapeTool(aDoc->Main());
    const occ::handle<XCAFDoc_DimTolTool> aDimTolTool =
      XCAFDoc_DocumentTool::DimTolTool(aDoc->Main());
    const TopoDS_Shape aBox      = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    const TDF_Label    aBoxLabel = aShapeTool->AddShape(aBox);
    TopExp_Explorer    aFaceExp(aBox, TopAbs_FACE);
    const TDF_Label    aFace = aShapeTool->AddSubShape(aBoxLabel, aFaceExp.Current());
    const TDF_Label    aTolL = aDimTolTool->AddGeomTolerance();
    aDimTolTool->SetGeomTolerance(aFace, aTolL);
    if (isWithDatum)
    {
      aFaceExp.Next();
      NCollection_Sequence<TDF_Label> aDatumShapes;
      aDatumShapes.Append(aShapeTool->AddSubShape(aBoxLabel, aFaceExp.Current()));
      const TDF_Label aDatumL = aDimTolTool->AddDatum();
      aDimTolTool->SetDatum(aDatumShapes, aDatumL);
      occ::handle<XCAFDimTolObjects_DatumObject> aDatum = new XCAFDimTolObjects_DatumObject;
      aDatum->SetName(new TCollection_HAsciiString("A"));
      aDatum->SetPosition(1);
      aDatum->SetModifierWithValue(XCAFDimTolObjects_DatumModifWithValue_Distance, 2.0);
      XCAFDoc_Datum::Set(aDatumL)->SetObject(aDatum);
      aDimTolTool->SetDatumToGeomTol(aDatumL, aTolL);
    }
    occ::handle<XCAFDimTolObjects_GeomToleranceObject> anObject =
      new XCAFDimTolObjects_GeomToleranceObject;
    anObject->SetType(isWithDatum ? XCAFDimTolObjects_GeomToleranceType_Parallelism
                                  : XCAFDimTolObjects_GeomToleranceType_Flatness);
    anObject->SetValue(0.1);
    anObject->AddModifier(XCAFDimTolObjects_GeomToleranceModif_Statistical_Tolerance);
    anObject->SetMaxValueModifier(0.5);
    XCAFDoc_GeomTolerance::Set(aTolL)->SetObject(anObject);

    DESTEP_Parameters aParams;
    aParams.InitFromStatic();
    aParams.WriteSchema = DESTEP_Parameters::WriteMode_StepSchema_AP242DIS;
    STEPCAFControl_Writer aWriter;
    aWriter.SetDimTolMode(true);
    ASSERT_TRUE(aWriter.Transfer(aDoc, aParams));
    std::ostringstream anOut;
    ASSERT_EQ(aWriter.ChangeWriter().WriteStream(anOut), IFSelect_RetDone);

    occ::handle<TDocStd_Document> aRead;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aRead);
    STEPCAFControl_Reader aReader;
    aReader.SetGDTMode(true);
    std::istringstream anIn(anOut.str());
    ASSERT_EQ(aReader.ReadStream("maxtol.stp", anIn), IFSelect_RetDone);
    ASSERT_TRUE(aReader.Transfer(aRead));
    NCollection_Sequence<TDF_Label> aTols;
    XCAFDoc_DocumentTool::DimTolTool(aRead->Main())->GetGeomToleranceLabels(aTols);
    ASSERT_EQ(aTols.Length(), 1) << "datum " << isWithDatum;
    occ::handle<XCAFDoc_GeomTolerance> anAttr;
    ASSERT_TRUE(aTols.First().FindAttribute(XCAFDoc_GeomTolerance::GetID(), anAttr));
    EXPECT_NEAR(anAttr->GetObject()->GetValue(), 0.1, 1.0e-9) << "datum " << isWithDatum;
    EXPECT_NEAR(anAttr->GetObject()->GetMaxValueModifier(), 0.5, 1.0e-9)
      << "datum " << isWithDatum;
    if (isWithDatum)
    {
      // the value of the datum modifier is a length written with the same unit
      NCollection_Sequence<TDF_Label> aDatums;
      XCAFDoc_DocumentTool::DimTolTool(aRead->Main())->GetDatumLabels(aDatums);
      ASSERT_EQ(aDatums.Length(), 1);
      occ::handle<XCAFDoc_Datum> aDatumAttr;
      ASSERT_TRUE(aDatums.First().FindAttribute(XCAFDoc_Datum::GetID(), aDatumAttr));
      XCAFDimTolObjects_DatumModifWithValue aModif = XCAFDimTolObjects_DatumModifWithValue_None;
      double                                aValue = 0.0;
      aDatumAttr->GetObject()->GetModifierWithValue(aModif, aValue);
      EXPECT_EQ(aModif, XCAFDimTolObjects_DatumModifWithValue_Distance);
      EXPECT_NEAR(aValue, 2.0, 1.0e-9);

      // The value was written as an unknown reference and the reader crashed on it: a file whose
      // modifier has no value is read without it.
      std::string  aBroken = anOut.str();
      const size_t aPos    = aBroken.find("DATUM_REFERENCE_MODIFIER_WITH_VALUE(.DISTANCE.,");
      ASSERT_NE(aPos, std::string::npos);
      const size_t aRef = aBroken.find('#', aPos);
      const size_t anEnd = aBroken.find(')', aRef);
      aBroken.replace(aRef, anEnd - aRef, "$");
      occ::handle<TDocStd_Document> aReadBroken;
      XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aReadBroken);
      STEPCAFControl_Reader aBrokenReader;
      aBrokenReader.SetGDTMode(true);
      std::istringstream aBrokenIn(aBroken);
      ASSERT_EQ(aBrokenReader.ReadStream("broken.stp", aBrokenIn), IFSelect_RetDone);
      EXPECT_NO_THROW(aBrokenReader.Transfer(aReadBroken));
    }
  }
}
