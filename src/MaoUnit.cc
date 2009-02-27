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

#include <iostream>
#include <sstream>

#include "MaoDebug.h"
#include "MaoUnit.h"

extern "C" const char *S_GET_NAME(symbolS *s);

//
// Class: MaoUnit
//

// Default to no subsection selected
// A default will be generated if necessary later on.
MaoUnit::MaoUnit(MaoOptions *mao_options)
    : current_subsection_(0), mao_options_(mao_options) {
  entry_vector_.clear();
  sub_sections_.clear();
  sections_.clear();
  functions_.clear();
}

MaoUnit::~MaoUnit() {
  // Remove subsections and free allocated memory
  for (std::vector<SubSection *>::iterator iter =
           sub_sections_.begin();
       iter != sub_sections_.end(); ++iter) {
    delete *iter;
  }


  // Remove functions and free allocated memory
  for (MaoUnit::ConstFunctionIterator iter = ConstFunctionBegin();
      iter != ConstFunctionEnd();
      ++iter) {
    delete *iter;
  }


  // Remove sections and free allocated memory
  for (std::map<const char *, Section *, ltstr>::const_iterator iter =
           sections_.begin();
       iter != sections_.end(); ++iter) {
    delete iter->second;
  }
}

void MaoUnit::PrintMaoUnit() const {
  PrintMaoUnit(stdout);
}

class EntryDumper : public MaoDebugAction {
 public:
  EntryDumper() {}

  void set_entry(MaoEntry *e) { entry_ = e; }

  virtual void Invoke(FILE *outfile) {
    fprintf(outfile, "***   generating asm for entry [%d], line: %d\n",
            entry_->id(), entry_->line_number());
  }

 private:
  MaoEntry *entry_;
};


// Prints all entries in the MaoUnit
void MaoUnit::PrintMaoUnit(FILE *out) const {
  EntryDumper entry_dumper;
  for (std::vector<SubSection *>::const_iterator iter = sub_sections_.begin();
       iter != sub_sections_.end(); ++iter) {
    SubSection *ss = *iter;
    for (SectionEntryIterator e_iter = ss->EntryBegin();
         e_iter != ss->EntryEnd();
         ++e_iter) {
      MaoEntry *e = *e_iter;
      entry_dumper.set_entry(e);
      e->PrintEntry(out);
    }
  }
}

void MaoUnit::PrintIR(bool print_entries, bool print_sections,
                      bool print_subsections, bool print_functions) const {
  PrintIR(stdout, print_entries, print_sections, print_subsections);
}


void MaoUnit::PrintIR(FILE *out, bool print_entries, bool print_sections,
                      bool print_subsections, bool print_functions) const {
  if (print_entries) {
    // Print the entries
    for (std::vector<SubSection *>::const_iterator iter = sub_sections_.begin();
         iter != sub_sections_.end(); ++iter) {
      SubSection *ss = *iter;
      for (SectionEntryIterator e_iter = ss->EntryBegin();
           e_iter != ss->EntryEnd();
           ++e_iter) {
        MaoEntry *e = *e_iter;
        fprintf(out, "[%5d][%c] ", e->id(), e->GetDescriptiveChar());
        if (MaoEntry::INSTRUCTION == e->Type()) {
          fprintf(out, "\t");
        }
        e->PrintIR(out);
        fprintf(out, "\n");
      }
    }
  }

  if (print_sections) {
    // Print the sections
    fprintf(out, "Sections : \n");
    for (ConstSectionIterator iter = ConstSectionBegin();
         iter != ConstSectionEnd();
         ++iter) {
      Section *section = *iter;
      fprintf(out, "[%3d] %s [", section->id(), section->name().c_str());
      const std::vector<SubSectionID> subsectionIDs =
          section->GetSubsectionIDs();
      for (std::vector<SubSectionID>::const_iterator ss_iter =
               subsectionIDs.begin();
           ss_iter != subsectionIDs.end();
           ++ss_iter) {
        fprintf(out, " %d", *ss_iter);
      }
      fprintf(out, "]\n");
    }
  }

  if (print_subsections) {
    // Print the subsections
    fprintf(out, "Subsections : \n");
    for (std::vector<SubSection *>::const_iterator iter = sub_sections_.begin();
         iter != sub_sections_.end(); ++iter) {
      SubSection *ss = *iter;
      fprintf(out, "[%3d] [%d-%d]: %s\n", ss->id(),
              ss->first_entry()->id(), ss->last_entry()->id(),
              ss->name().c_str());
    }
  }

  if (print_functions) {
    // Print the functions
    fprintf(out, "Functions : \n");
    for (MaoUnit::ConstFunctionIterator iter = ConstFunctionBegin();
         iter != ConstFunctionEnd();
         ++iter) {
      Function *function = *iter;
      fprintf(out, "[%3d] [%3d-%3d]: %s\n",
              function->id(),
              function->first_entry()->id(),
              function->last_entry()->id(),
              function->name().c_str());
    }
  }
}

Section *MaoUnit::GetSection(const std::string &section_name) const {
  Section *section = NULL;
  // See if section already have been created.
  std::map<const char *, Section *, ltstr>::const_iterator it =
      sections_.find(section_name.c_str());
  if (it != sections_.end()) {
    section = it->second;
  }
  return section;
}

std::pair<bool, Section *> MaoUnit::FindOrCreateAndFind(
    const char *section_name) {
  bool new_section = false;
  Section *section;
  // See if section already have been created.
  std::map<const char *, Section *, ltstr>::const_iterator it =
      sections_.find(section_name);
  if (it == sections_.end()) {
    // Create it.
    // TODO(martint): Use an ID factory for the ID
    section = new Section(section_name, sections_.size());
    sections_[section->name().c_str()] = section;
    new_section = true;
  } else {
    section = it->second;
  }
  MAO_ASSERT(section);
  return std::make_pair(new_section, section);
}

// Called when found a new subsection reference in the assembly.
bool MaoUnit::SetSubSection(const char *section_name,
                            unsigned int subsection_number,
                            MaoEntry *entry) {
  // Get (and possibly create) the section
  std::pair<bool, Section *> section_pair = FindOrCreateAndFind(section_name);
  Section *section = section_pair.second;
  MAO_ASSERT(section);
  // Create a new subsection
  // TODO(martint): create ID factory
  SubSection *subsection = new SubSection(sub_sections_.size(),
                                          subsection_number, section_name,
                                          section);

  // Makes it possible to link entries between subsections
  SubSection *last_subsection = section->GetLastSubSection();

  sub_sections_.push_back(subsection);
  section->AddSubSection(subsection);

  // set current_subsection!
  current_subsection_ = subsection;

  // Assume that the section is only one entry long.
  // last_entry is updated as we add entries..
  current_subsection_->set_first_entry(entry);
  current_subsection_->set_last_entry(entry);

  // Now we should check if we should link this entry back to the previous
  // subsection within this section!
  if (last_subsection) {
    MaoEntry *last_entry = last_subsection->last_entry();
    last_entry->set_next(entry);
    entry->set_prev(last_entry);
  }

  return section_pair.first;
}

