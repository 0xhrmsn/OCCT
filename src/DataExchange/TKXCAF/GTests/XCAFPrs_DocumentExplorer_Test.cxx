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

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <RWMesh_FaceIterator.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_GraphNode.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFPrs_DocumentExplorer.hxx>
#include <gp_Trsf.hxx>

#include <vector>

namespace
{

TopLoc_Location translation(const double theX, const double theY)
{
  gp_Trsf aTrsf;
  aTrsf.SetTranslation(gp_Vec(theX, theY, 0.0));
  return TopLoc_Location(aTrsf);
}

bool isRed(const Quantity_Color& theColor)
{
  return theColor.Red() > 0.99 && theColor.Green() < 0.01 && theColor.Blue() < 0.01;
}

//! Assembly "Top" with two instances of "Sub", each with two instances of a grey part:
//! four leaves. theSHUOPath selects the components of the occurrence colored in red by a SHUO
//! (none if empty): 0/1 = first/second instance of Sub in Top, 2/3 = first/second part in Sub.
occ::handle<TDocStd_Document> makeDocument(const std::vector<int>& theSHUOPath)
{
  occ::handle<TDocStd_Document> aDoc;
  XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aDoc);
  const occ::handle<XCAFDoc_ShapeTool> aShapeTool = XCAFDoc_DocumentTool::ShapeTool(aDoc->Main());
  const occ::handle<XCAFDoc_ColorTool> aColorTool = XCAFDoc_DocumentTool::ColorTool(aDoc->Main());
  const TDF_Label                      aPart =
    aShapeTool->AddShape(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape(), false);
  aColorTool->SetColor(aPart, Quantity_Color(0.5, 0.5, 0.5, Quantity_TOC_RGB), XCAFDoc_ColorSurf);
  const TDF_Label aSub = aShapeTool->NewShape();
  TDF_Label       aComps[4];
  aComps[2]            = aShapeTool->AddComponent(aSub, aPart, translation(0.0, 0.0));
  aComps[3]            = aShapeTool->AddComponent(aSub, aPart, translation(20.0, 0.0));
  const TDF_Label aTop = aShapeTool->NewShape();
  aComps[0]            = aShapeTool->AddComponent(aTop, aSub, translation(0.0, 0.0));
  aComps[1]            = aShapeTool->AddComponent(aTop, aSub, translation(0.0, 30.0));
  aShapeTool->UpdateAssemblies();
  if (!theSHUOPath.empty())
  {
    NCollection_Sequence<TDF_Label> aPath;
    for (const int anIndex : theSHUOPath)
    {
      aPath.Append(aComps[anIndex]);
    }
    occ::handle<XCAFDoc_GraphNode> aSHUO;
    EXPECT_TRUE(aShapeTool->SetSHUO(aPath, aSHUO));
    aColorTool->SetColor(aSHUO->Label(),
                         Quantity_Color(1.0, 0.0, 0.0, Quantity_TOC_RGB),
                         XCAFDoc_ColorSurf);
  }
  return aDoc;
}

//! Translations of the red leaves.
std::vector<gp_XYZ> redLeaves(const occ::handle<TDocStd_Document>& theDoc)
{
  std::vector<gp_XYZ> aRed;
  int                 aNbLeaves = 0;
  for (XCAFPrs_DocumentExplorer anExp(theDoc, XCAFPrs_DocumentExplorerFlags_OnlyLeafNodes);
       anExp.More();
       anExp.Next())
  {
    ++aNbLeaves;
    const XCAFPrs_Style& aStyle = anExp.Current().Style;
    if (aStyle.IsSetColorSurf() && isRed(aStyle.GetColorSurf()))
    {
      aRed.push_back(anExp.Current().Location.Transformation().TranslationPart());
    }
  }
  EXPECT_EQ(aNbLeaves, 4);
  return aRed;
}

} // namespace

// A SHUO colors one occurrence of a part (second Sub in Top, first part in Sub): the mesh
// writers (glTF, OBJ, PLY) take the styles of the leaves from the explorer.
TEST(XCAFPrs_DocumentExplorerTest, SHUOColor_AppliedToOccurrence)
{
  const std::vector<gp_XYZ> aRed = redLeaves(makeDocument({1, 2}));
  ASSERT_EQ(aRed.size(), 1u);
  EXPECT_NEAR((aRed.front() - gp_XYZ(0.0, 30.0, 0.0)).Modulus(), 0.0, 1.0e-12);
}

