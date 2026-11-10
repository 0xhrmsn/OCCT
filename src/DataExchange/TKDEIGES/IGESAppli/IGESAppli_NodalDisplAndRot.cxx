// Created by: CKY / Contract Toubro-Larsen
// Copyright (c) 1993-1999 Matra Datavision
// Copyright (c) 1999-2014 OPEN CASCADE SAS
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

//--------------------------------------------------------------------
//--------------------------------------------------------------------

#include <IGESAppli_NodalDisplAndRot.hxx>
#include <IGESAppli_Node.hxx>
#include <IGESBasic_HArray1OfHArray1OfXYZ.hxx>
#include <IGESDimen_GeneralNote.hxx>
#include <Standard_DimensionMismatch.hxx>
#include <Standard_Type.hxx>
#include <gp_XYZ.hxx>
#include <NCollection_Array1.hxx>
#include <NCollection_HArray1.hxx>
#include <IGESData_NonNull.pxx>

IMPLEMENT_STANDARD_RTTIEXT(IGESAppli_NodalDisplAndRot, IGESData_IGESEntity)

IGESAppli_NodalDisplAndRot::IGESAppli_NodalDisplAndRot() = default;

void IGESAppli_NodalDisplAndRot::Init(
  const occ::handle<NCollection_HArray1<occ::handle<IGESDimen_GeneralNote>>>& allNotes,
  const occ::handle<NCollection_HArray1<int>>&                                allIdentifiers,
  const occ::handle<NCollection_HArray1<occ::handle<IGESAppli_Node>>>&        allNodes,
  const occ::handle<IGESBasic_HArray1OfHArray1OfXYZ>&                         allRotParams,
  const occ::handle<IGESBasic_HArray1OfHArray1OfXYZ>&                         allTransParams)
{
  if (IGESData_NonNull(allNodes, "IGESAppli_NodalDisplAndRot::Init")->Lower() != 1
      || (IGESData_NonNull(allIdentifiers, "IGESAppli_NodalDisplAndRot::Init")->Lower() != 1
          || IGESData_NonNull(allIdentifiers, "IGESAppli_NodalDisplAndRot::Init")->Length()
               != IGESData_NonNull(allNodes, "IGESAppli_NodalDisplAndRot::Init")->Length())
      || (IGESData_NonNull(allTransParams, "IGESAppli_NodalDisplAndRot::Init")->Lower() != 1
          || IGESData_NonNull(allTransParams, "IGESAppli_NodalDisplAndRot::Init")->Length()
               != IGESData_NonNull(allNodes, "IGESAppli_NodalDisplAndRot::Init")->Length())
      || (IGESData_NonNull(allRotParams, "IGESAppli_NodalDisplAndRot::Init")->Lower() != 1
          || IGESData_NonNull(allRotParams, "IGESAppli_NodalDisplAndRot::Init")->Length()
               != IGESData_NonNull(allNodes, "IGESAppli_NodalDisplAndRot::Init")->Length()))
  {
    throw Standard_DimensionMismatch(
      "IGESAppli_NodalDisplAndRot : Init(Lengths of arrays inconsistent)");
  }

  for (int i = 1; i <= IGESData_NonNull(allNodes, "IGESAppli_NodalDisplAndRot::Init")->Length();
       i++)
  {
    occ::handle<NCollection_HArray1<gp_XYZ>> temp1 =
      IGESData_NonNull(allTransParams, "IGESAppli_NodalDisplAndRot::Init")->Value(i);
    occ::handle<NCollection_HArray1<gp_XYZ>> temp2 =
      IGESData_NonNull(allRotParams, "IGESAppli_NodalDisplAndRot::Init")->Value(i);
    // the lists of a node are not created when the number of cases is not read (damaged file)
    if (temp1.IsNull() || temp2.IsNull())
    {
      throw Standard_DimensionMismatch("IGESAppli_NodalDisplAndRot : Init");
    }
    if ((temp1->Lower() != 1
         || temp1->Length()
              != IGESData_NonNull(allNotes, "IGESAppli_NodalDisplAndRot::Init")->Length())
        || (temp2->Lower() != 1
            || temp2->Length()
                 != IGESData_NonNull(allNotes, "IGESAppli_NodalDisplAndRot::Init")->Length()))
    {
      throw Standard_DimensionMismatch(
        "IGESAppli_NodalDisplAndRot: Init(No. of Param per Node != Nbcases)");
    }
  }

  theNotes           = allNotes;
  theNodes           = allNodes;
  theNodeIdentifiers = allIdentifiers;
  theTransParam      = allTransParams;
  theRotParam        = allRotParams;
  InitTypeAndForm(138, 0);
}

int IGESAppli_NodalDisplAndRot::NbCases() const
{
  return theNotes->Length();
}

int IGESAppli_NodalDisplAndRot::NbNodes() const
{
  return theNodes->Length();
}

occ::handle<IGESDimen_GeneralNote> IGESAppli_NodalDisplAndRot::Note(const int Index) const
{
  return theNotes->Value(Index);
}

int IGESAppli_NodalDisplAndRot::NodeIdentifier(const int Index) const
{
  return theNodeIdentifiers->Value(Index);
}

occ::handle<IGESAppli_Node> IGESAppli_NodalDisplAndRot::Node(const int Index) const
{
  return theNodes->Value(Index);
}

gp_XYZ IGESAppli_NodalDisplAndRot::TranslationParameter(const int NodeNum, const int CaseNum) const
{
  return theTransParam->Value(NodeNum)->Value(CaseNum);
}

gp_XYZ IGESAppli_NodalDisplAndRot::RotationalParameter(const int NodeNum, const int CaseNum) const
{
  return theRotParam->Value(NodeNum)->Value(CaseNum);
}
