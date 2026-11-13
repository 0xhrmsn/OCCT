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
#include <Interface_Check.hxx>
#include <Interface_CheckIterator.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_Reader.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <Transfer_TransientProcess.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ViewTool.hxx>
#include <XSControl_TransferReader.hxx>
#include <XSControl_WorkSession.hxx>

#include <cstring>
#include <sstream>
#include <string>

namespace
{
// Levels written for a too deep assembly and levels expected back from the reader.
constexpr int THE_DEPTH_TOO_DEEP = 150;
constexpr int THE_DEPTH_READ     = 100;

// STEP text of a box nested in theDepth assemblies, each holding the previous one.
std::string writeNestedAssembly(const int theDepth)
{
  occ::handle<TDocStd_Document> aDoc;
  XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aDoc);
  const occ::handle<XCAFDoc_ShapeTool> aShapeTool = XCAFDoc_DocumentTool::ShapeTool(aDoc->Main());
  TDF_Label aPrev = aShapeTool->AddShape(BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape(), false);
  for (int aLevel = 0; aLevel < theDepth; ++aLevel)
  {
    const TDF_Label anAssembly = aShapeTool->NewShape();
    aShapeTool->AddComponent(anAssembly, aPrev, TopLoc_Location());
    aPrev = anAssembly;
  }
  aShapeTool->UpdateAssemblies();

  STEPCAFControl_Writer aWriter;
  if (!aWriter.Transfer(aDoc))
  {
    return std::string();
  }
  std::ostringstream aStream;
  aWriter.ChangeWriter().WriteStream(aStream);
  return aStream.str();
}

// Number of fails of theReader's last transfer whose text contains theText.
int countFails(const XSControl_Reader& theReader, const char* theText)
{
  int aNb = 0;
  for (Interface_CheckIterator anIter =
         theReader.WS()->TransferReader()->TransientProcess()->CheckList(false);
       anIter.More();
       anIter.Next())
  {
    for (int aFail = 1; aFail <= anIter.Value()->NbFails(); ++aFail)
    {
      if (std::strstr(anIter.Value()->CFail(aFail), theText) != nullptr)
      {
        ++aNb;
      }
    }
  }
  return aNb;
}

// Number of assembly levels read from theText: components followed from the free shape down
// to the first shape that is not an assembly; -1 when the text gives no single free shape.
// theNbFails receives the number of fails about too deep assemblies.
int readAssemblyDepth(const std::string& theText, int& theNbFails)
{
  occ::handle<TDocStd_Document> aDoc;
  XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aDoc);
  STEPCAFControl_Reader aReader;
  std::istringstream    aStream(theText);
  if (aReader.ReadStream("nested.stp", aStream) != IFSelect_RetDone || !aReader.Transfer(aDoc))
  {
    return -1;
  }
  theNbFails = countFails(aReader.Reader(), "nested too deeply");
  const occ::handle<XCAFDoc_ShapeTool> aShapeTool = XCAFDoc_DocumentTool::ShapeTool(aDoc->Main());
  NCollection_Sequence<TDF_Label>      aFree;
  aShapeTool->GetFreeShapes(aFree);
  if (aFree.Length() != 1)
  {
    return -1;
  }
  int       aDepth = 0;
  TDF_Label aLabel = aFree.First();
  while (XCAFDoc_ShapeTool::IsAssembly(aLabel))
  {
    NCollection_Sequence<TDF_Label> aComponents;
    XCAFDoc_ShapeTool::GetComponents(aLabel, aComponents);
    if (aComponents.Length() != 1
        || !XCAFDoc_ShapeTool::GetReferredShape(aComponents.First(), aLabel))
    {
      break;
    }
    ++aDepth;
  }
  return aDepth;
}