LabelEntry *MaoUnit::GetLabelEntry(const char *label_name) const {
  std::map<const char *, LabelEntry *>::const_iterator iter =
      labels_.find(label_name);
  MAO_ASSERT(iter != labels_.end());
  return iter->second;
}


// Add an entry to the MaoUnit list
bool MaoUnit::AddEntry(MaoEntry *entry,
                       bool  create_default_section) {
  // Variables that _might_ get used.
  LabelEntry *label_entry;
  DirectiveEntry *directive_entry;
  Symbol *symbol;

  // next free ID for the entry
  EntryID entry_index = entry_vector_.size();

  MAO_ASSERT(entry);
  entry->set_id(entry_index);

  if (!current_subsection_ && !create_default_section) {
    SetSubSection("mao_start_section", 0,  entry);
    current_subsection_->set_start_section(true);
  }
  // Create a subsection if necessary
  if (create_default_section &&
      (!current_subsection_ || current_subsection_->start_section())) {
    SetSubSection(DEFAULT_SECTION_NAME, 0, entry);
    MAO_ASSERT(current_subsection_);
  }

  // Check the type
  switch (entry->Type()) {
    case MaoEntry::INSTRUCTION:
      break;
    case MaoEntry::LABEL:
      // A Label will generate in a new symbol in the symbol table
      label_entry = static_cast<LabelEntry *>(entry);
      MAO_ASSERT(labels_.insert(std::make_pair(label_entry->name(),
                                               label_entry)).second);
      symbol = symbol_table_.FindOrCreateAndFind(label_entry->name(),
                                                current_subsection_->section());
      MAO_ASSERT(symbol);
      // TODO(martint): The codes does not currently set the correct
      //                section for all labels in the symboltable.
      break;
    case MaoEntry::DEBUG:
      break;
    case MaoEntry::DIRECTIVE:
      // Update sections when necessary. Doing it here instead of
      // in ir.cc makes it possible to add the entry
      // when creating a new subsection.
      directive_entry = static_cast<DirectiveEntry *>(entry);
      if (directive_entry->op() == DirectiveEntry::SECTION) {
        // the name from operand
        MAO_ASSERT(directive_entry->NumOperands() > 0);
        const DirectiveEntry::Operand *section_name =
            directive_entry->GetOperand(0);
        MAO_ASSERT(section_name->type == DirectiveEntry::STRING);
        std::string *s_section_name = section_name->data.str;
        SetSubSection(s_section_name->c_str(), 0, entry);
      }
      break;
    case MaoEntry::UNDEFINED:
      break;
    default:
      // should never happen. Catch all cases above.
      MAO_RASSERT_MSG(0, "Entry type not recognised.");
  }

  // Add the entry to the compilation unit
  entry_vector_.push_back(entry);

  // Update subsection information
  if (current_subsection_) {
    current_subsection_->set_last_entry(entry);
  }

  return true;
}


// Adds a common symbol
bool MaoUnit::AddCommSymbol(const char *name, unsigned int common_size,
                            unsigned int common_align) {
  // A common symbol is different as it allows
  // several definitions of the symbol.
  // See http://sourceware.org/binutils/docs-2.19/as/Comm.html#Comm
  Symbol *symbol;
  if ( !symbol_table_.Exists(name) ) {
    // If symbol does not exists,
    // insert it. with default properties
    // TODO(martint): Use a ID factory
    Section *section = current_subsection_?
        (current_subsection_->section()):
        NULL;
    symbol = symbol_table_.Add(name,
                               new Symbol(name, symbol_table_.Size(), section));
    symbol->set_symbol_type(OBJECT_SYMBOL);
  } else {
    // Get the symbol
    symbol = symbol_table_.Find(name);
  }
  // Set the attributes associated with common symbols
  symbol->set_common(true);
  if (symbol->common_size() < common_size) {
    symbol->set_common_size(common_size);
    MAO_ASSERT(symbol->size() < symbol->common_size());
    symbol->set_size(symbol->common_size());
  }
  if (symbol->common_align() < common_align) {
    symbol->set_common_align(common_align);
  }
  return true;
}

long MaoUnit::BBNameGen::i = 0;
const char *MaoUnit::BBNameGen::GetUniqueName() {
  char *buff = strdup(".mao_label_XXXX");
  MAO_ASSERT(i <= LONG_MAX);
  sprintf(buff, ".mao_label_%ld", i);
  i++;
  return buff;
}

MaoUnit::FunctionIterator MaoUnit::FunctionBegin() {
  return functions_.begin();
}

MaoUnit::FunctionIterator MaoUnit::FunctionEnd() {
  return functions_.end();
}

MaoUnit::ConstFunctionIterator MaoUnit::ConstFunctionBegin() const {
  return functions_.begin();
}

MaoUnit::ConstFunctionIterator MaoUnit::ConstFunctionEnd() const {
  return functions_.end();
}

SectionIterator MaoUnit::SectionBegin() {
  return SectionIterator(sections_.begin());
}

SectionIterator MaoUnit::SectionEnd() {
  return SectionIterator(sections_.end());
}

ConstSectionIterator MaoUnit::ConstSectionBegin() const {
  return ConstSectionIterator(sections_.begin());
}

ConstSectionIterator MaoUnit::ConstSectionEnd() const {
  return ConstSectionIterator(sections_.end());
}

void MaoUnit::FindFunctions() {
  // Use the symbol-table to find out the names of
  // the functions in the code!
  for (SymbolIterator iter = symbol_table_.Begin();
       iter != symbol_table_.End();
       ++iter) {
    Symbol *symbol = *iter;
    if (symbol->IsFunction()) {
      // Find the entry given the label now
      MaoEntry *entry = GetLabelEntry(symbol->name());
      // TODO(martint): create ID factory for functions
      Function *function = new Function(symbol->name(), functions_.size());
      function->set_first_entry(entry);

      // Find the last entry in this function:
      // Initial idea:
      // Move forward until you each one of the following two:
      //   1. Start of a new function
      //      (Check for labels that are marked as functions
      //       in the symbol table).
      //   2. End of the section.
      //      (The next pointer of the Entry is NULL)
      MaoEntry *entry_tail;
      // Assume that the function starts with a label
      // and holds atleast one more entry!
      MAO_ASSERT(entry->Type() == MaoEntry::LABEL);
      entry_tail = entry->next();
      MAO_ASSERT(entry_tail->prev() == entry);
      MAO_ASSERT(entry_tail);
      // Stops at the end of a section, or breaks when a new functions is found.
      while (entry_tail->next()) {
        // check if we found the next function?
        if (entry_tail->next()->Type() == MaoEntry::LABEL) {
          // is it a function?
          LabelEntry *label_entry =
              static_cast<LabelEntry *>(entry_tail->next());
          Symbol *l_symbol = symbol_table_.Find(label_entry->name());
          MAO_ASSERT(l_symbol);
          if (l_symbol->IsFunction()) {
            break;
          }
        }
        entry_tail = entry_tail->next();
      }

      // Now entry_tail can not move more forward.
      // TODO(martint): Should we move it back though?
      function->set_last_entry(entry_tail);
      functions_.push_back(function);
    }
  }
  return;
}


