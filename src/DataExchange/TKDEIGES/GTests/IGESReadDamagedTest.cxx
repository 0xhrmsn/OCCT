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

#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <Interface_CheckIterator.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <XSControl_TransferReader.hxx>
#include <XSControl_WorkSession.hxx>

#include <gtest/gtest.h>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
//! Reads an IGES file and transfers its roots; returns the number of fails of the reading of the
//! entities and of the transfer. The global check of the reading is not counted: its messages
//! (e.g. syntax errors of the sections) are only given by the first reading of a process (known
//! defect of IGESFile_Read, the same in the original code).
int readAndTransfer(const std::string& thePath)
{
  IGESControl_Reader aReader;
  aReader.ReadFile(thePath.c_str());
  aReader.TransferRoots();
  int aNbFails = 0;
  for (const Interface_CheckIterator& aChecks :
       {aReader.WS()->ModelCheckList(), aReader.WS()->TransferReader()->LastCheckList()})
  {
    for (aChecks.Start(); aChecks.More(); aChecks.Next())
    {
      aNbFails += aChecks.Number() > 0 ? aChecks.Value()->NbFails() : 0;
    }
  }
  return aNbFails;
}

//! Formats a fixed-format IGES line: 72 columns of data, the section letter, the sequence number.
std::string igesLine(const std::string& theData, const char theSection, const int theNumber)
{
  char aLine[96];
  std::snprintf(aLine, sizeof(aLine), "%-72.72s%c%7d", theData.c_str(), theSection, theNumber);
  return aLine;
}

//! Directory entry of an entity (two lines).
std::vector<std::string> directoryEntry(const int theType,
                                        const int theParam,
                                        const int theForm,
                                        const int theNumber)
{
  char aData[96];
  std::snprintf(aData,
                sizeof(aData),
                "%8d%8d%8d%8d%8d%8d%8d%8d%8s",
                theType,
                theParam,
                0,
                0,
                0,
                0,
                0,
                0,
                "00000000");
  std::vector<std::string> aLines{igesLine(aData, 'D', theNumber)};
  std::snprintf(aData,
                sizeof(aData),
                "%8d%8d%8d%8d%8d%8s%8s%8s%8d",
                theType,
                0,
                0,
                1,
                theForm,
                "",
                "",
                "",
                0);
  aLines.push_back(igesLine(aData, 'D', theNumber + 1));
  return aLines;
}

//! Parameter data line of the entity whose directory entry is theDE.
std::string parameterLine(const std::string& theParams, const int theDE, const int theNumber)
{
  char aData[96];
  std::snprintf(aData, sizeof(aData), "%-64.64s %7d", theParams.c_str(), theDE);
  return igesLine(aData, 'P', theNumber);
}

//! Path of a temporary file or folder, distinct for each call (tests may run in parallel).
std::filesystem::path uniqueTempPath(const std::string& theName)
{
  static std::random_device aDevice;
  return std::filesystem::temp_directory_path()
         / (theName + "_" + std::to_string(aDevice()) + "_" + std::to_string(aDevice()));
}

//! Lines of theShape written in IGES BRep mode.
std::vector<std::string> brepModeLines(const TopoDS_Shape& theShape)
{
  IGESControl_Writer aWriter("MM", 1);
  aWriter.AddShape(theShape);
  aWriter.ComputeModel();
  std::ostringstream anOut;
  aWriter.Write(anOut);
  std::vector<std::string> aLines;
  std::istringstream       anIn(anOut.str());
  for (std::string aLine; std::getline(anIn, aLine);)
  {
    aLines.push_back(aLine);
  }
  return aLines;
}