// STEP text of a box whose shape representation is reached through a chain of theNbMaps
// SHAPE_REPRESENTATION -> MAPPED_ITEM -> REPRESENTATION_MAP; with isCyclic the last map
// points back to the head of the chain instead of the box. With isThroughRelationship each map
// leads to an empty representation related to the next level by a
// SHAPE_REPRESENTATION_RELATIONSHIP instead of to the next level itself.
std::string mappedItemChain(const int  theNbMaps,
                            const bool isCyclic,
                            const bool isThroughRelationship = false)
{
  std::string aText = writeNestedAssembly(0);
  // #3 = SHAPE_DEFINITION_REPRESENTATION(#4,#10); #10 = ADVANCED_BREP_SHAPE_REPRESENTATION(
  // '',(#11,#15),#345); with #11 its placement.
  const size_t aSdr = aText.find("SHAPE_DEFINITION_REPRESENTATION(");
  const size_t aRep = aText.find("ADVANCED_BREP_SHAPE_REPRESENTATION(");
  if (aSdr == std::string::npos || aRep == std::string::npos)
  {
    return std::string();
  }
  const size_t aRepId    = aText.rfind('#', aRep);
  const size_t aRepIdEnd = aText.find(' ', aRepId);
  const std::string aBrep = aText.substr(aRepId, aRepIdEnd - aRepId);
  const size_t      anAx  = aText.find("(#", aRep) + 1;
  const std::string anAxis = aText.substr(anAx, aText.find(',', anAx) - anAx);
  const size_t      aCtxEnd = aText.find(");", aRep);
  const size_t      aCtx    = aText.rfind('#', aCtxEnd);
  const std::string aContext = aText.substr(aCtx, aCtxEnd - aCtx);

  const int         aFirst = 1000000;
  const int         aStep  = 5;
  const std::string aHead  = "#" + std::to_string(aFirst);
  auto              anId   = [](const int theId) { return "#" + std::to_string(theId); };
  std::string       aChain;
  for (int aMap = 0; aMap < theNbMaps; ++aMap)
  {
    const int         aRepr = aFirst + aStep * aMap;
    const std::string aNext =
      aMap + 1 < theNbMaps ? anId(aRepr + aStep) : (isCyclic ? aHead : aBrep);
    const std::string aMapped = isThroughRelationship ? anId(aRepr + 3) : aNext;
    aChain += anId(aRepr) + " = SHAPE_REPRESENTATION('',(" + anId(aRepr + 1) + "),"
              + aContext + ");\n";
    aChain += anId(aRepr + 1) + " = MAPPED_ITEM(''," + anId(aRepr + 2) + "," + anAxis + ");\n";
    aChain += anId(aRepr + 2) + " = REPRESENTATION_MAP(" + anAxis + "," + aMapped + ");\n";
    if (isThroughRelationship)
    {
      aChain += anId(aRepr + 3) + " = SHAPE_REPRESENTATION('',(" + anAxis + "),"
                + aContext + ");\n";
      aChain += anId(aRepr + 4) + " = SHAPE_REPRESENTATION_RELATIONSHIP('',''," + anId(aRepr + 3)
                + "," + aNext + ");\n";
    }
  }
  // The product shape is the head of the chain.
  const size_t aUsed = aText.find(aBrep + ")", aSdr);
  if (aUsed == std::string::npos)
  {
    return std::string();
  }
  aText.replace(aUsed, aBrep.size(), aHead);
  aText.insert(aText.rfind("ENDSEC;"), aChain);
  return aText;
}

// Number of solids read from theText by STEPControl_Reader, -1 when the text is not read.
// theNbFails receives the number of fails about too deep mapped items.
int readSolids(const std::string& theText, int& theNbFails)
{
  STEPControl_Reader aReader;
  std::istringstream aStream(theText);
  if (aReader.ReadStream("mapped.stp", aStream) != IFSelect_RetDone)
  {
    return -1;
  }
  aReader.TransferRoots();
  theNbFails = countFails(aReader, "nested too deeply");
  int aNb    = 0;
  for (TopExp_Explorer anExp(aReader.OneShape(), TopAbs_SOLID); anExp.More(); anExp.Next())
  {
    ++aNb;
  }
  return aNb;
}

