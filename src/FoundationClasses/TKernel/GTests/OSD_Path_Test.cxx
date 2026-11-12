// Copyright (c) 2025 OPEN CASCADE SAS
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

#include <OSD_Directory.hxx>
#include <OSD_DirectoryIterator.hxx>
#include <OSD_File.hxx>
#include <OSD_FileIterator.hxx>
#include <OSD_Path.hxx>
#include <OSD_Process.hxx>
#include <TCollection_AsciiString.hxx>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

// Test fixture for OSD_Path tests
class OSD_PathTest : public testing::Test
{
protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(OSD_PathTest, DosAbsolutePaths)
{
  // Test DOS absolute paths
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("c:\\folder\\file.png"));
  EXPECT_TRUE(OSD_Path::IsDosPath("c:\\folder\\file.png"));
  EXPECT_FALSE(OSD_Path::IsRelativePath("c:\\folder\\file.png"));
  EXPECT_FALSE(OSD_Path::IsUnixPath("c:\\folder\\file.png"));
  EXPECT_FALSE(OSD_Path::IsUncPath("c:\\folder\\file.png"));

  // Test DOS root
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("D:\\"));
  EXPECT_TRUE(OSD_Path::IsDosPath("D:\\"));

  // Test single file on DOS drive
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("c:\\file.png"));
  EXPECT_TRUE(OSD_Path::IsDosPath("c:\\file.png"));
}

TEST_F(OSD_PathTest, UncPaths)
{
  // Test UNC paths
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("\\\\share\\file.pdf"));
  EXPECT_TRUE(OSD_Path::IsUncPath("\\\\share\\file.pdf"));
  EXPECT_FALSE(OSD_Path::IsDosPath("\\\\share\\file.pdf"));
  EXPECT_FALSE(OSD_Path::IsUnixPath("\\\\share\\file.pdf"));
}

TEST_F(OSD_PathTest, NtExtendedPaths)
{
  // Test NT Extended paths
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("\\\\?\\C:\\documents\\file.docx"));
  EXPECT_TRUE(OSD_Path::IsNtExtendedPath("\\\\?\\C:\\documents\\file.docx"));
  EXPECT_FALSE(OSD_Path::IsUncPath("\\\\?\\C:\\documents\\file.docx"));
}

TEST_F(OSD_PathTest, UncExtendedPaths)
{
  // Test UNC Extended paths
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("\\\\?\\UNC\\server\\share\\file.zip"));
  EXPECT_TRUE(OSD_Path::IsUncPath("\\\\?\\UNC\\server\\share\\file.zip"));
  EXPECT_TRUE(OSD_Path::IsNtExtendedPath("\\\\?\\UNC\\server\\share\\file.zip"));
  EXPECT_TRUE(OSD_Path::IsUncExtendedPath("\\\\?\\UNC\\server\\share\\file.zip"));
}

TEST_F(OSD_PathTest, RemoteProtocolPaths)
{
  // Test remote protocol paths
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("https://www.server.org/file.gif"));
  EXPECT_TRUE(OSD_Path::IsRemoteProtocolPath("https://www.server.org/file.gif"));
  EXPECT_FALSE(OSD_Path::IsUnixPath("https://www.server.org/file.gif"));
  EXPECT_FALSE(OSD_Path::IsDosPath("https://www.server.org/file.gif"));

  // Test other protocols
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("ftp://ftp.server.com/file.dat"));
  EXPECT_TRUE(OSD_Path::IsRemoteProtocolPath("ftp://ftp.server.com/file.dat"));

  EXPECT_TRUE(OSD_Path::IsAbsolutePath("http://example.com/path"));
  EXPECT_TRUE(OSD_Path::IsRemoteProtocolPath("http://example.com/path"));
}

TEST_F(OSD_PathTest, ContentProtocolPaths)
{
  // Test content protocol paths
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("content://file.jpg"));
  EXPECT_TRUE(OSD_Path::IsRemoteProtocolPath("content://file.jpg"));
  EXPECT_TRUE(OSD_Path::IsContentProtocolPath("content://file.jpg"));
}

