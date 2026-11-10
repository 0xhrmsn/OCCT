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

#include <DESTEP_ConfigurationNode.hxx>
#include <DE_Wrapper.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <Message_Printer.hxx>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

//! Keeps the warnings and failures sent while it is attached to the default messenger.
class CollectingPrinter : public Message_Printer
{
public:
  void send(const TCollection_AsciiString& theString,
            const Message_Gravity          theGravity) const override
  {
    if (theGravity >= Message_Warning)
    {
      myMessages.push_back(theString.ToCString());
    }
  }

  bool Contains(const char* theText) const
  {
    for (const std::string& aMessage : myMessages)
    {
      if (aMessage.find(theText) != std::string::npos)
      {
        return true;
      }
    }
    return false;
  }

  mutable std::vector<std::string> myMessages;
};

//! Wrapper with the STEP node of OCCT.
occ::handle<DE_Wrapper> stepWrapper(occ::handle<DESTEP_ConfigurationNode>& theNode)
{
  occ::handle<DE_Wrapper> aWrapper = new DE_Wrapper();
  theNode                          = new DESTEP_ConfigurationNode();
  aWrapper->Bind(theNode);
  return aWrapper;
}

//! Loads theResource into a new STEP wrapper; returns the messages sent.
occ::handle<CollectingPrinter> load(const TCollection_AsciiString&         theResource,
                                    bool&                                  theIsLoaded,
                                    occ::handle<DESTEP_ConfigurationNode>& theNode)
{
  occ::handle<DE_Wrapper>        aWrapper = stepWrapper(theNode);
  occ::handle<CollectingPrinter> aPrinter = new CollectingPrinter();
  Message::DefaultMessenger()->AddPrinter(aPrinter);
  theIsLoaded = aWrapper->Load(theResource);
  Message::DefaultMessenger()->RemovePrinter(aPrinter);
  return aPrinter;
}

} // namespace

// A configuration with a misspelled parameter, or a value of a wrong type, was loaded without any
// message: the parameter kept its value silently.
TEST(DE_WrapperConfigurationTest, Load_UnknownParameterAndWrongValue)
{
  occ::handle<DESTEP_ConfigurationNode> aNode;
  const TCollection_AsciiString         aSaved = stepWrapper(aNode)->Save();

  // guard: a configuration written by the wrapper is read back without any message
  bool                           isLoaded = false;
  occ::handle<CollectingPrinter> aPrinter = load(aSaved, isLoaded, aNode);
  EXPECT_TRUE(isLoaded);
  EXPECT_TRUE(aPrinter->myMessages.empty()) << aPrinter->myMessages.front();

  aPrinter = load(aSaved + "provider.STEP.OCC.write.unitt : 2\n", isLoaded, aNode);
  EXPECT_TRUE(isLoaded);
  EXPECT_TRUE(aPrinter->Contains("provider.STEP.OCC.write.unitt"));

  aPrinter = load(aSaved + "provider.STEP.OCC.write.unit : abc\n", isLoaded, aNode);
  EXPECT_TRUE(aPrinter->Contains("provider.STEP.OCC.write.unit is not an integer"));

  aPrinter = load(aSaved + "provider.STEP.OCC.write.unit 2\n", isLoaded, aNode);
  EXPECT_TRUE(aPrinter->Contains("without parameter"));
}
