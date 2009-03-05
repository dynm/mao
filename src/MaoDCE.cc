//
// Copyright 2008 Google Inc.
//
// This program is free software; you can redistribute it and/or to
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   51 Franklin Street, Fifth Floor,
//   Boston, MA  02110-1301, USA.

// Dead Code Elimination
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"

#include <map>

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(DCE, 0) {
};

typedef std::map<BasicBlock *, bool> BasicBlockMap;

// recursively reach all basic blocks via the outedges,
// starting from the root.
//
static void Visit(BasicBlock *bb, BasicBlockMap *bbmap) {
  if ((*bbmap)[bb]) return;
  (*bbmap)[bb] = true;

  for (BasicBlock::ConstEdgeIterator outedges = bb->BeginOutEdges();
       outedges != bb->EndOutEdges(); ++outedges) {
    BasicBlock *target = (*outedges)->dest();
    Visit(target, bbmap);
  }
}

// Dead Code Elimination
//
// From the root node, recursively traverse all BBs, following
// the outedges.
//
// Every basic block that remains untouched, is dead code.
//
class DeadCodeElimPass : public MaoPass {
 public:
  DeadCodeElimPass(MaoUnit *mao, const CFG *cfg) :
    MaoPass("DCE", mao->mao_options(), MAO_OPTIONS(DCE), true, cfg),
    mao_(mao) {
  }

  void DoElim() {
    BasicBlockMap bbmap;
    FORALL_CFG_BB(cfg(),it)
      bbmap[(*it)] = false;

    BasicBlock *root = (*cfg()->Begin());
    Visit(root, &bbmap);

    FORALL_CFG_BB(cfg(),it)
      if (!bbmap[(*it)]) {
        BasicBlock *bb = *it;
        int num = bb->NumEntries();
        if (!num) {
          Trace(1, "Found dead, empty basic block");
        } else {
          if (num == 1) {
            if (bb->first_entry()->IsLabel())
              Trace(1, "Found dead, single label basic block");
            else
              Trace(1, "Found dead, single insn basic block");
          } else {
            Trace(1, "Found Dead Basic Block: BB#%d, %d insn",
                  (*it)->id(), num);
            if (tracing_level() > 0) {
              (*it)->first_entry()->PrintEntry(stderr);
            }
          }
        }
      }
  }

 private:
  MaoUnit   *mao_;
};


// External Entry Point
//
void PerformDeadCodeElimination(MaoUnit *mao, const CFG *cfg) {
  DeadCodeElimPass dce(mao, cfg);
  dce.set_timed();
  //disable for now - finding too many things
  dce.DoElim();
}