TEST_F(OSD_PathTest, UnixAbsolutePaths)
{
  // Test Unix absolute paths
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("/home/username/file.txt"));
  EXPECT_TRUE(OSD_Path::IsUnixPath("/home/username/file.txt"));
  EXPECT_FALSE(OSD_Path::IsRelativePath("/home/username/file.txt"));
  EXPECT_FALSE(OSD_Path::IsDosPath("/home/username/file.txt"));

  // Test Unix root
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("/"));
  EXPECT_TRUE(OSD_Path::IsUnixPath("/"));

  // Test single file on Unix
  EXPECT_TRUE(OSD_Path::IsAbsolutePath("/boot.bin"));
  EXPECT_TRUE(OSD_Path::IsUnixPath("/boot.bin"));
}

TEST_F(OSD_PathTest, RelativePaths)
{
  // Test relative paths with navigation
  EXPECT_TRUE(OSD_Path::IsRelativePath("./subfolder/../file.txt"));
  EXPECT_FALSE(OSD_Path::IsAbsolutePath("./subfolder/../file.txt"));

  EXPECT_TRUE(OSD_Path::IsRelativePath("../file.txt"));
  EXPECT_FALSE(OSD_Path::IsAbsolutePath("../file.txt"));

  // Test current directory
  EXPECT_TRUE(OSD_Path::IsRelativePath("."));
  EXPECT_FALSE(OSD_Path::IsAbsolutePath("."));

  // Test parent directory
  EXPECT_TRUE(OSD_Path::IsRelativePath(".."));
  EXPECT_FALSE(OSD_Path::IsAbsolutePath(".."));

  // Test simple relative path
  EXPECT_TRUE(OSD_Path::IsRelativePath("folder/file.txt"));
  EXPECT_FALSE(OSD_Path::IsAbsolutePath("folder/file.txt"));

  // Test simple filename
  EXPECT_TRUE(OSD_Path::IsRelativePath("file.txt"));
  EXPECT_FALSE(OSD_Path::IsAbsolutePath("file.txt"));
}

TEST_F(OSD_PathTest, FolderAndFileFromPath_UnixPaths)
{
  TCollection_AsciiString aFolder, aFileName;

  // Test Unix absolute path
  OSD_Path::FolderAndFileFromPath("/home/username/file.txt", aFolder, aFileName);
  EXPECT_STREQ("/home/username/", aFolder.ToCString());
  EXPECT_STREQ("file.txt", aFileName.ToCString());

  // Test Unix root file
  OSD_Path::FolderAndFileFromPath("/file.txt", aFolder, aFileName);
  EXPECT_STREQ("/", aFolder.ToCString());
  EXPECT_STREQ("file.txt", aFileName.ToCString());

  // Test Unix folder only (trailing slash)
  OSD_Path::FolderAndFileFromPath("/home/username/", aFolder, aFileName);
  EXPECT_STREQ("/home/username/", aFolder.ToCString());
  EXPECT_STREQ("", aFileName.ToCString());
}

TEST_F(OSD_PathTest, FolderAndFileFromPath_DosPaths)
{
  TCollection_AsciiString aFolder, aFileName;

  // Test DOS absolute path
  OSD_Path::FolderAndFileFromPath("C:\\Users\\John\\document.txt", aFolder, aFileName);
  EXPECT_STREQ("C:\\Users\\John\\", aFolder.ToCString());
  EXPECT_STREQ("document.txt", aFileName.ToCString());

  // Test DOS root file
  OSD_Path::FolderAndFileFromPath("C:\\file.txt", aFolder, aFileName);
  EXPECT_STREQ("C:\\", aFolder.ToCString());
  EXPECT_STREQ("file.txt", aFileName.ToCString());

  // Test DOS folder only (trailing backslash)
  OSD_Path::FolderAndFileFromPath("C:\\Program Files\\", aFolder, aFileName);
  EXPECT_STREQ("C:\\Program Files\\", aFolder.ToCString());
  EXPECT_STREQ("", aFileName.ToCString());
}

TEST_F(OSD_PathTest, FolderAndFileFromPath_RelativePaths)
{
  TCollection_AsciiString aFolder, aFileName;

  // Test relative path
  OSD_Path::FolderAndFileFromPath("folder/subfolder/file.txt", aFolder, aFileName);
  EXPECT_STREQ("folder/subfolder/", aFolder.ToCString());
  EXPECT_STREQ("file.txt", aFileName.ToCString());

  // Test relative with navigation
  OSD_Path::FolderAndFileFromPath("../folder/file.txt", aFolder, aFileName);
  EXPECT_STREQ("../folder/", aFolder.ToCString());
  EXPECT_STREQ("file.txt", aFileName.ToCString());

  // Test just filename
  OSD_Path::FolderAndFileFromPath("file.txt", aFolder, aFileName);
  EXPECT_STREQ("", aFolder.ToCString());
  EXPECT_STREQ("file.txt", aFileName.ToCString());
}