Symbol *MaoUnit::AddSymbol(const char *name) {
  Section *section = current_subsection_?
      (current_subsection_->section()):
      NULL;
  // TODO(martint): Use a ID factory
  return symbol_table_.Add(name,
                           new Symbol(name, symbol_table_.Size(), section));
}


Symbol *MaoUnit::FindOrCreateAndFindSymbol(const char *name) {
  Section *section = current_subsection_?
      (current_subsection_->section()):
      NULL;
  return symbol_table_.FindOrCreateAndFind(name, section);
}



//
// Class: SectionIterator
//

Section *&SectionIterator::operator *() const {
  return section_iter_->second;
}

SectionIterator &SectionIterator::operator ++() {
  ++section_iter_;
  return *this;
}

SectionIterator &SectionIterator::operator --() {
  --section_iter_;
  return *this;
}

bool SectionIterator::operator ==(const SectionIterator &other) const {
  return (section_iter_ == other.section_iter_);
}

bool SectionIterator::operator !=(const SectionIterator &other) const {
  return !((*this) == other);
}


//
// Class: ConstSectionIterator
//

Section * const &ConstSectionIterator::operator *() const {
  return section_iter_->second;
}

ConstSectionIterator const &ConstSectionIterator::operator ++() {
  ++section_iter_;
  return *this;
}

ConstSectionIterator const &ConstSectionIterator::operator --() {
  --section_iter_;
  return *this;
}

bool ConstSectionIterator::operator ==(const ConstSectionIterator &other)
    const {
  return (section_iter_ == other.section_iter_);
}

bool ConstSectionIterator::operator !=(const ConstSectionIterator &other)
    const {
  return !((*this) == other);
}




//
// Class: MaoEntry
//

MaoEntry::MaoEntry(unsigned int line_number, const char *line_verbatim,
                   const MaoUnit *maounit) :
    id_(0), next_(0), prev_(0), line_number_(line_number), maounit_(maounit) {
  if (line_verbatim) {
    MAO_ASSERT(strlen(line_verbatim) < MAX_VERBATIM_ASSEMBLY_STRING_LENGTH);
    line_verbatim_ = strdup(line_verbatim);
  } else {
    line_verbatim_ = 0;
  }
}

MaoEntry::~MaoEntry() {
}


void MaoEntry::Spaces(unsigned int n, FILE *outfile) const {
  for (unsigned int i = 0; i < n; i++) {
    fprintf(outfile, " ");
  }
}

void MaoEntry::PrintSourceInfo(FILE *out) const {
  fprintf(out, "\t # [%d], line: %d\t%s\n", id(), line_number(),
          line_verbatim() ? line_verbatim() : "");
}


//
// Class: LabelEntry
//

void LabelEntry::PrintEntry(FILE *out) const {
  MAO_ASSERT(name_);
  fprintf(out, "%s:", name_);
  MaoEntry::PrintSourceInfo(out);
}

void LabelEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(name_);
  fprintf(out, "%s", name_);
}


//
// Class: DirectiveEntry
//

const char *const DirectiveEntry::kOpcodeNames[NUM_OPCODES] = {
  ".file",
  ".section",
  ".globl",
  ".local",
  ".weak",
  ".type",
  ".size",
  ".byte",
  ".word",
  ".long",
  ".quad",
  ".rva",
  ".ascii",
  ".string",
  ".string16",
  ".string32",
  ".string64",
  ".sleb128",
  ".uleb128",
  ".p2align",
  ".p2alignw",
  ".p2alignl",
  ".space",
  ".ds.b",
  ".ds.w",
  ".ds.l",
  ".ds.d",
  ".ds.x",
  ".comm",
  ".ident",
  ".set",  // identical to .equ
  ".equiv"
};

void DirectiveEntry::PrintEntry(::FILE *out) const {
  std::string operands;
  fprintf(out, "\t%s\t%s", GetOpcodeName(),
          OperandsToString(&operands).c_str());
  PrintSourceInfo(out);
}

void DirectiveEntry::PrintIR(::FILE *out) const {
  std::string operands;
  fprintf(out, "%s %s", GetOpcodeName(), OperandsToString(&operands).c_str());
}

const std::string &DirectiveEntry::OperandsToString(std::string *out) const {
  for (OperandVector::const_iterator iter = operands_.begin();
       iter != operands_.end(); ++iter) {
    if (iter != operands_.begin())
      out->append(", ");
    OperandToString(**iter, out);
  }

  return *out;
}

const char *DirectiveEntry::GetDotOrSymbol(symbolS *symbol) const {
  const char *s = S_GET_NAME(symbol);
  MAO_ASSERT(s);
  if (strcmp(s, "L0\001") == 0) {
    return ".";
  } else {
    return s;
  }
}

