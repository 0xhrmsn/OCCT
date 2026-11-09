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

namespace
{

//! Document with a box and one dimension of theType between two faces of the box.
occ::handle<TDocStd_Document> documentWithDimension(
  const XCAFDimTolObjects_DimensionType                 theType,
  const occ::handle<XCAFDimTolObjects_DimensionObject>& theObject)
{
  occ::handle<TDocStd_Application> anApp = XCAFApp_Application::GetApplication();
  occ::handle<TDocStd_Document>    aDoc;
  anApp->NewDocument("MDTV-XCAF", aDoc);
  const occ::handle<XCAFDoc_ShapeTool>  aShapeTool = XCAFDoc_DocumentTool::ShapeTool(aDoc->Main());
  const occ::handle<XCAFDoc_DimTolTool> aDimTolTool =
    XCAFDoc_DocumentTool::DimTolTool(aDoc->Main());
  const TopoDS_Shape aBox      = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  const TDF_Label    aBoxLabel = aShapeTool->AddShape(aBox);
  TopExp_Explorer    anExp(aBox, TopAbs_FACE);
  const TDF_Label    aFirst = aShapeTool->AddSubShape(aBoxLabel, anExp.Current());
  anExp.Next();
  anExp.Next();
  const TDF_Label aSecond = aShapeTool->AddSubShape(aBoxLabel, anExp.Current());
  const TDF_Label aDimL   = aDimTolTool->AddDimension();
  aDimTolTool->SetDimension(aFirst, aSecond, aDimL);
  theObject->SetType(theType);
  XCAFDoc_Dimension::Set(aDimL)->SetObject(theObject);
  return aDoc;
}

//! Writes theDoc in AP242 with its GD&T and reads it back; returns the dimension read.
occ::handle<XCAFDimTolObjects_DimensionObject> roundTrip(
  const occ::handle<TDocStd_Document>& theDoc,
  std::string&                         theContent)
{
  DESTEP_Parameters aParams;
  aParams.InitFromStatic();
  aParams.WriteSchema = DESTEP_Parameters::WriteMode_StepSchema_AP242DIS;
  STEPCAFControl_Writer aWriter;
  aWriter.SetDimTolMode(true);
  EXPECT_TRUE(aWriter.Transfer(theDoc, aParams));
  std::ostringstream anOut;
  EXPECT_EQ(aWriter.ChangeWriter().WriteStream(anOut), IFSelect_RetDone);
  theContent = anOut.str();

  occ::handle<TDocStd_Document> aRead;
  XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aRead);
  STEPCAFControl_Reader aReader;
  aReader.SetGDTMode(true);
  std::istringstream anIn(theContent);
  EXPECT_EQ(aReader.ReadStream("angular.stp", anIn), IFSelect_RetDone);
  EXPECT_TRUE(aReader.Transfer(aRead));
  NCollection_Sequence<TDF_Label> aDims;
  XCAFDoc_DocumentTool::DimTolTool(aRead->Main())->GetDimensionLabels(aDims);
  EXPECT_EQ(aDims.Length(), 1);
  occ::handle<XCAFDoc_Dimension> anAttr;
  if (aDims.IsEmpty() || !aDims.First().FindAttribute(XCAFDoc_Dimension::GetID(), anAttr))
  {
    return occ::handle<XCAFDimTolObjects_DimensionObject>();
  }
  return anAttr->GetObject();
}

} // namespace

// The angles of XCAF are in degrees; STEP writes them in the plane angle unit of the context
// (radian), so 60 degrees is read back as 60, not as 60 radians (3437.7 degrees).
TEST(STEPCAFControl_AngularDimensionTest, AngularLocation_ValueKept)
{
  occ::handle<XCAFDimTolObjects_DimensionObject> anObject = new XCAFDimTolObjects_DimensionObject;
  anObject->SetValue(60.0);
  std::string                                          aContent;
  const occ::handle<XCAFDimTolObjects_DimensionObject> aRead =
    roundTrip(documentWithDimension(XCAFDimTolObjects_DimensionType_Location_Angular, anObject),
              aContent);
  ASSERT_FALSE(aRead.IsNull());
  EXPECT_EQ(aRead->GetType(), XCAFDimTolObjects_DimensionType_Location_Angular);
  EXPECT_NEAR(aRead->GetValue(), 60.0, 1.0e-9);
  // the file holds the angle in radians, as declared by its unit
  EXPECT_NE(aContent.find("1.0471975511"), std::string::npos);
}

// The plus/minus tolerance of an angular dimension is an angle too.
TEST(STEPCAFControl_AngularDimensionTest, AngularSize_ToleranceKept)
{
  occ::handle<XCAFDimTolObjects_DimensionObject> anObject = new XCAFDimTolObjects_DimensionObject;
  anObject->SetValue(45.0);
  ASSERT_TRUE(anObject->SetUpperTolValue(0.5));
  ASSERT_TRUE(anObject->SetLowerTolValue(-0.25));
  std::string                                          aContent;
  const occ::handle<XCAFDimTolObjects_DimensionObject> aRead =
    roundTrip(documentWithDimension(XCAFDimTolObjects_DimensionType_Size_Angular, anObject),
              aContent);
  ASSERT_FALSE(aRead.IsNull());
  EXPECT_NEAR(aRead->GetValue(), 45.0, 1.0e-9);
  ASSERT_TRUE(aRead->IsDimWithPlusMinusTolerance());
  EXPECT_NEAR(aRead->GetUpperTolValue(), 0.5, 1.0e-9);
  EXPECT_NEAR(std::abs(aRead->GetLowerTolValue()), 0.25, 1.0e-9);
}