// A SHUO on an occurrence of a sub-assembly colors the leaf below it (Top/second Mid/first Sub).
TEST(XCAFPrs_DocumentExplorerTest, SHUOOnSubAssembly_AppliedToItsLeaves)
{
  occ::handle<TDocStd_Document> aDoc;
  XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aDoc);
  const occ::handle<XCAFDoc_ShapeTool> aShapeTool = XCAFDoc_DocumentTool::ShapeTool(aDoc->Main());
  const occ::handle<XCAFDoc_ColorTool> aColorTool = XCAFDoc_DocumentTool::ColorTool(aDoc->Main());
  const TDF_Label                      aPart =
    aShapeTool->AddShape(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape(), false);
  const TDF_Label aSub = aShapeTool->NewShape();
  aShapeTool->AddComponent(aSub, aPart, translation(0.0, 0.0));
  const TDF_Label aMid     = aShapeTool->NewShape();
  const TDF_Label aMidComp = aShapeTool->AddComponent(aMid, aSub, translation(0.0, 0.0));
  aShapeTool->AddComponent(aMid, aSub, translation(20.0, 0.0));
  const TDF_Label aTop = aShapeTool->NewShape();
  aShapeTool->AddComponent(aTop, aMid, translation(0.0, 0.0));
  const TDF_Label aTopComp = aShapeTool->AddComponent(aTop, aMid, translation(0.0, 30.0));
  aShapeTool->UpdateAssemblies();
  NCollection_Sequence<TDF_Label> aPath;
  aPath.Append(aTopComp);
  aPath.Append(aMidComp);
  occ::handle<XCAFDoc_GraphNode> aSHUO;
  ASSERT_TRUE(aShapeTool->SetSHUO(aPath, aSHUO));
  aColorTool->SetColor(aSHUO->Label(),
                       Quantity_Color(1.0, 0.0, 0.0, Quantity_TOC_RGB),
                       XCAFDoc_ColorSurf);

  int aNbRed = 0;
  for (XCAFPrs_DocumentExplorer anExp(aDoc, XCAFPrs_DocumentExplorerFlags_OnlyLeafNodes);
       anExp.More();
       anExp.Next())
  {
    const XCAFPrs_Style& aStyle = anExp.Current().Style;
    if (aStyle.IsSetColorSurf() && isRed(aStyle.GetColorSurf()))
    {
      ++aNbRed;
      EXPECT_NEAR(
        (anExp.Current().Location.Transformation().TranslationPart() - gp_XYZ(0.0, 30.0, 0.0))
          .Modulus(),
        0.0,
        1.0e-12);
    }
  }
  EXPECT_EQ(aNbRed, 1);
}

// Guard: without SHUO no leaf is red, and the explorer without styles ignores the SHUO.
TEST(XCAFPrs_DocumentExplorerTest, NoSHUO_NoOverride)
{
  EXPECT_TRUE(redLeaves(makeDocument({})).empty());
  const occ::handle<TDocStd_Document> aDoc   = makeDocument({1, 2});
  int                                 aNbRed = 0;
  for (XCAFPrs_DocumentExplorer anExp(aDoc,
                                      XCAFPrs_DocumentExplorerFlags_OnlyLeafNodes
                                        | XCAFPrs_DocumentExplorerFlags_NoStyle);
       anExp.More();
       anExp.Next())
  {
    const XCAFPrs_Style& aStyle = anExp.Current().Style;
    aNbRed += aStyle.IsSetColorSurf() && isRed(aStyle.GetColorSurf()) ? 1 : 0;
  }
  EXPECT_EQ(aNbRed, 0);
}

namespace
{

//! Numbers of red faces and of faces seen by the mesh writers, which iterate each leaf of the
//! explorer with RWMesh_FaceIterator and the style of the leaf.
void countWriterFaces(const occ::handle<TDocStd_Document>& theDoc, int& theNbFaces, int& theNbRed)
{
  theNbFaces = 0;
  theNbRed   = 0;
  for (XCAFPrs_DocumentExplorer anExp(theDoc, XCAFPrs_DocumentExplorerFlags_OnlyLeafNodes);
       anExp.More();
       anExp.Next())
  {
    const XCAFPrs_DocumentNode& aNode = anExp.Current();
    BRepMesh_IncrementalMesh    aMesher(XCAFDoc_ShapeTool::GetShape(aNode.RefLabel), 1.0);
    for (RWMesh_FaceIterator aFaceIter(aNode.RefLabel, aNode.Location, true, aNode.Style);
         aFaceIter.More();
         aFaceIter.Next())
    {
      ++theNbFaces;
      theNbRed += aFaceIter.HasFaceColor() && isRed(aFaceIter.FaceColor().GetRGB()) ? 1 : 0;
    }
  }
}

} // namespace

// The color of the SHUO reaches the mesh writers even if the part has its own color.
TEST(XCAFPrs_DocumentExplorerTest, SHUOColor_ReachesMeshWriters)
{
  int aNbFaces = 0, aNbRed = 0;
  countWriterFaces(makeDocument({1, 2}), aNbFaces, aNbRed);
  EXPECT_EQ(aNbFaces, 24);
  EXPECT_EQ(aNbRed, 6);
}

// Guard: without SHUO the mesh writers keep the color of the part.
TEST(XCAFPrs_DocumentExplorerTest, NoSHUO_MeshWritersKeepPartColor)
{
  int aNbFaces = 0, aNbRed = 0;
  countWriterFaces(makeDocument({}), aNbFaces, aNbRed);
  EXPECT_EQ(aNbFaces, 24);
  EXPECT_EQ(aNbRed, 0);
}
