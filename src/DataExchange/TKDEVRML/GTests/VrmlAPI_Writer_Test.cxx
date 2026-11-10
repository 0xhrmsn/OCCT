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

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Quantity_Color.hxx>
#include <Vrml_Material.hxx>
#include <VrmlAPI_Writer.hxx>

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
//! Diffuse color of the first material of the VRML file written by theWriter.
bool writtenDiffuse(VrmlAPI_Writer& theWriter, const int theVersion, double theRgb[3])
{
  TopoDS_Shape aBox = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
  BRepMesh_IncrementalMesh(aBox, 0.5);
  const std::filesystem::path aPath =
    std::filesystem::temp_directory_path() / "occt_vrml_front_material.wrl";
  theWriter.SetRepresentation(VrmlAPI_ShadedRepresentation);
  if (!theWriter.Write(aBox, aPath.string().c_str(), theVersion))
  {
    return false;
  }
  std::string aText;
  {
    std::ifstream     anIn(aPath, std::ios::binary);
    std::stringstream aBuffer;
    aBuffer << anIn.rdbuf();
    aText = aBuffer.str();
  }
  std::filesystem::remove(aPath);
  const size_t aPos = aText.find("diffuseColor");
  if (aPos == std::string::npos)
  {
    return false;
  }
  std::string aValues = aText.substr(aPos + 12, 120);
  for (char& aChar : aValues)
  {
    aChar = (aChar == '[' || aChar == ']') ? ' ' : aChar;
  }
  std::istringstream aNumbers(aValues);
  return static_cast<bool>(aNumbers >> theRgb[0] >> theRgb[1] >> theRgb[2]);
}
} // namespace

// VRML 2.0 ignored the front material of the writer: the faces always had a golden color.
TEST(VrmlAPI_WriterTest, Version2_FrontMaterial)
{
  const Quantity_Color aBlue(0.1, 0.3, 0.9, Quantity_TOC_sRGB);
  for (const int aVersion : {1, 2})
  {
    VrmlAPI_Writer                                   aWriter;
    occ::handle<NCollection_HArray1<Quantity_Color>> aColors =
      new NCollection_HArray1<Quantity_Color>(1, 1, aBlue);
    aWriter.GetFrontMaterial()->SetDiffuseColor(aColors);
    double aRgb[3] = {-1.0, -1.0, -1.0};
    ASSERT_TRUE(writtenDiffuse(aWriter, aVersion, aRgb)) << aVersion;
    double aRed = 0.0, aGreen = 0.0, aBlueValue = 0.0;
    aBlue.Values(aRed, aGreen, aBlueValue, Quantity_TOC_sRGB);
    EXPECT_NEAR(aRgb[0], aRed, 1.0 / 255) << aVersion;
    EXPECT_NEAR(aRgb[1], aGreen, 1.0 / 255) << aVersion;
    EXPECT_NEAR(aRgb[2], aBlueValue, 1.0 / 255) << aVersion;
  }

  // the default front material is the same in both versions
  VrmlAPI_Writer aWriter;
  double         aRgb[3] = {-1.0, -1.0, -1.0};
  ASSERT_TRUE(writtenDiffuse(aWriter, 2, aRgb));
  EXPECT_NEAR(aRgb[0], 0.75, 1.0 / 255);
  EXPECT_NEAR(aRgb[1], 0.75, 1.0 / 255);
  EXPECT_NEAR(aRgb[2], 0.75, 1.0 / 255);
}