//! Replaces in theLines the first parameter (a count) of the entities of type theType by letters:
//! the reading of these entities fails. Returns the number of entities changed.
int breakEntities(std::vector<std::string>& theLines, const std::string& theType)
{
  int               aNbChanged = 0;
  const std::string aPrefix    = theType + ",";
  for (std::string& aLine : theLines)
  {
    if (aLine.size() < 73 || aLine[72] != 'P' || aLine.compare(0, aPrefix.size(), aPrefix) != 0)
    {
      continue;
    }
    for (size_t i = aPrefix.size(); i < 64 && aLine[i] != ',' && aLine[i] != ';'; ++i)
    {
      aLine[i] = 'X';
    }
    ++aNbChanged;
  }
  return aNbChanged;
}
} // namespace

// A solid written in BRep mode, with directory entries removed: the faces, loops and lists that
// refer to the missing entities failed to be read. Their lists were unset and the transfer
// dereferenced them (access violation without a signal handler), and values that were not read
// changed from one reading to the next.
TEST(IGESReadDamagedTest, BRepSolidWithMissingEntities_NoCrash)
{
  IGESControl_Controller::Init();
  const std::filesystem::path aDir = uniqueTempPath("IGESReadDamagedTest_BRepSolid");
  std::filesystem::create_directories(aDir);
  const std::string aSource = (aDir / "source.igs").string();
  {
    const TopoDS_Shape       aBox = BRepPrimAPI_MakeBox(40.0, 30.0, 20.0).Shape();
    BRepFilletAPI_MakeFillet aFillet(aBox);
    for (TopExp_Explorer anExp(aBox, TopAbs_EDGE); anExp.More(); anExp.Next())
    {
      aFillet.Add(3.0, TopoDS::Edge(anExp.Current()));
    }
    IGESControl_Writer aWriter("MM", 1);
    aWriter.AddShape(aFillet.Shape());
    aWriter.ComputeModel();
    ASSERT_TRUE(aWriter.Write(aSource.c_str()));
  }

  std::vector<std::string> aLines;
  {
    std::ifstream aFile(aSource);
    for (std::string aLine; std::getline(aFile, aLine);)
    {
      aLines.push_back(aLine);
    }
  }
  std::vector<size_t> aDirLines;
  for (size_t i = 0; i < aLines.size(); ++i)
  {
    if (aLines[i].size() >= 73 && aLines[i][72] == 'D')
    {
      aDirLines.push_back(i);
    }
  }
  ASSERT_GT(aDirLines.size(), 60u);

  const std::string aDamaged = (aDir / "damaged.igs").string();
  for (size_t anOffset = 0; anOffset + 30 < aDirLines.size(); anOffset += 6)
  {
    {
      std::ofstream aFile(aDamaged);
      for (size_t i = 0; i < aLines.size(); ++i)
      {
        if (i < aDirLines[anOffset] || i >= aDirLines[anOffset] + 30)
        {
          aFile << aLines[i] << "\n";
        }
      }
    }
    int aNbFails[2] = {0, 0};
    for (int aRun = 0; aRun < 2; ++aRun)
    {
      EXPECT_NO_THROW(aNbFails[aRun] = readAndTransfer(aDamaged)) << anOffset;
    }
    EXPECT_EQ(aNbFails[0], aNbFails[1]) << "same report on a second reading, offset " << anOffset;
  }
  std::error_code anError;
  std::filesystem::remove_all(aDir, anError);
}