TEST_F(OSD_PathTest, FolderAndFileFromPath_ProtocolPaths)
{
  TCollection_AsciiString aFolder, aFileName;

  // Test HTTPS path
  OSD_Path::FolderAndFileFromPath("https://www.server.org/folder/file.gif", aFolder, aFileName);
  EXPECT_STREQ("https://www.server.org/folder/", aFolder.ToCString());
  EXPECT_STREQ("file.gif", aFileName.ToCString());

  // Test content protocol
  OSD_Path::FolderAndFileFromPath("content://path/file.jpg", aFolder, aFileName);
  EXPECT_STREQ("content://path/", aFolder.ToCString());
  EXPECT_STREQ("file.jpg", aFileName.ToCString());
}

TEST_F(OSD_PathTest, EdgeCases)
{
  TCollection_AsciiString aFolder, aFileName;

  // Test empty path
  OSD_Path::FolderAndFileFromPath("", aFolder, aFileName);
  EXPECT_STREQ("", aFolder.ToCString());
  EXPECT_STREQ("", aFileName.ToCString());

  // Test just separator
  OSD_Path::FolderAndFileFromPath("/", aFolder, aFileName);
  EXPECT_STREQ("/", aFolder.ToCString());
  EXPECT_STREQ("", aFileName.ToCString());

  OSD_Path::FolderAndFileFromPath("\\", aFolder, aFileName);
  EXPECT_STREQ("\\", aFolder.ToCString());
  EXPECT_STREQ("", aFileName.ToCString());

  // Test path with no file extension
  OSD_Path::FolderAndFileFromPath("/home/username/foldername", aFolder, aFileName);
  EXPECT_STREQ("/home/username/", aFolder.ToCString());
  EXPECT_STREQ("foldername", aFileName.ToCString());
}

TEST_F(OSD_PathTest, MixedSeparators)
{
  TCollection_AsciiString aFolder, aFileName;

  // Test mixed separators (this might happen in cross-platform scenarios)
  OSD_Path::FolderAndFileFromPath("C:/Users/John\\Documents/file.txt", aFolder, aFileName);
  // The exact behavior might depend on implementation, but it should handle this gracefully
  EXPECT_FALSE(aFolder.IsEmpty() || aFileName.IsEmpty());
}

TEST_F(OSD_PathTest, OCC310_TrekAndUpTrek)
{
  OSD_Path                aPath("/where/you/want/tmp/qwerty/tmp/");
  TCollection_AsciiString aTrek = aPath.Trek();

  // OSD_Path uses | as the internal portable trek separator on all platforms
  EXPECT_STREQ("|where|you|want|tmp|qwerty|tmp|", aTrek.ToCString());

  aPath.UpTrek();
  aTrek = aPath.Trek();

  EXPECT_STREQ("|where|you|want|tmp|qwerty|", aTrek.ToCString());
}

TEST_F(OSD_PathTest, OCC309_CurrentDirectoryAndUpTrek)
{
  OSD_Process             aProcess;
  OSD_Path                aPath = aProcess.CurrentDirectory();
  TCollection_AsciiString aSystemName1;
  aPath.SystemName(aSystemName1);
  EXPECT_FALSE(aSystemName1.IsEmpty());

  aPath.UpTrek();
  TCollection_AsciiString aSystemName2;
  aPath.SystemName(aSystemName2);
  EXPECT_FALSE(aSystemName2.IsEmpty());
  EXPECT_NE(aSystemName1, aSystemName2);
  EXPECT_LT(aSystemName2.Length(), aSystemName1.Length());
}