// STEP text of a box with a view showing the representation at the end of theNbLinks shape
// representation relationships (Rep1 of each one is the next representation); with isCyclic
// the relationships are a cycle of two.
std::string viewOfRelatedRepresentations(const int theNbLinks, const bool isCyclic)
{
  std::string  aText = writeNestedAssembly(0);
  const size_t aRep  = aText.find("ADVANCED_BREP_SHAPE_REPRESENTATION(");
  if (aRep == std::string::npos)
  {
    return std::string();
  }
  const size_t      anAx     = aText.find("(#", aRep) + 1;
  const std::string anAxis   = aText.substr(anAx, aText.find(',', anAx) - anAx);
  const size_t      aCtxEnd  = aText.find(");", aRep);
  const size_t      aCtx     = aText.rfind('#', aCtxEnd);
  const std::string aContext = aText.substr(aCtx, aCtxEnd - aCtx);

  // #F .. #F+theNbLinks: representations; then the relationships and the view entities.
  const int   aFirst = 1000000;
  auto        anId   = [](const int theId) { return "#" + std::to_string(theId); };
  std::string anAdded;
  for (int aRepr = 0; aRepr <= theNbLinks; ++aRepr)
  {
    anAdded += anId(aFirst + aRepr) + " = SHAPE_REPRESENTATION('',(" + anAxis + "),"
               + aContext + ");\n";
  }
  int aNext = aFirst + theNbLinks + 1;
  for (int aLink = 0; aLink < theNbLinks; ++aLink)
  {
    anAdded += anId(aNext++) + " = SHAPE_REPRESENTATION_RELATIONSHIP('','',"
               + anId(aFirst + aLink + 1) + "," + anId(aFirst + aLink) + ");\n";
  }
  if (isCyclic)
  {
    anAdded += anId(aNext++) + " = SHAPE_REPRESENTATION_RELATIONSHIP('',''," + anId(aFirst)
               + "," + anId(aFirst + 1) + ");\n";
  }
  const int aView = aNext;
  anAdded += anId(aView) + " = REPRESENTATION_MAP(" + anAxis + "," + anId(aFirst) + ");\n";
  anAdded += anId(aView + 1) + " = MAPPED_ITEM(''," + anId(aView) + "," + anAxis + ");\n";
  anAdded += anId(aView + 2) + " = CARTESIAN_POINT('',(0.,0.,10.));\n";
  anAdded += anId(aView + 3) + " = PLANAR_BOX('',10.,10.," + anAxis + ");\n";
  anAdded += anId(aView + 4) + " = VIEW_VOLUME(.PARALLEL.," + anId(aView + 2)
             + ",1.,0.,.F.,0.,.F.,.T.," + anId(aView + 3) + ");\n";
  anAdded += anId(aView + 5) + " = CAMERA_MODEL_D3('view'," + anAxis + "," + anId(aView + 4)
             + ");\n";
  anAdded += anId(aView + 6) + " = DRAUGHTING_MODEL('',(" + anId(aView + 5) + ","
             + anId(aView + 1) + ")," + aContext + ");\n";
  aText.insert(aText.rfind("ENDSEC;"), anAdded);
  return aText;
}

// Number of views read from theText, -1 when the text is not read or transferred.
int readViews(const std::string& theText)
{
  occ::handle<TDocStd_Document> aDoc;
  XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", aDoc);
  STEPCAFControl_Reader aReader;
  std::istringstream    aStream(theText);
  if (aReader.ReadStream("view.stp", aStream) != IFSelect_RetDone || !aReader.Transfer(aDoc))
  {
    return -1;
  }
  NCollection_Sequence<TDF_Label> aViews;
  XCAFDoc_DocumentTool::ViewTool(aDoc->Main())->GetViewLabels(aViews);
  return aViews.Length();
}
} // namespace

// Every level of an assembly went through the large stack frame of
// STEPControl_ActorWrite::TransferShape: the writer exhausted a 1 MB stack at 67 levels
// (now between 600 and 700).
TEST(STEPCAFControl_AssemblyNestingTest, NinetyLevels_WrittenAndReadBack)
{
  const std::string aText = writeNestedAssembly(90);
  ASSERT_FALSE(aText.empty());
  int aNbFails = 0;
  EXPECT_EQ(readAssemblyDepth(aText, aNbFails), 90);
  EXPECT_EQ(aNbFails, 0);
}