const std::string &DirectiveEntry::OperandExpressionToString(
    const expressionS *expr, std::string *out) const {
  switch (expr->X_op) {
    // SUPPORTED
      /* X_add_number (a constant expression).  */
    case O_constant:
      {
        std::ostringstream int_string;
        int_string << expr->X_add_number;
        out->append(int_string.str());
      }
      break;
      /* X_add_symbol + X_add_number.  */
    case O_symbol:
      {
        std::ostringstream exp_string;
        if (expr->X_add_symbol) {
          exp_string << GetDotOrSymbol(expr->X_add_symbol)
                     << "+";
        }
        exp_string << expr->X_add_number;
        out->append(exp_string.str());
      }
      break;
      /* (X_add_symbol + X_op_symbol) + X_add_number.  */
    case O_add:
      {
        std::ostringstream exp_string;
        if (expr->X_add_symbol) {
          exp_string << GetDotOrSymbol(expr->X_add_symbol)
                     << "+";
        }
        if (expr->X_op_symbol) {
          exp_string << GetDotOrSymbol(expr->X_op_symbol)
                     << "+";
        }
        exp_string << expr->X_add_number;
        out->append(exp_string.str());
      }
      break;
      /* (X_add_symbol - X_op_symbol) + X_add_number.  */
    case O_subtract:
      {
        std::ostringstream exp_string;
        if (expr->X_add_symbol) {
          exp_string << GetDotOrSymbol(expr->X_add_symbol)
                     << "-";
        }
        if (expr->X_op_symbol) {
          exp_string << GetDotOrSymbol(expr->X_op_symbol)
                     << "+";
        }
        exp_string << expr->X_add_number;
        out->append(exp_string.str());
      }
      break;

    // UNSUPPORTED
    /* An illegal expression.  */
    case O_illegal:
      /* A nonexistent expression.  */
    case O_absent:
      /* X_add_symbol + X_add_number - the base address of the image.  */
    case O_symbol_rva:
      /* A register (X_add_number is register number).  */
    case O_register:
      /* A big value.  If X_add_number is negative or 0, the value is in
         generic_floating_point_number.  Otherwise the value is in
         generic_bignum, and X_add_number is the number of LITTLENUMs in
         the value.  */
    case O_big:
      /* (- X_add_symbol) + X_add_number.  */
    case O_uminus:
      /* (~ X_add_symbol) + X_add_number.  */
    case O_bit_not:
      /* (! X_add_symbol) + X_add_number.  */
    case O_logical_not:
      /* (X_add_symbol * X_op_symbol) + X_add_number.  */
    case O_multiply:
      /* (X_add_symbol / X_op_symbol) + X_add_number.  */
    case O_divide:
      /* (X_add_symbol % X_op_symbol) + X_add_number.  */
    case O_modulus:
      /* (X_add_symbol << X_op_symbol) + X_add_number.  */
    case O_left_shift:
      /* (X_add_symbol >> X_op_symbol) + X_add_number.  */
    case O_right_shift:
      /* (X_add_symbol | X_op_symbol) + X_add_number.  */
    case O_bit_inclusive_or:
      /* (X_add_symbol |~ X_op_symbol) + X_add_number.  */
    case O_bit_or_not:
      /* (X_add_symbol ^ X_op_symbol) + X_add_number.  */
    case O_bit_exclusive_or:
      /* (X_add_symbol & X_op_symbol) + X_add_number.  */
    case O_bit_and:
      /* (X_add_symbol == X_op_symbol) + X_add_number.  */
    case O_eq:
      /* (X_add_symbol != X_op_symbol) + X_add_number.  */
    case O_ne:
      /* (X_add_symbol < X_op_symbol) + X_add_number.  */
    case O_lt:
      /* (X_add_symbol <= X_op_symbol) + X_add_number.  */
    case O_le:
      /* (X_add_symbol >= X_op_symbol) + X_add_number.  */
    case O_ge:
      /* (X_add_symbol > X_op_symbol) + X_add_number.  */
    case O_gt:
      /* (X_add_symbol && X_op_symbol) + X_add_number.  */
    case O_logical_and:
      /* (X_add_symbol || X_op_symbol) + X_add_number.  */
    case O_logical_or:
      /* X_op_symbol [ X_add_symbol ] */
    case O_index:
      /* machine dependent operators */
    case O_md1:
    case O_md2:
    case O_md3:
    case O_md4:
    case O_md5:
    case O_md6:
    case O_md7:
    case O_md8:
    case O_md9:
    case O_md10:
    case O_md11:
    case O_md12:
    case O_md13:
    case O_md14:
    case O_md15:
    case O_md16:
    case O_md17:
    case O_md18:
    case O_md19:
    case O_md20:
    case O_md21:
    case O_md22:
    case O_md23:
    case O_md24:
    case O_md25:
    case O_md26:
    case O_md27:
    case O_md28:
    case O_md29:
    case O_md30:
    case O_md31:
    case O_md32:
  /* this must be the largest value */
    case O_max:
    default:
      MAO_ASSERT_MSG(
          0,
          "OperandExpressionToString does not support the symbol %d\n",
          expr->X_op);
      break;
  }
  return *out;
}

const std::string &DirectiveEntry::OperandToString(const Operand &operand,
                                                   std::string *out) const {
  switch (operand.type) {
    case NO_OPERAND:
      break;
    case STRING:
      out->append(*operand.data.str);
      break;
    case INT: {
      std::ostringstream int_string;
      int_string << operand.data.i;
      out->append(int_string.str());
      break;
    }
    case SYMBOL:
      out->append(S_GET_NAME(operand.data.sym));
      break;
    case EXPRESSION:
      OperandExpressionToString(operand.data.expr, out);
      break;
    case EMPTY_OPERAND:
      // Nothing to do
      break;
    default:
      MAO_ASSERT(false);
  }
  return *out;
}

MaoEntry::EntryType DirectiveEntry::Type() const {
  return DIRECTIVE;
}


//
// Class: DebugEntry
//

void DebugEntry::PrintEntry(FILE *out) const {
  fprintf(out, "\t%s\t%s", key_.c_str(), value_.c_str());
  PrintSourceInfo(out);
}

void DebugEntry::PrintIR(FILE *out) const {
  fprintf(out, "%s %s", key_.c_str(), value_.c_str());
}


MaoEntry::EntryType DebugEntry::Type() const {
  return DEBUG;
}


//
// Class: InstructionEntry
//

InstructionEntry::InstructionEntry(i386_insn *instruction,
                                   unsigned int line_number,
                                   const char* line_verbatim,
                                   const MaoUnit *maounit) :
    MaoEntry(line_number, line_verbatim, maounit) {
  op_ = GetOpcode(instruction->tm.name);
  MAO_ASSERT(op_ != OP_invalid);
  MAO_ASSERT(instruction);
  instruction_ = CreateInstructionCopy(instruction);
}

InstructionEntry::~InstructionEntry() {
  MAO_ASSERT(instruction_);
  FreeInstruction();
}

void InstructionEntry::PrintEntry(FILE *out) const {
  PrintInstruction(out);
  PrintSourceInfo(out);
}

void InstructionEntry::PrintIR(FILE *out) const {
  PrintInstruction(out);
}

MaoEntry::EntryType InstructionEntry::Type() const {
  return INSTRUCTION;
}

const char *InstructionEntry::GetOp() const {
  MAO_ASSERT(instruction_);
  MAO_ASSERT(instruction_->tm.name);
  return(instruction_->tm.name);
}

