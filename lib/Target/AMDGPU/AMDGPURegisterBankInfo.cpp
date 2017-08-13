//===- AMDGPURegisterBankInfo.cpp -------------------------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements the targeting of the RegisterBankInfo class for
/// AMDGPU.
/// \todo This should be generated by TableGen.
//===----------------------------------------------------------------------===//

#include "AMDGPURegisterBankInfo.h"
#include "AMDGPUInstrInfo.h"
#include "SIRegisterInfo.h"
#include "llvm/CodeGen/GlobalISel/RegisterBank.h"
#include "llvm/CodeGen/GlobalISel/RegisterBankInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

#define GET_TARGET_REGBANK_IMPL
#include "AMDGPUGenRegisterBank.inc"

// This file will be TableGen'ed at some point.
#include "AMDGPUGenRegisterBankInfo.def"

using namespace llvm;

AMDGPURegisterBankInfo::AMDGPURegisterBankInfo(const TargetRegisterInfo &TRI)
    : AMDGPUGenRegisterBankInfo(),
      TRI(static_cast<const SIRegisterInfo*>(&TRI)) {

  // HACK: Until this is fully tablegen'd
  static bool AlreadyInit = false;
  if (AlreadyInit)
    return;

  AlreadyInit = true;

  const RegisterBank &RBSGPR = getRegBank(AMDGPU::SGPRRegBankID);
  (void)RBSGPR;
  assert(&RBSGPR == &AMDGPU::SGPRRegBank);

  const RegisterBank &RBVGPR = getRegBank(AMDGPU::VGPRRegBankID);
  (void)RBVGPR;
  assert(&RBVGPR == &AMDGPU::VGPRRegBank);

}

unsigned AMDGPURegisterBankInfo::copyCost(const RegisterBank &A,
                                           const RegisterBank &B,
                                           unsigned Size) const {
  return RegisterBankInfo::copyCost(A, B, Size);
}

const RegisterBank &AMDGPURegisterBankInfo::getRegBankFromRegClass(
    const TargetRegisterClass &RC) const {

  if (TRI->isSGPRClass(&RC))
    return getRegBank(AMDGPU::SGPRRegBankID);

  return getRegBank(AMDGPU::VGPRRegBankID);
}

RegisterBankInfo::InstructionMappings
AMDGPURegisterBankInfo::getInstrAlternativeMappings(
    const MachineInstr &MI) const {

  const MachineFunction &MF = *MI.getParent()->getParent();
  const MachineRegisterInfo &MRI = MF.getRegInfo();

  unsigned Size = getSizeInBits(MI.getOperand(0).getReg(), MRI, *TRI);

  InstructionMappings AltMappings;
  switch (MI.getOpcode()) {
  case TargetOpcode::G_LOAD: {
    // FIXME: Should we be hard coding the size for these mappings?
    const InstructionMapping &SSMapping = getInstructionMapping(
        1, 1, getOperandsMapping(
                  {AMDGPU::getValueMapping(AMDGPU::SGPRRegBankID, Size),
                   AMDGPU::getValueMapping(AMDGPU::SGPRRegBankID, 64)}),
        2); // Num Operands
    AltMappings.push_back(&SSMapping);

    const InstructionMapping &VVMapping = getInstructionMapping(
        2, 1, getOperandsMapping(
                  {AMDGPU::getValueMapping(AMDGPU::VGPRRegBankID, Size),
                   AMDGPU::getValueMapping(AMDGPU::VGPRRegBankID, 64)}),
        2); // Num Operands
    AltMappings.push_back(&VVMapping);

    // FIXME: Should this be the pointer-size (64-bits) or the size of the
    // register that will hold the bufffer resourc (128-bits).
    const InstructionMapping &VSMapping = getInstructionMapping(
        3, 1, getOperandsMapping(
                  {AMDGPU::getValueMapping(AMDGPU::VGPRRegBankID, Size),
                   AMDGPU::getValueMapping(AMDGPU::SGPRRegBankID, 64)}),
        2); // Num Operands
    AltMappings.push_back(&VSMapping);

    return AltMappings;

  }
  default:
    break;
  }
  return RegisterBankInfo::getInstrAlternativeMappings(MI);
}

void AMDGPURegisterBankInfo::applyMappingImpl(
    const OperandsMapper &OpdMapper) const {
  return applyDefaultMapping(OpdMapper);
}

static bool isInstrUniform(const MachineInstr &MI) {
  if (!MI.hasOneMemOperand())
    return false;

  const MachineMemOperand *MMO = *MI.memoperands_begin();
  return AMDGPU::isUniformMMO(MMO);
}