// Two groups (402) or composite curves (102) that contain each other: the transfer recursed until
// the stack overflowed. The cycle is reported and the other entities are transferred.
TEST(IGESReadDamagedTest, EntitiesContainingEachOther_Reported)
{
  IGESControl_Controller::Init();
  for (const int aType : {402, 102})
  {
    const int                aForm = aType == 402 ? 1 : 0;
    const std::string        aT    = std::to_string(aType);
    std::vector<std::string> aLines{igesLine("Entities containing each other", 'S', 1)};
    const char* aGlobal[] = {"1H,,1H;,4Htest,8Htest.igs,4Htest,4Htest,32,38,6,308,15,4Htest,1.,2,",
                             "2HMM,1,0.01,15H20261110.120000,1.E-07,100.,4Htest,4Htest,11,0;"};
    aLines.push_back(igesLine(aGlobal[0], 'G', 1));
    aLines.push_back(igesLine(aGlobal[1], 'G', 2));
    // DE 1: line, DE 3: entity with the line and the entity DE 5, DE 5: entity with the entity
    // DE 3, DE 7: entity with the entity DE 3 (the root: the other entities are referenced)
    for (const std::string& aLine : directoryEntry(110, 1, 0, 1))
    {
      aLines.push_back(aLine);
    }
    for (const std::string& aLine : directoryEntry(aType, 2, aForm, 3))
    {
      aLines.push_back(aLine);
    }
    for (const std::string& aLine : directoryEntry(aType, 3, aForm, 5))
    {
      aLines.push_back(aLine);
    }
    for (const std::string& aLine : directoryEntry(aType, 4, aForm, 7))
    {
      aLines.push_back(aLine);
    }
    aLines.push_back(parameterLine("110,0.,0.,0.,10.,0.,0.;", 1, 1));
    aLines.push_back(parameterLine(aT + ",2,1,5;", 3, 2));
    aLines.push_back(parameterLine(aT + ",1,3;", 5, 3));
    aLines.push_back(parameterLine(aT + ",1,3;", 7, 4));
    aLines.push_back(igesLine("S      1G      2D      8P      4", 'T', 1));

    const std::filesystem::path aPath = uniqueTempPath("IGESReadDamagedTest_Cycle");
    {
      std::ofstream aFile(aPath);
      for (const std::string& aLine : aLines)
      {
        aFile << aLine << "\n";
      }
    }

    IGESControl_Reader aReader;
    const bool         isRead = aReader.ReadFile(aPath.string().c_str()) == IFSelect_RetDone;
    std::error_code    anError;
    std::filesystem::remove(aPath, anError);
    ASSERT_TRUE(isRead) << aType;
    EXPECT_NO_THROW(aReader.TransferRoots()) << aType;
    int                           aNbFails = 0;
    const Interface_CheckIterator aChecks  = aReader.WS()->TransferReader()->LastCheckList();
    for (aChecks.Start(); aChecks.More(); aChecks.Next())
    {
      aNbFails += aChecks.Value()->NbFails();
    }
    EXPECT_GT(aNbFails, 0) << aType;
    if (aType == 402)
    {
      // the line of the group is transferred (the composite curve is a single wire: its cycle
      // stops the whole curve)
      bool hasEdge = false;
      for (int i = 1; i <= aReader.NbShapes(); ++i)
      {
        hasEdge = hasEdge || TopExp_Explorer(aReader.Shape(i), TopAbs_EDGE).More();
      }
      EXPECT_TRUE(hasEdge);
    }
  }
}

// Solids written in BRep mode whose vertex lists (502) or shells (514) failed to be read. These
// entities are not translated, and the edges, loops and solids that use them got null vertices
// or shells: a null shape added with BRep_Builder crashed (access violation).
TEST(IGESReadDamagedTest, BrokenVertexListsAndShells_NoCrash)
{
  IGESControl_Controller::Init();
  const TopoDS_Shape aSphere = BRepPrimAPI_MakeSphere(10.0).Shape();
  const TopoDS_Shape aHollow =
    BRepAlgoAPI_Cut(BRepPrimAPI_MakeBox(40.0, 30.0, 20.0).Shape(),
                    BRepPrimAPI_MakeBox(gp_Pnt(10.0, 10.0, 5.0), 20.0, 10.0, 10.0).Shape())
      .Shape();
  int aCase = 0;
  for (const TopoDS_Shape& aShape : {aSphere, aHollow})
  {
    for (const char* aType : {"502", "514"})
    {
      ++aCase;
      std::vector<std::string> aLines = brepModeLines(aShape);
      ASSERT_GT(breakEntities(aLines, aType), 0) << aCase;
      const std::filesystem::path aPath = uniqueTempPath("IGESReadDamagedTest_Broken");
      {
        std::ofstream aFile(aPath);
        for (const std::string& aLine : aLines)
        {
          aFile << aLine << "\n";
        }
      }
      int aNbFails = 0;
      EXPECT_NO_THROW(aNbFails = readAndTransfer(aPath.string())) << aCase;
      EXPECT_GT(aNbFails, 0) << aCase;
      std::error_code anError;
      std::filesystem::remove(aPath, anError);
    }
  }
}