// Guard: a linear dimension is written and read in length units, unchanged.
TEST(STEPCAFControl_AngularDimensionTest, LinearDistance_ValueKept)
{
  occ::handle<XCAFDimTolObjects_DimensionObject> anObject = new XCAFDimTolObjects_DimensionObject;
  anObject->SetValue(20.0);
  ASSERT_TRUE(anObject->SetUpperTolValue(0.1));
  ASSERT_TRUE(anObject->SetLowerTolValue(-0.1));
  std::string                                          aContent;
  const occ::handle<XCAFDimTolObjects_DimensionObject> aRead = roundTrip(
    documentWithDimension(XCAFDimTolObjects_DimensionType_Location_LinearDistance, anObject),
    aContent);
  ASSERT_FALSE(aRead.IsNull());
  EXPECT_NEAR(aRead->GetValue(), 20.0, 1.0e-9);
  EXPECT_NEAR(aRead->GetUpperTolValue(), 0.1, 1.0e-9);
}

// The bounds of an angular dimension with a range are angles too.
TEST(STEPCAFControl_AngularDimensionTest, AngularLocation_RangeKept)
{
  occ::handle<XCAFDimTolObjects_DimensionObject> anObject = new XCAFDimTolObjects_DimensionObject;
  anObject->SetLowerBound(10.0);
  anObject->SetUpperBound(20.0);
  std::string                                          aContent;
  const occ::handle<XCAFDimTolObjects_DimensionObject> aRead =
    roundTrip(documentWithDimension(XCAFDimTolObjects_DimensionType_Location_Angular, anObject),
              aContent);
  ASSERT_FALSE(aRead.IsNull());
  ASSERT_TRUE(aRead->IsDimWithRange());
  EXPECT_NEAR(aRead->GetLowerBound(), 10.0, 1.0e-9);
  EXPECT_NEAR(aRead->GetUpperBound(), 20.0, 1.0e-9);
}

// The value of a runout tolerance zone is an angle in degrees.
TEST(STEPCAFControl_AngularDimensionTest, RunoutZone_ValueKept)
{
  occ::handle<TDocStd_Document> aDoc;
  XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aDoc);
  const occ::handle<XCAFDoc_ShapeTool>  aShapeTool = XCAFDoc_DocumentTool::ShapeTool(aDoc->Main());
  const occ::handle<XCAFDoc_DimTolTool> aDimTolTool =
    XCAFDoc_DocumentTool::DimTolTool(aDoc->Main());
  const TopoDS_Shape aBox      = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  const TDF_Label    aBoxLabel = aShapeTool->AddShape(aBox);
  const TDF_Label    aFace =
    aShapeTool->AddSubShape(aBoxLabel, TopExp_Explorer(aBox, TopAbs_FACE).Current());
  const TDF_Label aTolL = aDimTolTool->AddGeomTolerance();
  aDimTolTool->SetGeomTolerance(aFace, aTolL);
  TopExp_Explorer aFaceExp(aBox, TopAbs_FACE);
  aFaceExp.Next();
  NCollection_Sequence<TDF_Label> aDatumShapes;
  aDatumShapes.Append(aShapeTool->AddSubShape(aBoxLabel, aFaceExp.Current()));
  const TDF_Label aDatumL = aDimTolTool->AddDatum();
  aDimTolTool->SetDatum(aDatumShapes, aDatumL);
  occ::handle<XCAFDimTolObjects_DatumObject> aDatum = new XCAFDimTolObjects_DatumObject;
  aDatum->SetName(new TCollection_HAsciiString("A"));
  aDatum->SetPosition(1);
  XCAFDoc_Datum::Set(aDatumL)->SetObject(aDatum);
  aDimTolTool->SetDatumToGeomTol(aDatumL, aTolL);
  occ::handle<XCAFDimTolObjects_GeomToleranceObject> anObject =
    new XCAFDimTolObjects_GeomToleranceObject;
  anObject->SetType(XCAFDimTolObjects_GeomToleranceType_CircularRunout);
  anObject->SetValue(0.1);
  anObject->SetTypeOfValue(XCAFDimTolObjects_GeomToleranceTypeValue_Diameter);
  anObject->SetZoneModifier(XCAFDimTolObjects_GeomToleranceZoneModif_Runout);
  anObject->SetValueOfZoneModifier(30.0);
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
  ASSERT_EQ(aReader.ReadStream("runout.stp", anIn), IFSelect_RetDone);
  ASSERT_TRUE(aReader.Transfer(aRead));
  NCollection_Sequence<TDF_Label> aTols;
  XCAFDoc_DocumentTool::DimTolTool(aRead->Main())->GetGeomToleranceLabels(aTols);
  ASSERT_EQ(aTols.Length(), 1);
  occ::handle<XCAFDoc_GeomTolerance> anAttr;
  ASSERT_TRUE(aTols.First().FindAttribute(XCAFDoc_GeomTolerance::GetID(), anAttr));
  EXPECT_NEAR(anAttr->GetObject()->GetValueOfZoneModifier(), 30.0, 1.0e-9);
}