const RegisterBankInfo::InstructionMapping &
AMDGPURegisterBankInfo::getInstrMappingForLoad(const MachineInstr &MI) const {

  const MachineFunction &MF = *MI.getParent()->getParent();
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallVector<const ValueMapping*, 8> OpdsMapping(MI.getNumOperands());
  unsigned Size = getSizeInBits(MI.getOperand(0).getReg(), MRI, *TRI);
  unsigned PtrSize = getSizeInBits(MI.getOperand(1).getReg(), MRI, *TRI);

  const ValueMapping *ValMapping;
  const ValueMapping *PtrMapping;

  if (isInstrUniform(MI)) {
    // We have a uniform instruction so we want to use an SMRD load
    ValMapping = AMDGPU::getValueMapping(AMDGPU::SGPRRegBankID, Size);
    PtrMapping = AMDGPU::getValueMapping(AMDGPU::SGPRRegBankID, PtrSize);
  } else {
    ValMapping = AMDGPU::getValueMapping(AMDGPU::VGPRRegBankID, Size);
    // FIXME: What would happen if we used SGPRRegBankID here?
    PtrMapping = AMDGPU::getValueMapping(AMDGPU::VGPRRegBankID, PtrSize);
  }

  OpdsMapping[0] = ValMapping;
  OpdsMapping[1] = PtrMapping;
  const RegisterBankInfo::InstructionMapping &Mapping = getInstructionMapping(
      1, 1, getOperandsMapping(OpdsMapping), MI.getNumOperands());
  return Mapping;

  // FIXME: Do we want to add a mapping for FLAT load, or should we just
  // handle that during instruction selection?
}

const RegisterBankInfo::InstructionMapping &
AMDGPURegisterBankInfo::getInstrMapping(const MachineInstr &MI) const {
  const RegisterBankInfo::InstructionMapping &Mapping = getInstrMappingImpl(MI);

  if (Mapping.isValid())
    return Mapping;

  const MachineFunction &MF = *MI.getParent()->getParent();
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallVector<const ValueMapping*, 8> OpdsMapping(MI.getNumOperands());

  bool IsComplete = true;
  switch (MI.getOpcode()) {
  default:
    IsComplete = false;
    break;
  case AMDGPU::G_CONSTANT: {
    unsigned Size = MRI.getType(MI.getOperand(0).getReg()).getSizeInBits();
    OpdsMapping[0] = AMDGPU::getValueMapping(AMDGPU::SGPRRegBankID, Size);
    break;
  }
  case AMDGPU::G_GEP: {
    for (unsigned i = 0, e = MI.getNumOperands(); i != e; ++i) {
      if (!MI.getOperand(i).isReg())
        continue;

      unsigned Size = MRI.getType(MI.getOperand(i).getReg()).getSizeInBits();
      OpdsMapping[i] = AMDGPU::getValueMapping(AMDGPU::SGPRRegBankID, Size);
    }
    break;
  }
  case AMDGPU::G_STORE: {
    assert(MI.getOperand(0).isReg());
    unsigned Size = MRI.getType(MI.getOperand(0).getReg()).getSizeInBits();
    // FIXME: We need to specify a different reg bank once scalar stores
    // are supported.
    const ValueMapping *ValMapping =
        AMDGPU::getValueMapping(AMDGPU::VGPRRegBankID, Size);
    // FIXME: Depending on the type of store, the pointer could be in
    // the SGPR Reg bank.
    // FIXME: Pointer size should be based on the address space.
    const ValueMapping *PtrMapping =
        AMDGPU::getValueMapping(AMDGPU::VGPRRegBankID, 64);

    OpdsMapping[0] = ValMapping;
    OpdsMapping[1] = PtrMapping;
    break;
  }

  case AMDGPU::G_LOAD:
    return getInstrMappingForLoad(MI);
  }

  if (!IsComplete) {
    unsigned BankID = AMDGPU::SGPRRegBankID;

    unsigned Size = 0;
    for (unsigned Idx = 0; Idx < MI.getNumOperands(); ++Idx) {
      // If the operand is not a register default to the size of the previous
      // operand.
      // FIXME: Can't we pull the types from the MachineInstr rather than the
      // operands.
      if (MI.getOperand(Idx).isReg())
        Size = getSizeInBits(MI.getOperand(Idx).getReg(), MRI, *TRI);
      OpdsMapping.push_back(AMDGPU::getValueMapping(BankID, Size));
    }
  }
  return getInstructionMapping(1, 1, getOperandsMapping(OpdsMapping),
                               MI.getNumOperands());
}
