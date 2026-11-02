// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.

#include <XBRepMesh_Factory.hxx>

#include <BRepMesh_DiscretRoot.hxx>
#include <BRepPrimAPI_MakeBox.hxx>

#include <gtest/gtest.h>

TEST(XBRepMesh_FactoryTest, StaticInstance_Registered)
{
  EXPECT_FALSE(BRepMesh_DiscretAlgoFactory::FindFactory("XBRepMesh").IsNull());
}

// A second instance must survive its constructor although the name is already taken
// (it used to be deleted inside the constructor: heap-use-after-free).
TEST(XBRepMesh_FactoryTest, SecondInstance_OwnedByCaller)
{
  const occ::handle<BRepMesh_DiscretAlgoFactory> aRegistered =
    BRepMesh_DiscretAlgoFactory::FindFactory("XBRepMesh");
  ASSERT_FALSE(aRegistered.IsNull());

  occ::handle<XBRepMesh_Factory> aSecond = new XBRepMesh_Factory();
  EXPECT_EQ(aSecond->GetRefCount(), 1);
  EXPECT_EQ(aSecond->Name(), "XBRepMesh");
  EXPECT_EQ(BRepMesh_DiscretAlgoFactory::FindFactory("XBRepMesh"), aRegistered);

  const occ::handle<BRepMesh_DiscretRoot> anAlgo =
    aSecond->CreateAlgorithm(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape(), 0.1, 0.5);
  EXPECT_FALSE(anAlgo.IsNull());
}