//=================================================================================================
// Validation Tests
//=================================================================================================
#ifdef _WIN32
// The folder of a long path did not fit the buffers of _splitpath (256 characters): the path lost
// its parts, its extension included, so that no data exchange provider was found for it; a long
// folder was cut without a terminating null when the path was rebuilt.
TEST_F(OSD_PathTest, LongWindowsPath)
{
  const std::string aFolder = std::string("C:\\Users\\a\\") + std::string(150, 'x') + "\\"
                              + std::string(150, 'y') + "\\..\\" + std::string(150, 'z');
  for (const std::string& aPrefix : {std::string(), std::string("\\\\?\\")})
  {
    const TCollection_AsciiString aName((aPrefix + aFolder + "\\model.step").c_str());
    const OSD_Path                aPath(aName);
    EXPECT_STREQ(aPath.Name().ToCString(), "model") << aPrefix;
    EXPECT_STREQ(aPath.Extension().ToCString(), ".step") << aPrefix;

    TCollection_AsciiString aSystemName;
    aPath.SystemName(aSystemName);
    EXPECT_GT(aSystemName.Length(), 450) << aPrefix;
    EXPECT_NE(aSystemName.Search(std::string(150, 'z').c_str()), -1) << aPrefix;
    EXPECT_NE(aSystemName.Search("model.step"), -1) << aPrefix;
  }
}
// SystemName rebuilt "\\?\C:\..." as "//?/C:/...", which Windows normalizes as an ordinary path:
// a folder longer than MAX_PATH was not found, so that OCAF SaveAs reported "No such directory".
TEST_F(OSD_PathTest, ExtendedLengthPrefixKeepsBackslashes)
{
  TCollection_AsciiString aSystemName;
  OSD_Path(TCollection_AsciiString("\\\\?\\C:\\a\\b\\model.cbf")).SystemName(aSystemName);
  EXPECT_STREQ(aSystemName.ToCString(), "\\\\?\\C:\\a\\b\\model.cbf");
  OSD_Path(TCollection_AsciiString("C:\\a\\b\\model.cbf")).SystemName(aSystemName);
  EXPECT_STREQ(aSystemName.ToCString(), "C:/a/b/model.cbf");

  const std::filesystem::path aRoot =
    std::filesystem::temp_directory_path()
    / ("occt_osd_path_" + std::to_string(OSD_Process().ProcessId()));
  const std::wstring aPrefix = L"\\\\?\\";
  const std::filesystem::path aLong =
    aRoot / std::string(120, 'x') / std::string(120, 'y') / std::string(120, 'z');
  std::error_code anErr;
  std::filesystem::create_directories(aPrefix + aLong.wstring(), anErr);
  EXPECT_FALSE(anErr) << anErr.message();
  std::ofstream(aPrefix + (aLong / "part.stp").wstring()) << "x";
  const std::string aLongName = "\\\\?\\" + aLong.string();
  EXPECT_GT(aLongName.size(), 260u);
  const OSD_Path aLongPath(TCollection_AsciiString(aLongName.c_str()));
  EXPECT_TRUE(OSD_Directory(aLongPath).Exists());

  // The search pattern of the iterators was joined with '/', which "\\?\" does not take.
  int aNbFiles = 0;
  for (OSD_FileIterator anIter(aLongPath, "*"); anIter.More(); anIter.Next())
  {
    ++aNbFiles;
  }
  EXPECT_EQ(aNbFiles, 1);
  int aNbDirs = 0;
  for (OSD_DirectoryIterator anIter(OSD_Path(TCollection_AsciiString(
                                      ("\\\\?\\" + (aRoot / std::string(120, 'x')).string()).c_str())),
                                    "*");
       anIter.More();
       anIter.Next())
  {
    OSD_Path aDir;
    anIter.Values().Path(aDir);
    aNbDirs += aDir.Name() == TCollection_AsciiString(std::string(120, 'y').c_str()) ? 1 : 0;
  }
  EXPECT_EQ(aNbDirs, 1);

  // An external STEP part is looked for next to the main file through AbsolutePath, which
  // rejected a folder with the prefix: the part was then looked for in the working directory.
  const TCollection_AsciiString anAbs =
    OSD_Path::AbsolutePath(TCollection_AsciiString((aLongName + "\\").c_str()), "part.stp");
  EXPECT_STREQ(anAbs.ToCString(), (aLongName + "\\part.stp").c_str());
  EXPECT_TRUE(OSD_File(OSD_Path(anAbs)).Exists());

  std::filesystem::remove_all(aPrefix + aRoot.wstring(), anErr);
}
#endif