// This deallocates memory allocated in CreateInstructionCopy().
void InstructionEntry::FreeInstruction() {
  MAO_ASSERT(instruction_);
  for (unsigned int i = 0; i < instruction_->operands; i++) {
    if (IsImmediateOperand(instruction_, i)) {
      delete instruction_->op[i].imms;
    }
    if (IsMemOperand(instruction_, i)) {
      delete instruction_->op[i].disps;
    }
    if (IsRegisterOperand(instruction_, i)) {
      delete instruction_->op[i].regs;
    }
  }
  for (unsigned int i = 0; i < 2; i++) {
    delete instruction_->seg[i];
  }
  delete instruction_->base_reg;
  delete instruction_->index_reg;
  delete instruction_;
}


// Given a register, create a copy to be used in our instruction
reg_entry *InstructionEntry::CopyRegEntry(const reg_entry *in_reg) {
  if (!in_reg)
    return 0;
  reg_entry *tmp_r;
  tmp_r = new reg_entry;
  MAO_ASSERT(tmp_r);
  MAO_ASSERT(strlen(in_reg->reg_name) < kMaxRegisterNameLength);
  tmp_r->reg_name = strdup(in_reg->reg_name);
  tmp_r->reg_type = in_reg->reg_type;
  tmp_r->reg_flags = in_reg->reg_flags;
  tmp_r->reg_num = in_reg->reg_num;
  return tmp_r;
}