// A chain of groups, each containing the next one, the last one a line. A chain within the nesting
// limit is transferred (on the stack of a release build); a longer one is reported as failed, not
// recursed until the stack overflows.
TEST(IGESReadDamagedTest, NestedGroups_LimitedDepth)
{
  IGESControl_Controller::Init();
  for (const int aNbGroups : {90, 150})
  {
    std::vector<std::string> aLines{igesLine("Nested groups", 'S', 1)};
    const char* aGlobal[] = {"1H,,1H;,4Htest,8Htest.igs,4Htest,4Htest,32,38,6,308,15,4Htest,1.,2,",
                             "2HMM,1,0.01,15H20261110.120000,1.E-07,100.,4Htest,4Htest,11,0;"};
    aLines.push_back(igesLine(aGlobal[0], 'G', 1));
    aLines.push_back(igesLine(aGlobal[1], 'G', 2));
    // DE 1: line; DE 2k+1 (k = 1..aNbGroups): group with the entity DE 2k-1
    for (const std::string& aLine : directoryEntry(110, 1, 0, 1))
    {
      aLines.push_back(aLine);
    }
    for (int k = 1; k <= aNbGroups; ++k)
    {
      for (const std::string& aLine : directoryEntry(402, k + 1, 1, 2 * k + 1))
      {
        aLines.push_back(aLine);
      }
    }
    aLines.push_back(parameterLine("110,0.,0.,0.,10.,0.,0.;", 1, 1));
    for (int k = 1; k <= aNbGroups; ++k)
    {
      aLines.push_back(parameterLine("402,1," + std::to_string(2 * k - 1) + ";", 2 * k + 1, k + 1));
    }
    char aTerminate[64];
    std::snprintf(aTerminate,
                  sizeof(aTerminate),
                  "S%7dG%7dD%7dP%7d",
                  1,
                  2,
                  2 * (aNbGroups + 1),
                  aNbGroups + 1);
    aLines.push_back(igesLine(aTerminate, 'T', 1));

    const std::filesystem::path aPath = uniqueTempPath("IGESReadDamagedTest_Nested");
    {
      std::ofstream aFile(aPath);
      for (const std::string& aLine : aLines)
      {
        aFile << aLine << "\n";
      }
    }
    IGESControl_Reader aReader;
    const bool         isRead = aReader.ReadFile(aPath.string().c_str()) == IFSelect_RetDone;
    std::error_code    anError;
    std::filesystem::remove(aPath, anError);
    ASSERT_TRUE(isRead) << aNbGroups;
    ASSERT_EQ(aReader.NbRootsForTransfer(), 1) << aNbGroups;
    EXPECT_NO_THROW(aReader.TransferRoots()) << aNbGroups;
    bool hasEdge = false;
    for (int i = 1; i <= aReader.NbShapes(); ++i)
    {
      hasEdge = hasEdge || TopExp_Explorer(aReader.Shape(i), TopAbs_EDGE).More();
    }
    int                           aNbFails = 0;
    const Interface_CheckIterator aChecks  = aReader.WS()->TransferReader()->LastCheckList();
    for (aChecks.Start(); aChecks.More(); aChecks.Next())
    {
      aNbFails += aChecks.Value()->NbFails();
    }
    if (aNbGroups < 100)
    {
      EXPECT_TRUE(hasEdge);
      EXPECT_EQ(aNbFails, 0);
    }
    else
    {
      EXPECT_FALSE(hasEdge);
      EXPECT_GT(aNbFails, 0);
    }
  }
}