// The reader followed sub-assemblies without limit and exhausted a 1 MB stack at 600 levels.
// Levels deeper than the limit are now dropped with a fail.
TEST(STEPCAFControl_AssemblyNestingTest, TooDeep_DroppedWithFail)
{
  const std::string aText = writeNestedAssembly(THE_DEPTH_TOO_DEEP);
  ASSERT_FALSE(aText.empty());
  int aNbFails = 0;
  EXPECT_EQ(readAssemblyDepth(aText, aNbFails), THE_DEPTH_READ);
  EXPECT_EQ(aNbFails, 1);
}

// Shape representations reached through MAPPED_ITEM were followed without limit: a long chain
// exhausted the stack and a chain back to its head never ended.
TEST(STEPCAFControl_AssemblyNestingTest, MappedItemChain_ReadWithinLimit)
{
  const std::string aText = mappedItemChain(50, false);
  ASSERT_FALSE(aText.empty());
  int aNbFails = 0;
  EXPECT_EQ(readSolids(aText, aNbFails), 1);
  EXPECT_EQ(aNbFails, 0);
}

TEST(STEPCAFControl_AssemblyNestingTest, MappedItemChainTooDeep_DroppedWithFail)
{
  const std::string aText = mappedItemChain(1000, false);
  ASSERT_FALSE(aText.empty());
  int aNbFails = 0;
  EXPECT_EQ(readSolids(aText, aNbFails), 0);
  EXPECT_EQ(aNbFails, 1);
}

// The same chain going through shape representation relationships between the levels.
TEST(STEPCAFControl_AssemblyNestingTest, MappedItemRelationshipChain_ReadWithinLimit)
{
  const std::string aText = mappedItemChain(50, false, true);
  ASSERT_FALSE(aText.empty());
  int aNbFails = 0;
  EXPECT_EQ(readSolids(aText, aNbFails), 1);
  EXPECT_EQ(aNbFails, 0);
}

TEST(STEPCAFControl_AssemblyNestingTest, MappedItemRelationshipChainTooDeep_DroppedWithFail)
{
  const std::string aText = mappedItemChain(1000, false, true);
  ASSERT_FALSE(aText.empty());
  int aNbFails = 0;
  EXPECT_EQ(readSolids(aText, aNbFails), 0);
  EXPECT_GE(aNbFails, 1);
}

TEST(STEPCAFControl_AssemblyNestingTest, MappedItemRelationshipCycle_DroppedWithFail)
{
  const std::string aText = mappedItemChain(3, true, true);
  ASSERT_FALSE(aText.empty());
  int aNbFails = 0;
  EXPECT_EQ(readSolids(aText, aNbFails), 0);
  EXPECT_GE(aNbFails, 1);
}

TEST(STEPCAFControl_AssemblyNestingTest, MappedItemCycle_DroppedWithFail)
{
  const std::string aText = mappedItemChain(3, true);
  ASSERT_FALSE(aText.empty());
  int aNbFails = 0;
  EXPECT_EQ(readSolids(aText, aNbFails), 0);
  EXPECT_EQ(aNbFails, 1);
}

// The shapes of a view were collected by following representation relationships recursively,
// without remembering the representations already visited: two relationships forming a cycle
// recursed without end, and a long chain of them exhausted the stack.
TEST(STEPCAFControl_AssemblyNestingTest, ViewOfCyclicRelationships_Read)
{
  const std::string aText = viewOfRelatedRepresentations(1, true);
  ASSERT_FALSE(aText.empty());
  EXPECT_EQ(readViews(aText), 1);
}

TEST(STEPCAFControl_AssemblyNestingTest, ViewOfLongRelationshipChain_Read)
{
  const std::string aText = viewOfRelatedRepresentations(20000, false);
  ASSERT_FALSE(aText.empty());
  EXPECT_EQ(readViews(aText), 1);
}