bool InstructionEntry::IsMemOperand(const i386_insn *instruction,
                                  const unsigned int op_index) {
  MAO_ASSERT(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
  return (t.bitfield.disp8
          || t.bitfield.disp16
          || t.bitfield.disp32
          || t.bitfield.disp32s
          || t.bitfield.disp64
          || t.bitfield.baseindex);
}

bool InstructionEntry::IsImmediateOperand(const i386_insn *instruction,
                                        const unsigned int op_index) {
  MAO_ASSERT(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
  return (t.bitfield.imm1
          || t.bitfield.imm8
          || t.bitfield.imm8s
          || t.bitfield.imm16
          || t.bitfield.imm32
          || t.bitfield.imm32s
          || t.bitfield.imm64);
}

bool InstructionEntry::IsRegisterOperand(const i386_insn *instruction,
                                       const unsigned int op_index) {
  MAO_ASSERT(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
  return (t.bitfield.acc
          || t.bitfield.reg8
          || t.bitfield.reg16
          || t.bitfield.reg32
          || t.bitfield.reg64
          || t.bitfield.floatreg
          || t.bitfield.regxmm);
}

void InstructionEntry::PrintImmediateOperand(FILE *out,
                                           const enum bfd_reloc_code_real reloc,
                                           const expressionS *expr) const {
  switch (expr->X_op) {
    case O_constant:
      /* X_add_number (a constant expression).  */
      fprintf(out, "$%lld",
              (long long)expr->X_add_number);
      break;
    case O_symbol:
      /* X_add_symbol + X_add_number.  */
      if (expr->X_add_symbol) {
        fprintf(out, "$%s%s+",
                S_GET_NAME(expr->X_add_symbol),
                GetRelocString(reloc));
      }
      fprintf(out, "%lld",
              (long long)expr->X_add_number);
      break;
    case O_subtract:
      fprintf(out, "$(");
      /* (X_add_symbol - X_op_symbol) + X_add_number.  */
      if (expr->X_add_symbol || expr->X_op_symbol) {
        fprintf(out, "(");
      }
      if (expr->X_add_symbol) {
        fprintf(out, "%s%s",
                S_GET_NAME(expr->X_add_symbol),
                GetRelocString(reloc));
      }
      if (expr->X_op_symbol) {
        fprintf(out, "-%s",
                S_GET_NAME(expr->X_op_symbol));
      }
      if (expr->X_add_symbol || expr->X_op_symbol) {
        fprintf(out, ")+");
      }
      fprintf(out, "%lld)",
              (long long)expr->X_add_number);
      break;
    default:
      MAO_ASSERT_MSG(0, "Unable to print unsupported expression");
  }
  return;
}

const char *InstructionEntry::GetRelocString(
  const enum bfd_reloc_code_real reloc) const {
  switch (reloc) {
    case BFD_RELOC_X86_64_PLT32:
      return "@PLT";
    case BFD_RELOC_32_PCREL:
      return "@GOTPCREL";
    case BFD_RELOC_X86_64_TLSLD:
      return "@TLSLD";
    case BFD_RELOC_X86_64_TLSGD:
      return "@TLSGD";
    case BFD_RELOC_X86_64_DTPOFF32:
      return "@DTPOFF";
    case BFD_RELOC_NONE:  // found in "leaq    .LC0(%rip), %rcx"
      return "";
    case BFD_RELOC_X86_64_GOTTPOFF:
      return "@GOTTPOFF";
    default:
      MAO_ASSERT_MSG(false, "Unable to find info about reloc: %d", reloc);
      break;
  }
  return "";
}

// segment-override:signed-offset(base,index,scale)
void InstructionEntry::PrintMemoryOperand(FILE                  *out,
                                        const i386_operand_type &operand_type,
                                        const enum bfd_reloc_code_real reloc,
                                        const expressionS     *expr,
                                        const char            *segment_override,
                                        const bool            jumpabsolute)
    const {
  int scale[] = { 1, 2, 4, 8 };

  if (jumpabsolute) {
    fprintf(out, "*");
  }

  // segment-override:
  if (segment_override) {
    fprintf(out, "%%%s:", segment_override);
  }

  if (operand_type.bitfield.disp8 ||
      operand_type.bitfield.disp16 ||
      operand_type.bitfield.disp32 ||
      operand_type.bitfield.disp32s ||
      operand_type.bitfield.disp64) {
    // Signed-offset:
    switch (expr->X_op) {
      case O_constant:
        /* X_add_number (a constant expression).  */
        fprintf(out, "(%lld)",
                (long long)expr->X_add_number);
        break;
      case O_symbol:
        if (expr->X_add_number)
          fprintf(out, "(");
        /* X_add_symbol + X_add_number.  */
        if (expr->X_add_symbol) {
          fprintf(out, "%s%s",
                  S_GET_NAME(expr->X_add_symbol),
                  GetRelocString(reloc));
        }
        if (expr->X_add_number) {
          fprintf(out, "+%lld", (long long)expr->X_add_number);
          fprintf(out, ")");
        }
        break;
        /* (X_add_symbol - X_op_symbol) + X_add_number.  */
      case O_subtract:
        if (expr->X_add_symbol || expr->X_op_symbol) {
          fprintf(out, "(");
        }
        if (expr->X_add_symbol) {
          fprintf(out, "%s%s",
                  S_GET_NAME(expr->X_add_symbol),
                  GetRelocString(reloc));
        }
        // When GOTPCREL is used, the second symbol is implicit and
        // should not be printed.
        if (reloc != BFD_RELOC_32_PCREL) {
          if (expr->X_op_symbol) {
            fprintf(out, "-%s",
                    S_GET_NAME(expr->X_op_symbol));
          }
        }
        if (expr->X_add_symbol || expr->X_op_symbol) {
          fprintf(out, ")+");
        }
        fprintf(out, "%lld",
                (long long)expr->X_add_number);
        break;
      default:
        MAO_ASSERT_MSG(0, "Unable to print unsupported expression: %d",
                       expr->X_op);
    }
  }

  // (base,index,scale)
  if (instruction_->base_reg || instruction_->index_reg)
    fprintf(out, "(");
  if (instruction_->base_reg)
    fprintf(out, "%%%s", instruction_->base_reg->reg_name);
  if (instruction_->index_reg)
    fprintf(out, ",%%%s", instruction_->index_reg->reg_name);
  if (instruction_->log2_scale_factor)
    fprintf(out, ",%d", scale[instruction_->log2_scale_factor]);
  if (instruction_->base_reg || instruction_->index_reg)
    fprintf(out, ")");

  return;
}



bool InstructionEntry::PrintSuffix() const {
  if (instruction_->suffix == 0) {
    return false;
  }
  const MaoOpcode opcode_has_l_suffix[] =  {
    OP_movsbl, OP_movswl, OP_movzbl, OP_movzwl, OP_movswl, OP_cmovl, OP_cmovnl,
    OP_cwtl, OP_cltd
  };
  const MaoOpcode opcode_has_w_suffix[] =  {
    OP_cbtw, OP_fnstsw, OP_movsbw
  };
  const MaoOpcode opcode_has_b_suffix[] =  {
    OP_setb
  };
  if ((instruction_->suffix == 'l') &&
      IsInList(op(), opcode_has_l_suffix,
               sizeof(opcode_has_l_suffix)/sizeof(MaoOpcode))) {
    return false;
  }
  if ((instruction_->suffix == 'w') &&
      IsInList(op(), opcode_has_w_suffix,
               sizeof(opcode_has_w_suffix)/sizeof(MaoOpcode))) {
    return false;
  }
  if ((instruction_->suffix == 'b') &&
      IsInList(op(), opcode_has_b_suffix,
               sizeof(opcode_has_b_suffix)/sizeof(MaoOpcode))) {
    return false;
  }
  if (instruction_->suffix == 'q' &&
      instruction_->tm.name[strlen(instruction_->tm.name)-1] == 'q') {
    return false;
  }


  return true;
}

// Prints out the instruction.
// Goal is to make it print instructions generated by gcc
void InstructionEntry::PrintInstruction(FILE *out) const {
  const MaoOpcode rep_ops[] = {OP_ins, OP_outs, OP_movs, OP_lods, OP_stos};
  const MaoOpcode repe_ops[]= {OP_cmps, OP_scas};
  // Opcodes with REPE-bit prefix, but without any prefix in the assembly;
  const MaoOpcode norep_ops[]= {OP_movdqu, OP_mulss, OP_divss, OP_subss,
                                OP_movq, OP_cvttss2si, OP_addss};
  const MaoOpcode norepn_ops[]= {OP_movsd, OP_cvtsi2sd, OP_mulsd, OP_addsd,
                                 OP_divsd,  OP_subsd, OP_cvttsd2si};

// 
// OP_cvttsd2si
// OP_cvttsd2siq

  // Prefixes
  fprintf(out, "\t");
  if (instruction_->prefixes > 0) {
    for (unsigned int i = 0;
         i < sizeof(instruction_->prefix)/sizeof(unsigned char);
         ++i) {
      if (instruction_->prefix[i] != 0) {
        switch (instruction_->prefix[i]) {
          // http://www.intel.com/software/products/documentation/vlin/mergedprojects/analyzer_ec/mergedprojects/reference_olh/mergedProjects/instructions/instruct32_hh/vc276.htm
          // Repeats a string instruction the number of times specified in the
          // count register ((E)CX) or until the indicated condition of the ZF
          // flag is no longer met.
          // REP (repeat)                                 ins: INS, OUTS, MOVS
          //                                                   LODS, STOS
          // REPE,  REPZ (repeat while equal, zero)          ins: CMPS, SCAS
          // REPNE, REPNZ (repeat while not equal, not zero) ins: CMPS, SCAS
          case REPNE_PREFIX_OPCODE:
            if (IsInList(op(), repe_ops, sizeof(repe_ops)/sizeof(MaoOpcode))) {
              fprintf(out, "repne ");
            } else if (IsInList(op(), rep_ops,
                                sizeof(repe_ops)/sizeof(MaoOpcode))) {
              MAO_ASSERT_MSG(false,
                             "Found prefix does not match the instruction.");
            } else if (IsInList(op(), norepn_ops,
                                sizeof(norepn_ops)/sizeof(MaoOpcode))) {
	      ;
            } else {
              MAO_ASSERT_MSG(false,
                             "Unable to find instruction with repn* "
                             "prefix for %d", op());
            }
            break;
          case REPE_PREFIX_OPCODE:
            if (IsInList(op(), repe_ops, sizeof(repe_ops)/sizeof(MaoOpcode))) {
              fprintf(out, "repe ");
            } else if (IsInList(op(), rep_ops,
                                sizeof(rep_ops)/sizeof(MaoOpcode))) {
              fprintf(out, "rep ");
            } else if (IsInList(op(), norep_ops,
                                sizeof(norep_ops)/sizeof(MaoOpcode))) {
	      ;
	    } else {
              MAO_ASSERT_MSG(false,
                             "Unable to find instruction with rep* "
                             "prefix for %d", op());
            }
            break;
            // Rex prefixes are used for 64-bit extention
          case REX_OPCODE+0:  // e.g.: movb    %sil, -24(%rbp)
          case REX_OPCODE+1:  // e.g.: movl    $.LC0, %r8d
          case REX_OPCODE+2:
          case REX_OPCODE+3:
          case REX_OPCODE+4:  // e.g.: movl    %r8d, -100(%rbp)
          case REX_OPCODE+5:  // e.g.: movl    %r13d, %r8d
          case REX_OPCODE+6:
          case REX_OPCODE+7:
          case REX_OPCODE+8:  // e.g.: add $1, %rax
          case REX_OPCODE+9:  // e.g.: add $1, %r9
          case REX_OPCODE+10:
          case REX_OPCODE+11:
          case REX_OPCODE+12:  // e.g.: mov    %r9, (%eax)
          case REX_OPCODE+13:  // e.g : movq    %r12, %r9
          case REX_OPCODE+14:
          case REX_OPCODE+15:
            break;
          case DATA_PREFIX_OPCODE:  // e.g. : cmpw    %cx, %ax
            break;
          case CS_PREFIX_OPCODE:
          case DS_PREFIX_OPCODE:
          case ES_PREFIX_OPCODE:
          case FS_PREFIX_OPCODE:
          case GS_PREFIX_OPCODE:
          case SS_PREFIX_OPCODE:
            break;
          case ADDR_PREFIX_OPCODE:
            // used in movl (%eax), %eax
            break;
          default:
            MAO_ASSERT_MSG(false, "Unknown prefix found 0x%x\n",
                           instruction_->prefix[i]);
        }
      }
    }
  }

  // Do not print suffixes that are already in the template
  if (!PrintSuffix()) {
    fprintf(out, "%s\t", instruction_->tm.name);
  } else {
    fprintf(out, "%s%c\t", instruction_->tm.name, instruction_->suffix);
  }

  // Loop over operands
  for (unsigned int i = 0; i < instruction_->operands; ++i) {
    // IMMEDIATE
    // Immext means that an opcode modifier is encoded
    // in the instruction structure as an extra operand,
    // even though its not one!
    if (!instruction_->tm.opcode_modifier.immext &&
        IsImmediateOperand(instruction_, i)) {
      PrintImmediateOperand(out, instruction_->reloc[i],
                            instruction_->op[i].imms);
    }
    // This case is now handles in the displacement
    //     if (instruction_->tm.opcode_modifier.jump ||
    //         instruction_->tm.opcode_modifier.jumpdword ||
    //         instruction_->tm.opcode_modifier.jumpbyte) {
    //       if (instruction_->op[i].disps->X_op == O_symbol)
    //         fprintf(out, "%s",
    //                 S_GET_NAME(instruction_->op[i].disps->X_add_symbol) );
    //       else
    //         fprintf(out, "*unk*");
    //     }

    // MEMORY OPERANDS

    // Segment overrides are always placed in seg[0], even
    // if it applies to the second operand.
    if (IsMemOperand(instruction_, i)) {
      // Ugly hack:
      // for some string instruction, both operands have baseindex == 1
      // though only the first should be printed...
      // the first is implicit "(%edi)".
      // e.g.: rep   CMPSb (%edi), (%esi)
      if (instruction_->operands == 2 &&
          i == 0 &&
          IsMemOperand(instruction_, 1) &&
          IsInList(op(), repe_ops, sizeof(repe_ops)/sizeof(MaoOpcode))) {
        fprintf(out, "(%%edi) ");
      } else {
        PrintMemoryOperand(
            out,
            instruction_->types[i],
            instruction_->reloc[i],
            instruction_->op[i].disps,
            instruction_->seg[0]?instruction_->seg[0]->seg_name:0,
            instruction_->types[i].bitfield.jumpabsolute ||
            instruction_->tm.operand_types[i].bitfield.jumpabsolute);
      }
    }

    // ACC register
    if (instruction_->types[i].bitfield.floatacc) {
        fprintf(out, "%%st");
    }
    // TODO(martint): fix floatacc
    // if ((instruction_->types[i].bitfield.floatacc)...

    // REGISTERS

    // Segment register
    if (instruction_->types[i].bitfield.sreg2) {
      switch (instruction_->rm.reg) {
        case 0:
          fprintf(out, "%%es");
          break;
        case 1:
          fprintf(out, "%%cs");
          break;
        case 2:
          fprintf(out, "%%ss");
          break;
        case 3:
          fprintf(out, "%%ds");
          break;
        default:
          fprintf(stderr, "Unable to find segement regsiter sreg2%d\n",
                  instruction_->rm.reg);
          MAO_ASSERT(false);
      }
    }
    if (instruction_->types[i].bitfield.sreg3) {
      switch (instruction_->rm.reg) {
        case 4:
          fprintf(out, "%%fs");
          break;
        case 5:
          fprintf(out, "%%gs");
          break;
        default:
          fprintf(stderr, "Unable to find segement regsiter sreg3%d\n",
                  instruction_->rm.reg);
          MAO_ASSERT(false);
      }
    }

    // XMM registers
    if (instruction_->types[i].bitfield.regmmx) {
      if (instruction_->tm.operand_types[i].bitfield.regmmx) {
        fprintf(out, "%%mm%d", instruction_->rm.reg);
      } else if (instruction_->tm.operand_types[i].bitfield.regxmm) {
        fprintf(out, "%%xmm%d", instruction_->rm.reg);
      }
    }


    if (IsRegisterOperand(instruction_, i)) {
      if (instruction_->types[i].bitfield.jumpabsolute) {
        fprintf(out, "*");
      }
      fprintf(out, "%%%s", instruction_->op[i].regs->reg_name);
    }

    // Handle spacial case found in tc-i386.c:7326
    if (instruction_->types[i].bitfield.inoutportreg) {
      // its a register name!
      fprintf(out, "(%%dx)");
    }

    if (instruction_->types[i].bitfield.shiftcount) {
      // its a register name!
      fprintf(out, "%%%s", instruction_->op[i].regs->reg_name);
    }

    if (i < instruction_->operands-1)
      fprintf(out, ", ");
  }
}

// From an instruction given by gas, allocate new memory and populate the
// members.
i386_insn *InstructionEntry::CreateInstructionCopy(i386_insn *in_inst) {
  i386_insn *new_inst = new i386_insn;
  MAO_ASSERT(new_inst);

  // Copy all non-pointer data
  memcpy(new_inst, in_inst, sizeof(i386_insn));

  // Copy references
  for (unsigned int i = 0; i < new_inst->operands; i++) {
    // Select the correct part of the operand union.
    if (IsImmediateOperand(in_inst, i)) {
      new_inst->op[i].imms = new expressionS;
      MAO_ASSERT(new_inst->op[i].imms);
      *new_inst->op[i].imms = *in_inst->op[i].imms;
    } else if (IsMemOperand(in_inst, i) && in_inst->op[i].disps) {
      new_inst->op[i].disps = new expressionS;
      MAO_ASSERT(new_inst->op[i].disps);
      *new_inst->op[i].disps = *in_inst->op[i].disps;
    } else if (IsRegisterOperand(in_inst, i) ||
              in_inst->types[i].bitfield.shiftcount ) {
      new_inst->op[i].regs = CopyRegEntry(in_inst->op[i].regs);
    }
  }
  new_inst->base_reg = CopyRegEntry(in_inst->base_reg);
  new_inst->index_reg = CopyRegEntry(in_inst->index_reg);

  // Segment overrides
  for (unsigned int i = 0; i < 2; i++) {
    if (in_inst->seg[i]) {
      seg_entry *tmp_seg = new seg_entry;
      MAO_ASSERT(tmp_seg);
      MAO_ASSERT(strlen(in_inst->seg[i]->seg_name) < MAX_SEGMENT_NAME_LENGTH);
      tmp_seg->seg_name = strdup(in_inst->seg[i]->seg_name);
      tmp_seg->seg_prefix = in_inst->seg[i]->seg_prefix;
      new_inst->seg[i] = tmp_seg;
    }
  }

  return new_inst;
}

bool InstructionEntry::IsInList(MaoOpcode opcode, const MaoOpcode list[],
                              const unsigned int number_of_elements) const {
  for (unsigned int i = 0; i < number_of_elements; i++) {
    if (opcode == list[i])
      return true;
  }
  return false;
}

const MaoOpcode cond_jumps[] = {
  // Conditional jumps.
  OP_jo,  OP_jno, OP_jb,   OP_jc,  OP_jnae, OP_jnb,  OP_jnc, OP_jae, OP_je,
  OP_jz,  OP_jne, OP_jnz,  OP_jbe, OP_jna,  OP_jnbe, OP_ja,  OP_js,  OP_jns,
  OP_jp,  OP_jpe, OP_jnp,  OP_jpo, OP_jl,   OP_jnge, OP_jnl, OP_jge, OP_jle,
  OP_jng,  OP_jnle, OP_jg,

  // jcxz vs. jecxz is chosen on the basis of the address size prefix.
  OP_jcxz, OP_jecxz, OP_jecxz, OP_jrcxz,

  // loop variants
  OP_loop, OP_loopz, OP_loope, OP_loopnz, OP_loopne
};

bool InstructionEntry::HasFallThrough() const {
  // TODO(martint): Get this info from the i386_insn structure
  //                or complete the list
  if (IsReturn()) return false;
  if (!HasTarget()) return true;
  if (IsCall()) return true;
  if (IsInList(op(), cond_jumps, sizeof(cond_jumps)/sizeof(MaoOpcode))) {
    return true;
  }

  return false;
}

bool InstructionEntry::HasTarget() const {
  // TODO(martint): Get this info from the i386_insn structure
  //                or complete the list
  const MaoOpcode insn[] = {OP_jmp, OP_ljmp};
  if (IsInList(op(), insn, sizeof(insn)/sizeof(MaoOpcode)))
    return true;
  if (IsInList(op(), cond_jumps, sizeof(cond_jumps)/sizeof(MaoOpcode)))
    return true;

  return false;
}

const char *InstructionEntry::GetTarget() const {
  //
  for (unsigned int i =0; i < instruction_->operands; i++) {
    if (IsMemOperand(instruction_, i)) {
      // symbol?
      if (instruction_->types[i].bitfield.disp8 ||
          instruction_->types[i].bitfield.disp16 ||
          instruction_->types[i].bitfield.disp32 ||
          instruction_->types[i].bitfield.disp32s ||
          instruction_->types[i].bitfield.disp64) {
        if (instruction_->op[i].disps->X_op == O_symbol) {
          return S_GET_NAME(instruction_->op[i].disps->X_add_symbol);
        }
      }
    }
  }
  return "<UKNOWN>";
}

bool InstructionEntry::IsCall() const {
  const MaoOpcode calls[] = {
    OP_call, OP_lcall, OP_vmcall, OP_syscall, OP_vmmcall
  };
  return IsInList(op(), calls, sizeof(calls)/sizeof(MaoOpcode));
}

bool InstructionEntry::IsReturn() const {
  const MaoOpcode rets[] = {
    OP_ret, OP_lret, OP_retf, OP_iret, OP_sysret
  };
  return IsInList(op(), rets, sizeof(rets)/sizeof(MaoOpcode));
}


// Class: SubSection
void SubSection::set_last_entry(MaoEntry *entry) {
  // Link the entries, unless we insert the first entry. This special case is
  // handled in AddEntry().
  if (entry != first_entry_) {
    last_entry_->set_next(entry);
    entry->set_prev(last_entry_);
  }
  last_entry_ = entry;
}


SectionEntryIterator SubSection::EntryBegin() {
  return SectionEntryIterator(first_entry());
}

SectionEntryIterator SubSection::EntryEnd() {
  MaoEntry *entry = last_entry();
  if (entry) {
    entry = entry->next();
  }
  return SectionEntryIterator(entry);
}



//
// Class: Section
//

void Section::AddSubSection(SubSection  *subsection) {
  subsections_.push_back(subsection);
}


std::vector<SubSectionID> Section::GetSubsectionIDs() const {
  std::vector<SubSectionID> subsections;
  for (std::vector<SubSection *>::const_iterator ss_iter = subsections_.begin();
       ss_iter != subsections_.end();
       ++ss_iter) {
    subsections.push_back((*ss_iter)->id());
  }
  return subsections;
}

SectionEntryIterator Section::EntryBegin() {
  if (subsections_.size() == 0)
    return EntryEnd();
  SubSection *ss = subsections_[0];
  return SectionEntryIterator(ss->first_entry());
}

SectionEntryIterator Section::EntryEnd() {
  return SectionEntryIterator(NULL);
}

SubSection *Section::GetLastSubSection() const {
  if (subsections_.size() == 0) {
    return NULL;
  } else {
    return subsections_[subsections_.size()-1];
  }
}

//
// Class: SectionEntryIterator
//

SectionEntryIterator::SectionEntryIterator(MaoEntry *entry)
    : current_entry_(entry) {
  return;
}

SectionEntryIterator &SectionEntryIterator::operator ++() {
  current_entry_ = current_entry_->next();
  return *this;
}

SectionEntryIterator &SectionEntryIterator::operator --() {
  current_entry_ = current_entry_->prev();
  return *this;
}

bool SectionEntryIterator::operator ==(const SectionEntryIterator &other)
    const {
  return (current_entry_ == other.current_entry_);
}

bool SectionEntryIterator::operator !=(const SectionEntryIterator &other)
    const {
  return !((*this) == other);
}


//
// Class: Function
//

SectionEntryIterator Function::EntryBegin() {
  return SectionEntryIterator(first_entry());
}

SectionEntryIterator Function::EntryEnd() {
  MaoEntry *entry = last_entry();
  if (entry) {
    entry = entry->next();
  }
  return SectionEntryIterator(entry);
}


void Function::Print() {
  Print(stdout);
}

void Function::Print(FILE *out) {
  fprintf(out, "Function: %s\n", name().c_str());
  for (SectionEntryIterator iter = EntryBegin();
       iter != EntryEnd();
       ++iter) {
    MaoEntry *entry = *iter;
    entry->PrintEntry(out);
  }
}
