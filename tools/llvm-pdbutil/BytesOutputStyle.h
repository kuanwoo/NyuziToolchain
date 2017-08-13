//===- BytesOutputStyle.h ------------------------------------- *- C++ --*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVMPDBDUMP_BYTESOUTPUTSTYLE_H
#define LLVM_TOOLS_LLVMPDBDUMP_BYTESOUTPUTSTYLE_H

#include "LinePrinter.h"
#include "OutputStyle.h"

#include "llvm/Support/Error.h"

namespace llvm {

namespace codeview {
class LazyRandomTypeCollection;
}

namespace pdb {

class PDBFile;

class BytesOutputStyle : public OutputStyle {
public:
  BytesOutputStyle(PDBFile &File);

  Error dump() override;

private:
  void dumpNameMap();
  void dumpBlockRanges(uint32_t Min, uint32_t Max);
  void dumpByteRanges(uint32_t Min, uint32_t Max);
  void dumpFpm();
  void dumpStreamBytes();

  void dumpSectionContributions();
  void dumpSectionMap();
  void dumpModuleInfos();
  void dumpFileInfo();
  void dumpTypeServerMap();
  void dumpECData();

  void dumpModuleSyms();
  void dumpModuleC11();
  void dumpModuleC13();

  void dumpTypeIndex(uint32_t StreamIdx, ArrayRef<uint32_t> Indices);

  Expected<codeview::LazyRandomTypeCollection &>
  initializeTypes(uint32_t StreamIdx);

  std::unique_ptr<codeview::LazyRandomTypeCollection> TpiTypes;
  std::unique_ptr<codeview::LazyRandomTypeCollection> IpiTypes;

  PDBFile &File;
  LinePrinter P;
  ExitOnError Err;
  SmallVector<std::string, 8> StreamPurposes;
};
} // namespace pdb
} // namespace llvm

#endif
