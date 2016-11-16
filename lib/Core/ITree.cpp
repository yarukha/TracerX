//===-- ITree.cpp - Interpolation tree --------------------------*- C++ -*-===//
//
//               The Tracer-X KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementations of the classes for
/// interpolation and subsumption checks for search-space reduction.
///
//===----------------------------------------------------------------------===//

#include "ITree.h"
#include "Dependency.h"
#include "TimingSolver.h"

#include <klee/CommandLine.h>
#include <klee/Expr.h>
#include <klee/Solver.h>
#include <klee/SolverStats.h>
#include <klee/TimerStatIncrementer.h>
#include <klee/util/ExprPPrinter.h>
#include <fstream>
#include <vector>

using namespace klee;

/**/

std::string SearchTree::PrettyExpressionBuilder::bvConst32(uint32_t value) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}
std::string SearchTree::PrettyExpressionBuilder::bvConst64(uint64_t value) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}
std::string SearchTree::PrettyExpressionBuilder::bvZExtConst(uint64_t value) {
  return bvConst64(value);
}
std::string SearchTree::PrettyExpressionBuilder::bvSExtConst(uint64_t value) {
  return bvConst64(value);
}
std::string SearchTree::PrettyExpressionBuilder::bvBoolExtract(std::string expr,
                                                               int bit) {
  std::ostringstream stream;
  stream << expr << "[" << bit << "]";
  return stream.str();
}
std::string SearchTree::PrettyExpressionBuilder::bvExtract(std::string expr,
                                                           unsigned top,
                                                           unsigned bottom) {
  std::ostringstream stream;
  stream << expr << "[" << top << "," << bottom << "]";
  return stream.str();
}
std::string SearchTree::PrettyExpressionBuilder::eqExpr(std::string a,
                                                        std::string b) {
  if (a == "false")
    return "!" + b;
  return "(" + a + " = " + b + ")";
}

// logical left and right shift (not arithmetic)
std::string SearchTree::PrettyExpressionBuilder::bvLeftShift(std::string expr,
                                                             unsigned shift) {
  std::ostringstream stream;
  stream << "(" << expr << " \\<\\< " << shift << ")";
  return stream.str();
}
std::string SearchTree::PrettyExpressionBuilder::bvRightShift(std::string expr,
                                                              unsigned shift) {
  std::ostringstream stream;
  stream << "(" << expr << " \\>\\> " << shift << ")";
  return stream.str();
}
std::string
SearchTree::PrettyExpressionBuilder::bvVarLeftShift(std::string expr,
                                                    std::string shift) {
  return "(" + expr + " \\<\\< " + shift + ")";
}
std::string
SearchTree::PrettyExpressionBuilder::bvVarRightShift(std::string expr,
                                                     std::string shift) {
  return "(" + expr + " \\>\\> " + shift + ")";
}
std::string
SearchTree::PrettyExpressionBuilder::bvVarArithRightShift(std::string expr,
                                                          std::string shift) {
  return bvVarRightShift(expr, shift);
}

// Some STP-style bitvector arithmetic
std::string
SearchTree::PrettyExpressionBuilder::bvMinusExpr(std::string minuend,
                                                 std::string subtrahend) {
  return "(" + minuend + " - " + subtrahend + ")";
}
std::string
SearchTree::PrettyExpressionBuilder::bvPlusExpr(std::string augend,
                                                std::string addend) {
  return "(" + augend + " + " + addend + ")";
}
std::string
SearchTree::PrettyExpressionBuilder::bvMultExpr(std::string multiplacand,
                                                std::string multiplier) {
  return "(" + multiplacand + " * " + multiplier + ")";
}
std::string
SearchTree::PrettyExpressionBuilder::bvDivExpr(std::string dividend,
                                               std::string divisor) {
  return "(" + dividend + " / " + divisor + ")";
}
std::string
SearchTree::PrettyExpressionBuilder::sbvDivExpr(std::string dividend,
                                                std::string divisor) {
  return "(" + dividend + " / " + divisor + ")";
}
std::string
SearchTree::PrettyExpressionBuilder::bvModExpr(std::string dividend,
                                               std::string divisor) {
  return "(" + dividend + " % " + divisor + ")";
}
std::string
SearchTree::PrettyExpressionBuilder::sbvModExpr(std::string dividend,
                                                std::string divisor) {
  return "(" + dividend + " % " + divisor + ")";
}
std::string SearchTree::PrettyExpressionBuilder::notExpr(std::string expr) {
  return "!(" + expr + ")";
}
std::string SearchTree::PrettyExpressionBuilder::bvAndExpr(std::string lhs,
                                                           std::string rhs) {
  return "(" + lhs + " & " + rhs + ")";
}
std::string SearchTree::PrettyExpressionBuilder::bvOrExpr(std::string lhs,
                                                          std::string rhs) {
  return "(" + lhs + " | " + rhs + ")";
}
std::string SearchTree::PrettyExpressionBuilder::iffExpr(std::string lhs,
                                                         std::string rhs) {
  return "(" + lhs + " \\<=\\> " + rhs + ")";
}
std::string SearchTree::PrettyExpressionBuilder::bvXorExpr(std::string lhs,
                                                           std::string rhs) {
  return "(" + lhs + " xor " + rhs + ")";
}
std::string SearchTree::PrettyExpressionBuilder::bvSignExtend(std::string src) {
  return src;
}

// Some STP-style array domain interface
std::string SearchTree::PrettyExpressionBuilder::writeExpr(std::string array,
                                                           std::string index,
                                                           std::string value) {
  return "update(" + array + "," + index + "," + value + ")";
}
std::string SearchTree::PrettyExpressionBuilder::readExpr(std::string array,
                                                          std::string index) {
  return array + "[" + index + "]";
}

// ITE-expression constructor
std::string SearchTree::PrettyExpressionBuilder::iteExpr(
    std::string condition, std::string whenTrue, std::string whenFalse) {
  return "ite(" + condition + "," + whenTrue + "," + whenFalse + ")";
}

// Bitvector comparison
std::string SearchTree::PrettyExpressionBuilder::bvLtExpr(std::string lhs,
                                                          std::string rhs) {
  return "(" + lhs + " \\< " + rhs + ")";
}
std::string SearchTree::PrettyExpressionBuilder::bvLeExpr(std::string lhs,
                                                          std::string rhs) {
  return "(" + lhs + " \\<= " + rhs + ")";
}
std::string SearchTree::PrettyExpressionBuilder::sbvLtExpr(std::string lhs,
                                                           std::string rhs) {
  return "(" + lhs + " \\< " + rhs + ")";
}
std::string SearchTree::PrettyExpressionBuilder::sbvLeExpr(std::string lhs,
                                                           std::string rhs) {
  return "(" + lhs + " \\<= " + rhs + ")";
}

std::string SearchTree::PrettyExpressionBuilder::constructAShrByConstant(
    std::string expr, unsigned shift, std::string isSigned) {
  return bvRightShift(expr, shift);
}
std::string
SearchTree::PrettyExpressionBuilder::constructMulByConstant(std::string expr,
                                                            uint64_t x) {
  std::ostringstream stream;
  stream << "(" << expr << " * " << x << ")";
  return stream.str();
}
std::string
SearchTree::PrettyExpressionBuilder::constructUDivByConstant(std::string expr,
                                                             uint64_t d) {
  std::ostringstream stream;
  stream << "(" << expr << " / " << d << ")";
  return stream.str();
}
std::string
SearchTree::PrettyExpressionBuilder::constructSDivByConstant(std::string expr,
                                                             uint64_t d) {
  std::ostringstream stream;
  stream << "(" << expr << " / " << d << ")";
  return stream.str();
}

std::string
SearchTree::PrettyExpressionBuilder::getInitialArray(const Array *root) {
  std::string arrayExpr =
      buildArray(root->name.c_str(), root->getDomain(), root->getRange());

  if (root->isConstantArray()) {
    for (unsigned i = 0, e = root->size; i != e; ++i) {
      std::string prev = arrayExpr;
      arrayExpr = writeExpr(
          prev, constructActual(ConstantExpr::create(i, root->getDomain())),
          constructActual(root->constantValues[i]));
    }
  }
  return arrayExpr;
}
std::string
SearchTree::PrettyExpressionBuilder::getArrayForUpdate(const Array *root,
                                                       const UpdateNode *un) {
  if (!un) {
    return (getInitialArray(root));
  }
  return writeExpr(getArrayForUpdate(root, un->next),
                   constructActual(un->index), constructActual(un->value));
}

std::string SearchTree::PrettyExpressionBuilder::constructActual(ref<Expr> e) {
  switch (e->getKind()) {
  case Expr::Constant: {
    ConstantExpr *CE = cast<ConstantExpr>(e);
    int width = CE->getWidth();

    // Coerce to bool if necessary.
    if (width == 1)
      return CE->isTrue() ? getTrue() : getFalse();

    // Fast path.
    if (width <= 32)
      return bvConst32(CE->getZExtValue(32));
    if (width <= 64)
      return bvConst64(CE->getZExtValue());

    ref<ConstantExpr> Tmp = CE;
    return bvConst64(Tmp->Extract(0, 64)->getZExtValue());
  }

  // Special
  case Expr::NotOptimized: {
    NotOptimizedExpr *noe = cast<NotOptimizedExpr>(e);
    return constructActual(noe->src);
  }

  case Expr::Read: {
    ReadExpr *re = cast<ReadExpr>(e);
    assert(re && re->updates.root);
    return readExpr(getArrayForUpdate(re->updates.root, re->updates.head),
                    constructActual(re->index));
  }

  case Expr::Select: {
    SelectExpr *se = cast<SelectExpr>(e);
    std::string cond = constructActual(se->cond);
    std::string tExpr = constructActual(se->trueExpr);
    std::string fExpr = constructActual(se->falseExpr);
    return iteExpr(cond, tExpr, fExpr);
  }

  case Expr::Concat: {
    ConcatExpr *ce = cast<ConcatExpr>(e);
    unsigned numKids = ce->getNumKids();
    std::string res = constructActual(ce->getKid(numKids - 1));
    for (int i = numKids - 2; i >= 0; i--) {
      res = constructActual(ce->getKid(i)) + "." + res;
    }
    return res;
  }

  case Expr::Extract: {
    ExtractExpr *ee = cast<ExtractExpr>(e);
    std::string src = constructActual(ee->expr);
    int width = ee->getWidth();
    if (width == 1) {
      return bvBoolExtract(src, ee->offset);
    } else {
      return bvExtract(src, ee->offset + width - 1, ee->offset);
    }
  }

  // Casting
  case Expr::ZExt: {
    CastExpr *ce = cast<CastExpr>(e);
    std::string src = constructActual(ce->src);
    int width = ce->getWidth();
    if (width == 1) {
      return iteExpr(src, bvOne(), bvZero());
    } else {
      return src;
    }
  }

  case Expr::SExt: {
    CastExpr *ce = cast<CastExpr>(e);
    std::string src = constructActual(ce->src);
    return bvSignExtend(src);
  }

  // Arithmetic
  case Expr::Add: {
    AddExpr *ae = cast<AddExpr>(e);
    std::string left = constructActual(ae->left);
    std::string right = constructActual(ae->right);
    return bvPlusExpr(left, right);
  }

  case Expr::Sub: {
    SubExpr *se = cast<SubExpr>(e);
    std::string left = constructActual(se->left);
    std::string right = constructActual(se->right);
    return bvMinusExpr(left, right);
  }

  case Expr::Mul: {
    MulExpr *me = cast<MulExpr>(e);
    std::string right = constructActual(me->right);
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(me->left))
      if (CE->getWidth() <= 64)
        return constructMulByConstant(right, CE->getZExtValue());

    std::string left = constructActual(me->left);
    return bvMultExpr(left, right);
  }

  case Expr::UDiv: {
    UDivExpr *de = cast<UDivExpr>(e);
    std::string left = constructActual(de->left);

    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(de->right)) {
      if (CE->getWidth() <= 64) {
        uint64_t divisor = CE->getZExtValue();

        if (bits64::isPowerOfTwo(divisor)) {
          return bvRightShift(left, bits64::indexOfSingleBit(divisor));
        }
      }
    }

    std::string right = constructActual(de->right);
    return bvDivExpr(left, right);
  }

  case Expr::SDiv: {
    SDivExpr *de = cast<SDivExpr>(e);
    std::string left = constructActual(de->left);
    std::string right = constructActual(de->right);
    return sbvDivExpr(left, right);
  }

  case Expr::URem: {
    URemExpr *de = cast<URemExpr>(e);
    std::string left = constructActual(de->left);

    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(de->right)) {
      if (CE->getWidth() <= 64) {
        uint64_t divisor = CE->getZExtValue();

        if (bits64::isPowerOfTwo(divisor)) {
          unsigned bits = bits64::indexOfSingleBit(divisor);

          // special case for modding by 1 or else we bvExtract -1:0
          if (bits == 0) {
            return bvZero();
          } else {
            return bvExtract(left, bits - 1, 0);
          }
        }
      }
    }

    std::string right = constructActual(de->right);
    return bvModExpr(left, right);
  }

  case Expr::SRem: {
    SRemExpr *de = cast<SRemExpr>(e);
    std::string left = constructActual(de->left);
    std::string right = constructActual(de->right);
    return sbvModExpr(left, right);
  }

  // Bitwise
  case Expr::Not: {
    NotExpr *ne = cast<NotExpr>(e);
    std::string expr = constructActual(ne->expr);
    return notExpr(expr);
  }

  case Expr::And: {
    AndExpr *ae = cast<AndExpr>(e);
    std::string left = constructActual(ae->left);
    std::string right = constructActual(ae->right);
    return bvAndExpr(left, right);
  }

  case Expr::Or: {
    OrExpr *oe = cast<OrExpr>(e);
    std::string left = constructActual(oe->left);
    std::string right = constructActual(oe->right);
    return bvOrExpr(left, right);
  }

  case Expr::Xor: {
    XorExpr *xe = cast<XorExpr>(e);
    std::string left = constructActual(xe->left);
    std::string right = constructActual(xe->right);
    return bvXorExpr(left, right);
  }

  case Expr::Shl: {
    ShlExpr *se = cast<ShlExpr>(e);
    std::string left = constructActual(se->left);
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(se->right)) {
      return bvLeftShift(left, (unsigned)CE->getLimitedValue());
    } else {
      std::string amount = constructActual(se->right);
      return bvVarLeftShift(left, amount);
    }
  }

  case Expr::LShr: {
    LShrExpr *lse = cast<LShrExpr>(e);
    std::string left = constructActual(lse->left);
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(lse->right)) {
      return bvRightShift(left, (unsigned)CE->getLimitedValue());
    } else {
      std::string amount = constructActual(lse->right);
      return bvVarRightShift(left, amount);
    }
  }

  case Expr::AShr: {
    AShrExpr *ase = cast<AShrExpr>(e);
    std::string left = constructActual(ase->left);
    std::string amount = constructActual(ase->right);
    return bvVarArithRightShift(left, amount);
  }

  // Comparison
  case Expr::Eq: {
    EqExpr *ee = cast<EqExpr>(e);
    std::string left = constructActual(ee->left);
    std::string right = constructActual(ee->right);
    return eqExpr(left, right);
  }

  case Expr::Ult: {
    UltExpr *ue = cast<UltExpr>(e);
    std::string left = constructActual(ue->left);
    std::string right = constructActual(ue->right);
    return bvLtExpr(left, right);
  }

  case Expr::Ule: {
    UleExpr *ue = cast<UleExpr>(e);
    std::string left = constructActual(ue->left);
    std::string right = constructActual(ue->right);
    return bvLeExpr(left, right);
  }

  case Expr::Slt: {
    SltExpr *se = cast<SltExpr>(e);
    std::string left = constructActual(se->left);
    std::string right = constructActual(se->right);
    return sbvLtExpr(left, right);
  }

  case Expr::Sle: {
    SleExpr *se = cast<SleExpr>(e);
    std::string left = constructActual(se->left);
    std::string right = constructActual(se->right);
    return sbvLeExpr(left, right);
  }

  case Expr::Exists: {
    ExistsExpr *xe = cast<ExistsExpr>(e);
    std::string existentials;

    for (std::set<const Array *>::iterator it = xe->variables.begin(),
                                           ie = xe->variables.end();
         it != ie; ++it) {
      existentials += (*it)->name;
      if (it != ie)
        existentials += ",";
    }

    return "(exists (" + existentials + ") " + constructActual(xe->body) + ")";
  }

  default:
    assert(0 && "unhandled Expr type");
    return getTrue();
  }
}
std::string SearchTree::PrettyExpressionBuilder::construct(ref<Expr> e) {
  PrettyExpressionBuilder *instance = new PrettyExpressionBuilder();
  std::string ret = instance->constructActual(e);
  delete instance;
  return ret;
}

std::string SearchTree::PrettyExpressionBuilder::buildArray(
    const char *name, unsigned indexWidth, unsigned valueWidth) {
  return name;
}

std::string SearchTree::PrettyExpressionBuilder::getTrue() { return "true"; }
std::string SearchTree::PrettyExpressionBuilder::getFalse() { return "false"; }
std::string
SearchTree::PrettyExpressionBuilder::getInitialRead(const Array *root,
                                                    unsigned index) {
  return readExpr(getInitialArray(root), bvConst32(index));
}

SearchTree::PrettyExpressionBuilder::PrettyExpressionBuilder() {}

SearchTree::PrettyExpressionBuilder::~PrettyExpressionBuilder() {}

/**/

std::string SearchTree::NumberedEdge::render() const {
  std::ostringstream stream;
  stream << "Node" << source->nodeId << " -> Node" << destination->nodeId
         << " [style=dashed,label=\"" << number << "\"];";
  return stream.str();
}

/**/

unsigned long SearchTree::nextNodeId = 1;

SearchTree *SearchTree::instance = 0;

std::string SearchTree::recurseRender(const SearchTree::Node *node) {
  std::ostringstream stream;

  stream << "Node" << node->nodeId;
  std::string sourceNodeName = stream.str();

  size_t pos = 0;
  std::string replacementName(node->name);
  std::stringstream repStream1;
  while ((pos = replacementName.find("{")) != std::string::npos) {
    if (pos != std::string::npos) {
      repStream1 << replacementName.substr(0, pos) << "\\{";
      replacementName = replacementName.substr(pos + 2);
    }
  }
  repStream1 << replacementName;
  replacementName = repStream1.str();
  std::stringstream repStream2;
  while ((pos = replacementName.find("}")) != std::string::npos) {
    if (pos != std::string::npos) {
      repStream2 << replacementName.substr(0, pos) << "\\}";
      replacementName = replacementName.substr(pos + 2);
    }
  }
  repStream2 << replacementName;
  replacementName = repStream2.str();

  stream << " [shape=record,label=\"{" << node->nodeId << ": "
         << replacementName << "\\l";
  for (std::map<PathCondition *, std::pair<std::string, bool> >::const_iterator
           it = node->pathConditionTable.begin(),
           ie = node->pathConditionTable.end();
       it != ie; ++it) {
    stream << it->second.first;
    if (it->second.second)
      stream << " ITP";
    stream << "\\l";
  }
  if (node->subsumed) {
    stream << "(subsumed)\\l";
  }
  if (node->falseTarget || node->trueTarget)
    stream << "|{<s0>F|<s1>T}";
  stream << "}\"];\n";

  if (node->falseTarget) {
    stream << sourceNodeName << ":s0 -> Node" << node->falseTarget->nodeId
           << ";\n";
  }
  if (node->trueTarget) {
    stream << sourceNodeName + ":s1 -> Node" << node->trueTarget->nodeId
           << ";\n";
  }
  if (node->falseTarget) {
    stream << recurseRender(node->falseTarget);
  }
  if (node->trueTarget) {
    stream << recurseRender(node->trueTarget);
  }
  return stream.str();
}

std::string SearchTree::render() {
  std::string res("");

  // Simply return empty string when root is undefined
  if (!root)
    return res;

  std::ostringstream stream;
  for (std::vector<SearchTree::NumberedEdge *>::iterator
           it = subsumptionEdges.begin(),
           ie = subsumptionEdges.end();
       it != ie; ++it) {
    stream << (*it)->render() << "\n";
  }

  res = "digraph search_tree {\n";
  res += recurseRender(root);
  res += stream.str();
  res += "}\n";
  return res;
}

SearchTree::SearchTree(ITreeNode *_root) : subsumptionEdgeNumber(0) {
  root = SearchTree::Node::createNode(_root->getNodeId());
  itreeNodeMap[_root] = root;
}

SearchTree::~SearchTree() {
  if (root)
    delete root;

  itreeNodeMap.clear();

  for (std::vector<SearchTree::NumberedEdge *>::iterator
           it = subsumptionEdges.begin(),
           ie = subsumptionEdges.end();
       it != ie; ++it) {
    delete *it;
  }
  subsumptionEdges.clear();
}

void SearchTree::addChildren(ITreeNode *parent, ITreeNode *falseChild,
                             ITreeNode *trueChild) {
  if (!OUTPUT_INTERPOLATION_TREE)
    return;

  assert(SearchTree::instance && "Search tree graph not initialized");

  SearchTree::Node *parentNode = instance->itreeNodeMap[parent];
  parentNode->falseTarget =
      SearchTree::Node::createNode(falseChild->getNodeId());
  parentNode->trueTarget = SearchTree::Node::createNode(trueChild->getNodeId());
  instance->itreeNodeMap[falseChild] = parentNode->falseTarget;
  instance->itreeNodeMap[trueChild] = parentNode->trueTarget;
}

void SearchTree::setCurrentNode(ExecutionState &state,
                                const uintptr_t programPoint) {
  if (!OUTPUT_INTERPOLATION_TREE)
    return;

  assert(SearchTree::instance && "Search tree graph not initialized");

  ITreeNode *iTreeNode = state.itreeNode;
  SearchTree::Node *node = instance->itreeNodeMap[iTreeNode];
  if (!node->nodeId) {
    std::string functionName(
        state.pc->inst->getParent()->getParent()->getName().str());
    node->name = functionName + "\\l";
    llvm::raw_string_ostream out(node->name);
    state.pc->inst->print(out);
    node->name = out.str();

    node->iTreeNodeId = programPoint;
    node->nodeId = nextNodeId++;
  }
}

void SearchTree::markAsSubsumed(ITreeNode *iTreeNode,
                                SubsumptionTableEntry *entry) {
  if (!OUTPUT_INTERPOLATION_TREE)
    return;

  assert(SearchTree::instance && "Search tree graph not initialized");

  SearchTree::Node *node = instance->itreeNodeMap[iTreeNode];
  node->subsumed = true;
  SearchTree::Node *subsuming = instance->tableEntryMap[entry];
  instance->subsumptionEdges.push_back(new SearchTree::NumberedEdge(
      node, subsuming, ++(instance->subsumptionEdgeNumber)));
}

void SearchTree::addPathCondition(ITreeNode *iTreeNode,
                                  PathCondition *pathCondition,
                                  ref<Expr> condition) {
  if (!OUTPUT_INTERPOLATION_TREE)
    return;

  assert(SearchTree::instance && "Search tree graph not initialized");

  SearchTree::Node *node = instance->itreeNodeMap[iTreeNode];

  std::string s = PrettyExpressionBuilder::construct(condition);

  std::pair<std::string, bool> p(s, false);
  node->pathConditionTable[pathCondition] = p;
  instance->pathConditionMap[pathCondition] = node;
}

void SearchTree::addTableEntryMapping(ITreeNode *iTreeNode,
                                      SubsumptionTableEntry *entry) {
  if (!OUTPUT_INTERPOLATION_TREE)
    return;

  assert(SearchTree::instance && "Search tree graph not initialized");

  SearchTree::Node *node = instance->itreeNodeMap[iTreeNode];
  instance->tableEntryMap[entry] = node;
}

void SearchTree::setAsCore(PathCondition *pathCondition) {
  if (!OUTPUT_INTERPOLATION_TREE)
    return;

  assert(SearchTree::instance && "Search tree graph not initialized");

  instance->pathConditionMap[pathCondition]
      ->pathConditionTable[pathCondition]
      .second = true;
}

void SearchTree::save(std::string dotFileName) {
  if (!OUTPUT_INTERPOLATION_TREE)
    return;

  assert(SearchTree::instance && "Search tree graph not initialized");

  std::string g(instance->render());
  std::ofstream out(dotFileName.c_str());
  if (!out.fail()) {
    out << g;
    out.close();
  }
}

/**/

PathCondition::PathCondition(ref<Expr> &constraint, Dependency *dependency,
                             llvm::Value *condition, PathCondition *prev)
    : constraint(constraint), shadowConstraint(constraint), shadowed(false),
      dependency(dependency),
      condition(dependency ? dependency->getLatestValue(condition, constraint)
                           : 0),
      core(false), tail(prev) {}

PathCondition::~PathCondition() {}

ref<Expr> PathCondition::car() const { return constraint; }

PathCondition *PathCondition::cdr() const { return tail; }

void PathCondition::setAsCore() {
  // We mark all values to which this constraint depends
  dependency->markAllValues(condition);

  // We mark this constraint itself as core
  core = true;

  // We mark constraint as core in the search tree graph as well.
  SearchTree::setAsCore(this);
}

bool PathCondition::isCore() const { return core; }

ref<Expr>
PathCondition::packInterpolant(std::set<const Array *> &replacements) {
  ref<Expr> res;
  for (PathCondition *it = this; it != 0; it = it->tail) {
    if (it->core) {
      if (!it->shadowed) {
#ifdef ENABLE_Z3
        it->shadowConstraint =
            (NoExistential ? it->constraint
                           : ShadowArray::getShadowExpression(it->constraint,
                                                              replacements));
#else
	it->shadowConstraint = it->constraint;
#endif
        it->shadowed = true;
        it->boundVariables.insert(replacements.begin(), replacements.end());
      } else {
        // Already shadowed, we add the bound variables
        replacements.insert(it->boundVariables.begin(),
                            it->boundVariables.end());
      }
      if (!res.isNull()) {
        res = AndExpr::create(res, it->shadowConstraint);
      } else {
        res = it->shadowConstraint;
      }
    }
  }
  return res;
}

void PathCondition::dump() const {
  this->print(llvm::errs());
  llvm::errs() << "\n";
}

void PathCondition::print(llvm::raw_ostream &stream) const {
  stream << "[";
  for (const PathCondition *it = this; it != 0; it = it->tail) {
    it->constraint->print(stream);
    stream << ": " << (it->core ? "core" : "non-core");
    if (it->tail != 0)
      stream << ",";
  }
  stream << "]";
}

/**/

SubsumptionTableEntry::SubsumptionTableEntry(ITreeNode *node)
    : nodeId(node->getNodeId()) {
  std::set<const Array *> replacements;

  interpolant = node->getInterpolant(replacements);

  std::pair<Dependency::ConcreteStore, Dependency::SymbolicStore>
  storedExpressions = node->getStoredCoreExpressions(replacements);

  concreteAddressStore = storedExpressions.first;
  for (Dependency::ConcreteStore::iterator it = concreteAddressStore.begin(),
                                           ie = concreteAddressStore.end();
       it != ie; ++it) {
    concreteAddressStoreKeys.push_back(it->first);
  }

  symbolicAddressStore = storedExpressions.second;
  for (Dependency::SymbolicStore::iterator it = symbolicAddressStore.begin(),
                                           ie = symbolicAddressStore.end();
       it != ie; ++it) {
    symbolicAddressStoreKeys.push_back(it->first);
  }

  existentials = replacements;
}

SubsumptionTableEntry::~SubsumptionTableEntry() {}

bool
SubsumptionTableEntry::hasVariableInSet(std::set<const Array *> &existentials,
                                        ref<Expr> expr) {
  for (int i = 0, numKids = expr->getNumKids(); i < numKids; ++i) {
    if (llvm::isa<ReadExpr>(expr)) {
      ReadExpr *readExpr = llvm::dyn_cast<ReadExpr>(expr);
      const Array *array = (readExpr->updates).root;
      for (std::set<const Array *>::iterator it = existentials.begin(),
                                             ie = existentials.end();
           it != ie; ++it) {
        if ((*it) == array)
          return true;
      }
    } else if (hasVariableInSet(existentials, expr->getKid(i)))
      return true;
  }
  return false;
}

bool SubsumptionTableEntry::hasVariableNotInSet(
    std::set<const Array *> &existentials, ref<Expr> expr) {
  for (int i = 0, numKids = expr->getNumKids(); i < numKids; ++i) {
    if (llvm::isa<ReadExpr>(expr)) {
      ReadExpr *readExpr = llvm::dyn_cast<ReadExpr>(expr);
      const Array *array = (readExpr->updates).root;
      for (std::set<const Array *>::iterator it = existentials.begin(),
                                             ie = existentials.end();
           it != ie; ++it) {
        if ((*it) == array)
          return false;
      }
      return true;
    } else if (hasVariableNotInSet(existentials, expr->getKid(i)))
      return true;
  }
  return false;
}

ref<Expr>
SubsumptionTableEntry::simplifyArithmeticBody(ref<Expr> existsExpr,
                                              bool &hasExistentialsOnly) {
  assert(llvm::isa<ExistsExpr>(existsExpr));

  ExistsExpr *expr = llvm::dyn_cast<ExistsExpr>(existsExpr);

  // Assume the we shall return general ExistsExpr that does not contain
  // only existential variables.
  hasExistentialsOnly = false;

  std::set<const Array *> boundVariables = expr->variables;
  // We assume that the body is always a conjunction of interpolant in terms of
  // shadow (existentially-quantified) variables and state equality constraints,
  // which may contain both normal and shadow variables.
  ref<Expr> body = expr->body;

  // We only simplify a conjunction of interpolant and equalities
  if (!llvm::isa<AndExpr>(body))
    return existsExpr;

  // If the post-simplified body was a constant, simply return the body;
  if (llvm::isa<ConstantExpr>(body))
    return body;

  // The equality constraint is only a single disjunctive clause
  // of a CNF formula. In this case we simplify nothing.
  if (llvm::isa<OrExpr>(body->getKid(1)))
    return existsExpr;

  // Here we process equality constraints of shadow and normal variables.
  // The following procedure returns simplified version of the expression
  // by reducing any equality expression into constant (TRUE/FALSE).
  // if body is A and (Eq 2 4), body can be simplified into false.
  // if body is A and (Eq 2 2), body can be simplified into A.
  //
  // Along the way, It also collects the remaining equalities in equalityPack.
  // The equality constraints (body->getKid(1)) is a CNF of the form
  // c1 /\ ... /\ cn. This procedure collects into equalityPack all ci for
  // 1<=i<=n which are atomic equalities, to be used in simplifying the
  // interpolant.
  std::vector<ref<Expr> > equalityPack;

  ref<Expr> fullEqualityConstraint =
      simplifyEqualityExpr(equalityPack, body->getKid(1));

  if (fullEqualityConstraint->isFalse())
    return fullEqualityConstraint;

  // Try to simplify the interpolant. If the resulting simplification
  // was the constant true, then the equality constraints would contain
  // equality with constants only and no equality with shadow (existential)
  // variables, hence it should be safe to simply return the equality
  // constraint.
  std::vector<ref<Expr> > interpolantPack;

  ref<Expr> simplifiedInterpolant =
      simplifyInterpolantExpr(interpolantPack, body->getKid(0));
  if (simplifiedInterpolant->isTrue())
    return fullEqualityConstraint;

  if (fullEqualityConstraint->isTrue()) {
    // This is the case when the result is still an existentially-quantified
    // formula, but one that does not contain free variables.
    hasExistentialsOnly =
        !hasVariableNotInSet(expr->variables, simplifiedInterpolant);
    if (hasExistentialsOnly) {
      return existsExpr->rebuild(&simplifiedInterpolant);
    }
  }

  ref<Expr> newInterpolant;

  for (std::vector<ref<Expr> >::iterator it = interpolantPack.begin(),
                                         ie = interpolantPack.end();
       it != ie; ++it) {

    ref<Expr> interpolantAtom = (*it); // For example C cmp D

    // only process the interpolant that still has existential variables in it.
    if (hasVariableInSet(boundVariables, interpolantAtom)) {
      for (std::vector<ref<Expr> >::iterator it1 = equalityPack.begin(),
                                             ie1 = equalityPack.end();
           it1 != ie1; ++it1) {

        ref<Expr> equalityConstraint =
            *it1; // For example, say this constraint is A == B
        if (equalityConstraint->isFalse()) {
          return ConstantExpr::create(0, Expr::Bool);
        } else if (equalityConstraint->isTrue()) {
          return ConstantExpr::create(1, Expr::Bool);
        }
        // Left-hand side of the equality formula (A in our example) that
        // contains
        // the shadow expression (we assume that the existentially-quantified
        // shadow variable is always on the left side).
        ref<Expr> equalityConstraintLeft = equalityConstraint->getKid(0);

        // Right-hand side of the equality formula (B in our example) that does
        // not contain existentially-quantified shadow variables.
        ref<Expr> equalityConstraintRight = equalityConstraint->getKid(1);

        ref<Expr> newIntpLeft;
        ref<Expr> newIntpRight;

        // When the if condition holds, we perform substitution
        if (hasSubExpression(equalityConstraintLeft,
                             interpolantAtom->getKid(0))) {
          // Here we perform substitution, where given
          // an interpolant atom and an equality constraint,
          // we try to find a subexpression in the equality constraint
          // that matches the lhs expression of the interpolant atom.

          // Here we assume that the equality constraint is A == B and the
          // interpolant atom is C cmp D.

          // newIntpLeft == B
          newIntpLeft = equalityConstraintRight;

          // If equalityConstraintLeft does not have any arithmetic operation
          // we could directly assign newIntpRight = D, otherwise,
          // newIntpRight == A[D/C]
          if (!llvm::isa<BinaryExpr>(equalityConstraintLeft))
            newIntpRight = interpolantAtom->getKid(1);
          else {
            // newIntpRight is A, but with every occurrence of C replaced with D
            // i.e., newIntpRight == A[D/C]
            newIntpRight =
                replaceExpr(equalityConstraintLeft, interpolantAtom->getKid(0),
                            interpolantAtom->getKid(1));
          }

          interpolantAtom = ShadowArray::createBinaryOfSameKind(
              interpolantAtom, newIntpLeft, newIntpRight);
        }
      }
    }

    // We add the modified interpolant conjunct into a conjunction of
    // new interpolants.
    if (!newInterpolant.isNull()) {
      newInterpolant = AndExpr::create(newInterpolant, interpolantAtom);
    } else {
      newInterpolant = interpolantAtom;
    }
  }

  ref<Expr> newBody;

  if (!newInterpolant.isNull()) {
    if (!hasVariableInSet(expr->variables, newInterpolant))
      return newInterpolant;

    newBody = AndExpr::create(newInterpolant, fullEqualityConstraint);
  } else {
    newBody = AndExpr::create(simplifiedInterpolant, fullEqualityConstraint);
  }

  return existsExpr->rebuild(&newBody);
}

ref<Expr> SubsumptionTableEntry::replaceExpr(ref<Expr> originalExpr,
                                             ref<Expr> replacedExpr,
                                             ref<Expr> replacementExpr) {
  // We only handle binary expressions
  if (!llvm::isa<BinaryExpr>(originalExpr) ||
      llvm::isa<ConcatExpr>(originalExpr))
    return originalExpr;

  if (originalExpr->getKid(0) == replacedExpr)
    return ShadowArray::createBinaryOfSameKind(originalExpr, replacementExpr,
                                               originalExpr->getKid(1));

  if (originalExpr->getKid(1) == replacedExpr)
    return ShadowArray::createBinaryOfSameKind(
        originalExpr, originalExpr->getKid(0), replacementExpr);

  return ShadowArray::createBinaryOfSameKind(
      originalExpr,
      replaceExpr(originalExpr->getKid(0), replacedExpr, replacementExpr),
      replaceExpr(originalExpr->getKid(1), replacedExpr, replacementExpr));
}

bool SubsumptionTableEntry::hasSubExpression(ref<Expr> expr,
                                             ref<Expr> subExpr) {
  if (expr == subExpr)
    return true;
  if (expr->getNumKids() < 2 && expr != subExpr)
    return false;

  return hasSubExpression(expr->getKid(0), subExpr) ||
         hasSubExpression(expr->getKid(1), subExpr);
}

ref<Expr> SubsumptionTableEntry::simplifyInterpolantExpr(
    std::vector<ref<Expr> > &interpolantPack, ref<Expr> expr) {
  if (expr->getNumKids() < 2)
    return expr;

  if (llvm::isa<EqExpr>(expr) && llvm::isa<ConstantExpr>(expr->getKid(0)) &&
      llvm::isa<ConstantExpr>(expr->getKid(1))) {
    return (expr->getKid(0) == expr->getKid(1))
               ? ConstantExpr::create(1, Expr::Bool)
               : ConstantExpr::create(0, Expr::Bool);
  } else if (llvm::isa<NeExpr>(expr) &&
             llvm::isa<ConstantExpr>(expr->getKid(0)) &&
             llvm::isa<ConstantExpr>(expr->getKid(1))) {
    return (expr->getKid(0) != expr->getKid(1))
               ? ConstantExpr::create(1, Expr::Bool)
               : ConstantExpr::create(0, Expr::Bool);
  }

  ref<Expr> lhs = expr->getKid(0);
  ref<Expr> rhs = expr->getKid(1);

  if (!llvm::isa<AndExpr>(expr)) {

    // If the current expression has a form like (Eq false P), where P is some
    // comparison, we change it into the negation of P.
    if (llvm::isa<EqExpr>(expr) && expr->getKid(0)->getWidth() == Expr::Bool &&
        expr->getKid(0)->isFalse()) {
      if (llvm::isa<SltExpr>(rhs)) {
        expr = SgeExpr::create(rhs->getKid(0), rhs->getKid(1));
      } else if (llvm::isa<SgeExpr>(rhs)) {
        expr = SltExpr::create(rhs->getKid(0), rhs->getKid(1));
      } else if (llvm::isa<SleExpr>(rhs)) {
        expr = SgtExpr::create(rhs->getKid(0), rhs->getKid(1));
      } else if (llvm::isa<SgtExpr>(rhs)) {
        expr = SleExpr::create(rhs->getKid(0), rhs->getKid(1));
      }
    }

    // Collect unique interpolant expressions in one vector
    if (std::find(interpolantPack.begin(), interpolantPack.end(), expr) ==
        interpolantPack.end())
      interpolantPack.push_back(expr);

    return expr;
  }

  ref<Expr> simplifiedLhs = simplifyInterpolantExpr(interpolantPack, lhs);
  if (simplifiedLhs->isFalse())
    return simplifiedLhs;

  ref<Expr> simplifiedRhs = simplifyInterpolantExpr(interpolantPack, rhs);
  if (simplifiedRhs->isFalse())
    return simplifiedRhs;

  if (simplifiedLhs->isTrue())
    return simplifiedRhs;

  if (simplifiedRhs->isTrue())
    return simplifiedLhs;

  return AndExpr::create(simplifiedLhs, simplifiedRhs);
}

ref<Expr> SubsumptionTableEntry::simplifyEqualityExpr(
    std::vector<ref<Expr> > &equalityPack, ref<Expr> expr) {
  if (expr->getNumKids() < 2)
    return expr;

  if (llvm::isa<EqExpr>(expr)) {
    if (llvm::isa<ConstantExpr>(expr->getKid(0)) &&
        llvm::isa<ConstantExpr>(expr->getKid(1))) {
      return (expr->getKid(0) == expr->getKid(1))
                 ? ConstantExpr::create(1, Expr::Bool)
                 : ConstantExpr::create(0, Expr::Bool);
    }

    // Collect unique equality and in-equality expressions in one vector
    if (std::find(equalityPack.begin(), equalityPack.end(), expr) ==
        equalityPack.end())
      equalityPack.push_back(expr);

    return expr;
  }

  if (llvm::isa<AndExpr>(expr)) {
    ref<Expr> lhs = simplifyEqualityExpr(equalityPack, expr->getKid(0));
    if (lhs->isFalse())
      return lhs;

    ref<Expr> rhs = simplifyEqualityExpr(equalityPack, expr->getKid(1));
    if (rhs->isFalse())
      return rhs;

    if (lhs->isTrue())
      return rhs;

    if (rhs->isTrue())
      return lhs;

    return AndExpr::create(lhs, rhs);
  } else if (llvm::isa<OrExpr>(expr)) {
    // We provide throw-away dummy equalityPack, as we do not use the atomic
    // equalities within disjunctive clause to simplify the interpolant.
    std::vector<ref<Expr> > dummy;
    ref<Expr> lhs = simplifyEqualityExpr(dummy, expr->getKid(0));
    if (lhs->isTrue())
      return lhs;

    ref<Expr> rhs = simplifyEqualityExpr(dummy, expr->getKid(1));
    if (rhs->isTrue())
      return rhs;

    if (lhs->isFalse())
      return rhs;

    if (rhs->isFalse())
      return lhs;

    return OrExpr::create(lhs, rhs);
  }

  assert(!"Invalid expression type.");
}

ref<Expr>
SubsumptionTableEntry::getSubstitution(ref<Expr> equalities,
                                       std::map<ref<Expr>, ref<Expr> > &map) {
  // It is assumed the lhs is an expression on the existentially-quantified
  // variable whereas the rhs is an expression on the free variables.
  if (llvm::isa<EqExpr>(equalities)) {
    ref<Expr> lhs = equalities->getKid(0);
    if (isVariable(lhs)) {
      map[lhs] = equalities->getKid(1);
      return ConstantExpr::create(1, Expr::Bool);
    }
    return equalities;
  }

  if (llvm::isa<AndExpr>(equalities)) {
    ref<Expr> lhs = getSubstitution(equalities->getKid(0), map);
    ref<Expr> rhs = getSubstitution(equalities->getKid(1), map);
    if (lhs->isTrue()) {
      if (rhs->isTrue()) {
        return ConstantExpr::create(1, Expr::Bool);
      }
      return rhs;
    } else {
      if (rhs->isTrue()) {
        return lhs;
      }
      return AndExpr::create(lhs, rhs);
    }
  }
  return equalities;
}

bool SubsumptionTableEntry::detectConflictPrimitives(ExecutionState &state,
                                                     ref<Expr> query) {
  if (llvm::isa<ExistsExpr>(query))
    return true;

  std::vector<ref<Expr> > conjunction;
  if (!fetchQueryEqualityConjuncts(conjunction, query)) {
    return false;
  }

  for (std::vector<ref<Expr> >::const_iterator it1 = state.constraints.begin(),
                                               ie1 = state.constraints.end();
       it1 != ie1; ++it1) {

    if ((*it1)->getKind() != Expr::Eq)
      continue;

    for (std::vector<ref<Expr> >::const_iterator it2 = conjunction.begin(),
                                                 ie2 = conjunction.end();
         it2 != ie2; ++it2) {

      ref<Expr> stateConstraintExpr = it1->get();
      ref<Expr> queryExpr = it2->get();

      if (stateConstraintExpr != queryExpr &&
          (llvm::isa<EqExpr>(stateConstraintExpr) ||
           llvm::isa<EqExpr>(queryExpr))) {

        if (stateConstraintExpr ==
                EqExpr::create(ConstantExpr::create(0, Expr::Bool),
                               queryExpr) ||
            EqExpr::create(ConstantExpr::create(0, Expr::Bool),
                           stateConstraintExpr) == queryExpr) {
          return false;
        }
      }
    }
  }

  return true;
}

bool SubsumptionTableEntry::fetchQueryEqualityConjuncts(
    std::vector<ref<Expr> > &conjunction, ref<Expr> query) {

  if (!llvm::isa<AndExpr>(query)) {
    if (query->getKind() == Expr::Eq) {

      EqExpr *equality = llvm::dyn_cast<EqExpr>(query);

      if (llvm::isa<ConstantExpr>(equality->getKid(0)) &&
          llvm::isa<ConstantExpr>(equality->getKid(1)) &&
          equality->getKid(0) != equality->getKid(1)) {
        return false;
      } else {
        conjunction.push_back(query);
      }

    }
    return true;
  }

  return fetchQueryEqualityConjuncts(conjunction, query->getKid(0)) &&
         fetchQueryEqualityConjuncts(conjunction, query->getKid(1));
}

ref<Expr> SubsumptionTableEntry::simplifyExistsExpr(ref<Expr> existsExpr,
                                                    bool &hasExistentialsOnly) {
  assert(llvm::isa<ExistsExpr>(existsExpr));

  ref<Expr> body = llvm::dyn_cast<ExistsExpr>(existsExpr)->body;
  assert(llvm::isa<AndExpr>(body));

  std::map<ref<Expr>, ref<Expr> > substitution;
  ref<Expr> equalities = getSubstitution(body->getKid(1), substitution);
  ref<Expr> interpolant =
      ApplySubstitutionVisitor(substitution).visit(body->getKid(0));

  ExistsExpr *expr = llvm::dyn_cast<ExistsExpr>(existsExpr);
  if (hasVariableInSet(expr->variables, equalities)) {
    // we could also replace the occurrence of some variables with its
    // corresponding substitution mapping.
    equalities = ApplySubstitutionVisitor(substitution).visit(equalities);
  }

  ref<Expr> newBody = AndExpr::create(interpolant, equalities);

  // FIXME: Need to test the performance of the following.
  if (!hasVariableInSet(expr->variables, newBody))
    return newBody;

  ref<Expr> ret = simplifyArithmeticBody(existsExpr->rebuild(&newBody),
                                         hasExistentialsOnly);
  return ret;
}

bool SubsumptionTableEntry::subsumed(
    TimingSolver *solver, ExecutionState &state, double timeout,
    const std::pair<Dependency::ConcreteStore, Dependency::SymbolicStore>
        storedExpressions) {
#ifdef ENABLE_Z3
  // Tell the solver implementation that we are checking for subsumption for
  // collecting statistics of solver calls.
  SubsumptionCheckMarker subsumptionCheckMarker;
#endif

  // Quick check for subsumption in case the interpolant is empty
  if (empty())
    return true;

  Dependency::ConcreteStore stateConcreteAddressStore = storedExpressions.first;

  Dependency::SymbolicStore stateSymbolicAddressStore =
      storedExpressions.second;

  ref<Expr> stateEqualityConstraints;

  // Build constraints from concrete-address interpolant store
  for (std::vector<llvm::Value *>::iterator
           it1 = concreteAddressStoreKeys.begin(),
           ie1 = concreteAddressStoreKeys.end();
       it1 != ie1; ++it1) {
    const Dependency::ConcreteStoreMap tabledConcreteMap =
        concreteAddressStore[*it1];
    const Dependency::ConcreteStoreMap stateConcreteMap =
        stateConcreteAddressStore[*it1];
    const Dependency::SymbolicStoreMap stateSymbolicMap =
        stateSymbolicAddressStore[*it1];

    // If the current state does not constrain the same base, subsumption fails.
    if (stateConcreteMap.empty() && stateSymbolicMap.empty())
      return false;

    for (Dependency::ConcreteStoreMap::const_iterator
             it2 = tabledConcreteMap.begin(),
             ie2 = tabledConcreteMap.end();
         it2 != ie2; ++it2) {

      // The address is not constrained by the current state, therefore
      // the current state is incomparable to the stored interpolant,
      // and we therefore fail the subsumption.
      if (!stateConcreteMap.count(it2->first))
        return false;

      const Dependency::AddressValuePair stateConcrete =
          stateConcreteMap.at(it2->first);

      ref<Expr> tabledExpr = it2->second.second->getExpression();
      ref<Expr> res;

      if (!stateConcrete.second.isNull()) {
        //	  ref<StoredValue> tabledValue = it2->second.second;
        //	  ref<StoredValue> stateValue = stateConcrete.second;
        //	  if (!tabledValue->isWithinBound(stateValue)) {
        //	      return false;
        //	  }
        res = EqExpr::create(tabledExpr, stateConcrete.second->getExpression());
      }

      if (!stateSymbolicMap.empty()) {
        const ref<Expr> tabledConcreteAddress = it2->second.first;
        ref<Expr> conjunction;

        for (Dependency::SymbolicStoreMap::const_iterator
                 it3 = stateSymbolicMap.begin(),
                 ie3 = stateSymbolicMap.end();
             it3 != ie3; ++it3) {
          // Implication: if tabledConcreteAddress == it3->first, then
          // tabledValue ==
          // it3->second
          ref<Expr> newTerm = OrExpr::create(
              EqExpr::create(ConstantExpr::create(0, Expr::Bool),
                             EqExpr::create(tabledConcreteAddress, it3->first)),
              EqExpr::create(tabledExpr, it3->second->getExpression()));
          if (!conjunction.isNull()) {
            conjunction = AndExpr::create(newTerm, conjunction);
          } else {
            conjunction = newTerm;
          }
        }
        // If there were corresponding concrete as well as symbolic allocations
        // in the current state, conjunct them
        res = (!res.isNull() ? AndExpr::create(res, conjunction) : conjunction);
      }

      if (!res.isNull()) {
        stateEqualityConstraints =
            (stateEqualityConstraints.isNull()
                 ? res
                 : AndExpr::create(res, stateEqualityConstraints));
      }
    }
  }

  // Build constraints from symbolic-address interpolant store
  for (std::vector<llvm::Value *>::iterator
           it1 = symbolicAddressStoreKeys.begin(),
           ie1 = symbolicAddressStoreKeys.end();
       it1 != ie1; ++it1) {
    const Dependency::SymbolicStoreMap tabledSymbolicMap =
        symbolicAddressStore[*it1];
    const Dependency::ConcreteStoreMap stateConcreteMap =
        stateConcreteAddressStore[*it1];
    const Dependency::SymbolicStoreMap stateSymbolicMap =
        stateSymbolicAddressStore[*it1];

    ref<Expr> conjunction;

    for (Dependency::SymbolicStoreMap::const_iterator
             it2 = tabledSymbolicMap.begin(),
             ie2 = tabledSymbolicMap.end();
         it2 != ie2; ++it2) {
      ref<Expr> tabledSymbolicAddress = it2->first;
      ref<Expr> tabledValue = it2->second->getExpression();

      for (Dependency::ConcreteStoreMap::const_iterator
               it3 = stateConcreteMap.begin(),
               ie3 = stateConcreteMap.end();
           it3 != ie3; ++it3) {
        ref<Expr> stateConcreteAddress = it3->second.first;
        ref<Expr> stateValue = it3->second.second->getExpression();
        // Implication: if tabledSymbolicAddress == stateConcreteAddress, then
        // tabledValue == stateValue
        ref<Expr> newTerm =
            OrExpr::create(EqExpr::create(ConstantExpr::create(0, Expr::Bool),
                                          EqExpr::create(tabledSymbolicAddress,
                                                         stateConcreteAddress)),
                           EqExpr::create(tabledValue, stateValue));
        if (!conjunction.isNull()) {
          conjunction = AndExpr::create(newTerm, conjunction);
        } else {
          conjunction = newTerm;
        }
      }

      for (Dependency::SymbolicStoreMap::const_iterator
               it3 = stateSymbolicMap.begin(),
               ie3 = stateSymbolicMap.end();
           it3 != ie3; ++it3) {
        ref<Expr> stateSymbolicAddress = it3->first;
        ref<Expr> stateValue = it3->second->getExpression();
        // Implication: if tabledSymbolicAddress == stateSymbolicAddress then
        // tabledValue == stateValue
        ref<Expr> newTerm =
            OrExpr::create(EqExpr::create(ConstantExpr::create(0, Expr::Bool),
                                          EqExpr::create(tabledSymbolicAddress,
                                                         stateSymbolicAddress)),
                           EqExpr::create(tabledValue, stateValue));
        if (!conjunction.isNull()) {
          conjunction = AndExpr::create(newTerm, conjunction);
        } else {
          conjunction = newTerm;
        }
      }
    }

    if (!conjunction.isNull()) {
      stateEqualityConstraints =
          (stateEqualityConstraints.isNull()
               ? conjunction
               : AndExpr::create(conjunction, stateEqualityConstraints));
    }
  }

  Solver::Validity result;
  ref<Expr> query;

  // Here we build the query, after which it is always a conjunction of
  // the interpolant and the state equality constraints. Here we call
  // AndExpr::alloc instead of AndExpr::create as we need to guarantee that the
  // resulting expression is an AndExpr, otherwise simplifyExistsExpr would not
  // work.
  if (!interpolant.isNull()) {
    query =
        !stateEqualityConstraints.isNull()
            ? AndExpr::alloc(interpolant, stateEqualityConstraints)
            : AndExpr::alloc(interpolant, ConstantExpr::create(1, Expr::Bool));
  } else if (!stateEqualityConstraints.isNull()) {
    query = AndExpr::alloc(ConstantExpr::create(1, Expr::Bool),
                           stateEqualityConstraints);
  } else {
    // Here both the interpolant constraints and state equality
    // constraints are empty, therefore everything gets subsumed
    return true;
  }

  bool queryHasNoFreeVariables = false;

  if (!existentials.empty()) {
    ref<Expr> existsExpr = ExistsExpr::create(existentials, query);
    // llvm::errs() << "Before simplification:\n";
    // ExprPPrinter::printQuery(llvm::errs(), state.constraints, existsExpr);
    query = simplifyExistsExpr(existsExpr, queryHasNoFreeVariables);
  }

  // If query simplification result was false, we quickly fail without calling
  // the solver
  if (query->isFalse())
    return false;

  bool success = false;

  if (!detectConflictPrimitives(state, query))
    return false;

#ifdef ENABLE_Z3
  Z3Solver *z3solver = 0;
#endif /* ENABLE_Z3 */

  // We call the solver only when the simplified query is
  // not a constant and no contradictory unary constraints found from
  // solvingUnaryConstraints method.
  if (!llvm::isa<ConstantExpr>(query)) {

#ifdef ENABLE_Z3
    if (!existentials.empty() && llvm::isa<ExistsExpr>(query)) {
      // llvm::errs() << "Existentials not empty\n";

      // Instantiate a new Z3 solver to make sure we use Z3
      // without pre-solving optimizations. It would be nice
      // in the future to just run solver->evaluate so that
      // the optimizations can be used, but this requires
      // handling of quantified expressions by KLEE's pre-solving
      // procedure, which does not exist currently.
      z3solver = new Z3Solver();

      z3solver->setCoreSolverTimeout(timeout);

      if (queryHasNoFreeVariables) {
        // In case the query has no free variables, we reformulate
        // the solver call as satisfiability check of the body of
        // the query.
        ConstraintManager constraints;
        ref<ConstantExpr> tmpExpr;

        ref<Expr> falseExpr = ConstantExpr::create(0, Expr::Bool);
        constraints.addConstraint(EqExpr::create(falseExpr, query->getKid(0)));

        // llvm::errs() << "Querying for satisfiability check:\n";
        // ExprPPrinter::printQuery(llvm::errs(), constraints,
        // falseExpr);

        success = z3solver->getValue(Query(constraints, falseExpr), tmpExpr);
        result = success ? Solver::True : Solver::Unknown;
      } else {
        // llvm::errs() << "Querying for subsumption check:\n";
        // ExprPPrinter::printQuery(llvm::errs(), state.constraints,
        // query);

        success = z3solver->directComputeValidity(
            Query(state.constraints, query), result);
      }

      z3solver->setCoreSolverTimeout(0);

    } else
#endif /* ENABLE_Z3 */
    {
      // llvm::errs() << "No existential\n";

      // llvm::errs() << "Querying for subsumption check:\n";
      // ExprPPrinter::printQuery(llvm::errs(), state.constraints, query);

      // We call the solver in the standard way if the
      // formula is unquantified.

      solver->setTimeout(timeout);
      success = solver->evaluate(state, query, result);
      solver->setTimeout(0);
    }
  } else {
    // query is a constant expression
    if (query->isTrue())
      return true;
    return false;
  }

  if (success && result == Solver::True) {
    // llvm::errs() << "Solver decided validity\n";
    std::vector<ref<Expr> > unsatCore;
#ifdef ENABLE_Z3
    if (z3solver) {
      unsatCore = z3solver->getUnsatCore();
      delete z3solver;
    } else
#endif /* ENABLE_Z3 */
      unsatCore = solver->getUnsatCore();

    // State subsumed, we mark needed constraints on the
    // path condition.

    // We create path condition marking structure to mark core constraints
    state.itreeNode->unsatCoreMarking(unsatCore);
    return true;
  }

  // Here the solver could not decide that the subsumption is valid.
  // It may have decided invalidity, however,
  // CexCachingSolver::computeValidity,
  // which was eventually called from solver->evaluate
  // is conservative, where it returns Solver::Unknown even in case when
  // invalidity is established by the solver.
  // llvm::errs() << "Solver did not decide validity\n";

#ifdef ENABLE_Z3
  if (z3solver)
    delete z3solver;
#endif /* ENABLE_Z3 */

  return false;
}

ref<Expr> SubsumptionTableEntry::getInterpolant() const { return interpolant; }

void SubsumptionTableEntry::print(llvm::raw_ostream &stream) const {
  stream << "------------ Subsumption Table Entry ------------\n";
  stream << "Program point = " << nodeId << "\n";
  stream << "interpolant = ";
  if (!interpolant.isNull())
    interpolant->print(stream);
  else
    stream << "(empty)";
  stream << "\n";

  if (!concreteAddressStore.empty()) {
    stream << "concrete store = [";
    for (Dependency::ConcreteStore::const_iterator
             is1 = concreteAddressStore.begin(),
             ie1 = concreteAddressStore.end(), it1 = is1;
         it1 != ie1; ++it1) {
      for (Dependency::ConcreteStoreMap::const_iterator
               is2 = it1->second.begin(),
               ie2 = it1->second.end(), it2 = is2;
           it2 != ie2; ++it2) {
        if (it1 != is1 || it2 != is2)
          stream << ",";
        stream << "(";
        it2->second.first->print(stream);
        stream << ",";
        it2->second.second->print(stream);
        stream << ")";
      }
    }
    stream << "]\n";
  }

  if (!symbolicAddressStore.empty()) {
    stream << "symbolic store = [";
    for (Dependency::SymbolicStore::const_iterator
             is1 = symbolicAddressStore.begin(),
             ie1 = symbolicAddressStore.end(), it1 = is1;
         it1 != ie1; ++it1) {
      for (Dependency::SymbolicStoreMap::const_iterator
               is2 = it1->second.begin(),
               ie2 = it1->second.end(), it2 = is2;
           it2 != ie2; ++it2) {
        if (it1 != is1 || it2 != is2)
          stream << ",";
        stream << "(";
        it2->first->print(stream);
        stream << ",";
        it2->second->print(stream);
        stream << ")";
      }
    }
    stream << "]\n";
  }

  if (!existentials.empty()) {
    stream << "existentials = [";
    for (std::set<const Array *>::const_iterator is = existentials.begin(),
                                                 ie = existentials.end(),
                                                 it = is;
         it != ie; ++it) {
      if (it != is)
        stream << ", ";
      stream << (*it)->name;
    }
    stream << "]\n";
  }
}

void SubsumptionTableEntry::printStat(std::stringstream &stream) {
  stream << "KLEE: done:     Time for actual solver calls in subsumption check "
            "(ms) = " << ((double)stats::subsumptionQueryTime.getValue()) / 1000
         << "\n";
  stream << "KLEE: done:     Number of solver calls for subsumption check "
            "(failed) = " << stats::subsumptionQueryCount.getValue() << " ("
         << stats::subsumptionQueryFailureCount.getValue() << ")\n";
}

/**/

Statistic ITree::setCurrentINodeTime("SetCurrentINodeTime",
                                     "SetCurrentINodeTime");
Statistic ITree::removeTime("RemoveTime", "RemoveTime");
Statistic ITree::subsumptionCheckTime("SubsumptionCheckTime",
                                      "SubsubmptionCheckTime");
Statistic ITree::markPathConditionTime("MarkPathConditionTime", "MarkPCTime");
Statistic ITree::splitTime("SplitTime", "SplitTime");
Statistic ITree::executeOnNodeTime("ExecuteOnNodeTime", "ExecuteOnNodeTime");

double ITree::entryNumber;

double ITree::programPointNumber;

bool ITree::symbolicExecutionError = false;

unsigned long ITree::subsumptionCheckCount = 0;

void ITree::printTimeStat(std::stringstream &stream) {
  stream << "KLEE: done:     setCurrentINode = "
         << ((double)setCurrentINodeTime.getValue()) / 1000 << "\n";
  stream << "KLEE: done:     remove = " << ((double)removeTime.getValue()) /
                                               1000 << "\n";
  stream << "KLEE: done:     subsumptionCheck = "
         << ((double)subsumptionCheckTime.getValue()) / 1000 << "\n";
  stream << "KLEE: done:     markPathCondition = "
         << ((double)markPathConditionTime.getValue()) / 1000 << "\n";
  stream << "KLEE: done:     split = " << ((double)splitTime.getValue()) / 1000
         << "\n";
  stream << "KLEE: done:     executeOnNode = "
         << ((double)executeOnNodeTime.getValue()) / 1000 << "\n";
}

void ITree::printTableStat(std::stringstream &stream) {
  SubsumptionTableEntry::printStat(stream);

  stream
      << "KLEE: done:     Average table entries per subsumption checkpoint = "
      << inTwoDecimalPoints(entryNumber / programPointNumber) << "\n";

  stream << "KLEE: done:     Number of subsumption checks = "
         << subsumptionCheckCount << "\n";

  stream << "KLEE: done:     Average solver calls per subsumption check = "
         << inTwoDecimalPoints((double)stats::subsumptionQueryCount /
                               (double)subsumptionCheckCount) << "\n";
}

std::string ITree::inTwoDecimalPoints(const double n) {
  std::ostringstream stream;
  unsigned long x = (unsigned)((n - ((unsigned)n)) * 100);
  unsigned y = (unsigned)n;
  stream << y << ".";
  if (x > 9)
    stream << x;
  else if (x > 0)
    stream << "0" << x;
  else
    stream << "00";
  return stream.str();
}

std::string ITree::getInterpolationStat() {
  std::stringstream stream;
  stream << "\nKLEE: done: Subsumption statistics\n";
  printTableStat(stream);
  stream << "KLEE: done: ITree method execution times (ms):\n";
  printTimeStat(stream);
  stream << "KLEE: done: ITreeNode method execution times (ms):\n";
  ITreeNode::printTimeStat(stream);
  return stream.str();
}

ITree::ITree(ExecutionState *_root, llvm::DataLayout *_targetData)
    : targetData(_targetData) {
  currentINode = 0;
  assert(_targetData && "target data layout not provided");
  if (!_root->itreeNode) {
    currentINode = new ITreeNode(0, _targetData);
  }
  root = currentINode;
}

ITree::~ITree() {
  for (std::map<uintptr_t, std::deque<SubsumptionTableEntry *> >::const_iterator
           it = subsumptionTable.begin(),
           ie = subsumptionTable.end();
       it != ie; ++it) {
    if (!it->second.empty()) {
      entryNumber += it->second.size();
      ++programPointNumber;
    }
  }

  for (std::map<uintptr_t, std::deque<SubsumptionTableEntry *> >::iterator
           it = subsumptionTable.begin(),
           ie = subsumptionTable.end();
       it != ie; ++it) {
    for (std::deque<SubsumptionTableEntry *>::iterator it1 = it->second.begin(),
                                                       ie1 = it->second.end();
         it1 != ie1; ++it1) {
      delete *it1;
    }
    it->second.clear();
  }

  subsumptionTable.clear();
}

bool ITree::subsumptionCheck(TimingSolver *solver, ExecutionState &state,
                             double timeout) {
  assert(state.itreeNode == currentINode);

  // Immediately return if the state's instruction is not the
  // the interpolation node id. The interpolation node id is the
  // first instruction executed of the sequence executed for a state
  // node, typically this the first instruction of a basic block.
  // Subsumption check only matches against this first instruction.
  if (!state.itreeNode || reinterpret_cast<uintptr_t>(state.pc->inst) !=
                              state.itreeNode->getNodeId())
    return false;

  ++subsumptionCheckCount; // For profiling

  TimerStatIncrementer t(subsumptionCheckTime);
  std::deque<SubsumptionTableEntry *> entryList =
      subsumptionTable[state.itreeNode->getNodeId()];

  if (entryList.empty())
    return false;

  std::pair<Dependency::ConcreteStore, Dependency::SymbolicStore>
  storedExpressions = state.itreeNode->getStoredExpressions();

  // Iterate the subsumption table entry with reverse iterator because
  // the successful subsumption mostly happen in the newest entry.
  for (std::deque<SubsumptionTableEntry *>::reverse_iterator
           it = entryList.rbegin(),
           ie = entryList.rend();
       it != ie; ++it) {
    if ((*it)->subsumed(solver, state, timeout, storedExpressions)) {
      // We mark as subsumed such that the node will not be
      // stored into table (the table already contains a more
      // general entry).
      currentINode->isSubsumed = true;

      // Mark the node as subsumed, and create a subsumption edge
      SearchTree::markAsSubsumed(currentINode, (*it));
      return true;
    }
  }
  return false;
}

void ITree::store(SubsumptionTableEntry *subItem) {
    subsumptionTable[subItem->nodeId].push_back(subItem);
#ifdef ENABLE_Z3
    if (MaxFailSubsumption > 0 &&
        (unsigned)MaxFailSubsumption <
            subsumptionTable[subItem->nodeId].size()) {
      subsumptionTable[subItem->nodeId].pop_front();
    }
#endif
}

void ITree::setCurrentINode(ExecutionState &state) {
  TimerStatIncrementer t(setCurrentINodeTime);
  currentINode = state.itreeNode;
  currentINode->setNodeLocation(state.pc->inst);
  SearchTree::setCurrentNode(state, currentINode->getNodeId());
}

void ITree::remove(ITreeNode *node) {
  TimerStatIncrementer t(removeTime);
  assert(!node->left && !node->right);
  do {
    ITreeNode *p = node->parent;

    // As the node is about to be deleted, it must have been completely
    // traversed, hence the correct time to table the interpolant.
    if (!node->isSubsumed && node->storable) {
      SubsumptionTableEntry *entry = new SubsumptionTableEntry(node);
      store(entry);
      SearchTree::addTableEntryMapping(node, entry);
    }

    delete node;
    if (p) {
      if (node == p->left) {
        p->left = 0;
      } else {
        assert(node == p->right);
        p->right = 0;
      }
    }
    node = p;
  } while (node && !node->left && !node->right);
}

std::pair<ITreeNode *, ITreeNode *>
ITree::split(ITreeNode *parent, ExecutionState *left, ExecutionState *right) {
  TimerStatIncrementer t(splitTime);
  parent->split(left, right);
  SearchTree::addChildren(parent, parent->left, parent->right);
  std::pair<ITreeNode *, ITreeNode *> ret(parent->left, parent->right);
  return ret;
}

void ITree::markPathCondition(ExecutionState &state, TimingSolver *solver) {
  TimerStatIncrementer t(markPathConditionTime);
  std::vector<ref<Expr> > unsatCore = solver->getUnsatCore();

  llvm::BranchInst *binst =
      llvm::dyn_cast<llvm::BranchInst>(state.prevPC->inst);
  if (binst) {
    currentINode->dependency->markAllValues(binst->getCondition());
  }

  PathCondition *pc = currentINode->pathCondition;

  if (pc != 0) {
    for (std::vector<ref<Expr> >::iterator it = unsatCore.begin(),
                                           ie = unsatCore.end();
         it != ie; ++it) {
      for (; pc != 0; pc = pc->cdr()) {
        if (pc->car().compare(it->get()) == 0) {
          pc->setAsCore();
          pc = pc->cdr();
          break;
        }
      }
    }
  }
}

void ITree::execute(llvm::Instruction *instr) {
  std::vector<ref<Expr> > dummyArgs;
  executeOnNode(currentINode, instr, dummyArgs);
}

void ITree::execute(llvm::Instruction *instr, ref<Expr> arg1) {
  std::vector<ref<Expr> > args;
  args.push_back(arg1);
  executeOnNode(currentINode, instr, args);
}

void ITree::execute(llvm::Instruction *instr, ref<Expr> arg1, ref<Expr> arg2) {
  std::vector<ref<Expr> > args;
  args.push_back(arg1);
  args.push_back(arg2);
  executeOnNode(currentINode, instr, args);
}

void ITree::execute(llvm::Instruction *instr, ref<Expr> arg1, ref<Expr> arg2,
                    ref<Expr> arg3) {
  std::vector<ref<Expr> > args;
  args.push_back(arg1);
  args.push_back(arg2);
  args.push_back(arg3);
  executeOnNode(currentINode, instr, args);
}

void ITree::execute(llvm::Instruction *instr, std::vector<ref<Expr> > &args) {
  executeOnNode(currentINode, instr, args);
}

void ITree::executePHI(llvm::Instruction *instr, unsigned int incomingBlock,
                       ref<Expr> valueExpr) {
  currentINode->dependency->executePHI(instr, incomingBlock, valueExpr,
                                       symbolicExecutionError);
  symbolicExecutionError = false;
}

void ITree::executeOnNode(ITreeNode *node, llvm::Instruction *instr,
                          std::vector<ref<Expr> > &args) {
  TimerStatIncrementer t(executeOnNodeTime);
  node->execute(instr, args, symbolicExecutionError);
  symbolicExecutionError = false;
}

void ITree::printNode(llvm::raw_ostream &stream, ITreeNode *n,
                      std::string edges) const {
  if (n->left != 0) {
    stream << "\n";
    stream << edges << "+-- L:" << n->left->nodeId;
    if (this->currentINode == n->left) {
      stream << " (active)";
    }
    if (n->right != 0) {
      printNode(stream, n->left, edges + "|   ");
    } else {
      printNode(stream, n->left, edges + "    ");
    }
  }
  if (n->right != 0) {
    stream << "\n";
    stream << edges << "+-- R:" << n->right->nodeId;
    if (this->currentINode == n->right) {
      stream << " (active)";
    }
    printNode(stream, n->right, edges + "    ");
  }
}

void ITree::print(llvm::raw_ostream &stream) const {
  stream << "------------------------- ITree Structure "
            "---------------------------\n";
  stream << this->root->nodeId;
  if (this->root == this->currentINode) {
    stream << " (active)";
  }
  this->printNode(stream, this->root, "");
  stream << "\n------------------------- Subsumption Table "
            "-------------------------\n";
  for (std::map<uintptr_t, std::deque<SubsumptionTableEntry *> >::const_iterator
           it = subsumptionTable.begin(),
           ie = subsumptionTable.end();
       it != ie; ++it) {
    for (std::deque<SubsumptionTableEntry *>::const_iterator
             it1 = it->second.begin(),
             ie1 = it->second.end();
         it1 != ie1; ++it1) {
      (*it1)->print(stream);
    }
  }
}

void ITree::dump() const { this->print(llvm::errs()); }

/**/

// Statistics
Statistic ITreeNode::getInterpolantTime("GetInterpolantTime",
                                        "GetInterpolantTime");
Statistic ITreeNode::addConstraintTime("AddConstraintTime",
                                       "AddConstraintTime");
Statistic ITreeNode::splitTime("SplitTime", "SplitTime");
Statistic ITreeNode::executeTime("ExecuteTime", "ExecuteTime");
Statistic ITreeNode::bindCallArgumentsTime("BindCallArgumentsTime",
                                           "BindCallArgumentsTime");
Statistic ITreeNode::bindReturnValueTime("BindReturnValueTime",
                                         "BindReturnValueTime");
Statistic ITreeNode::getStoredExpressionsTime("GetStoredExpressionsTime",
                                              "GetStoredExpressionsTime");
Statistic
ITreeNode::getStoredCoreExpressionsTime("GetStoredCoreExpressionsTime",
                                        "GetStoredCoreExpressionsTime");

void ITreeNode::printTimeStat(std::stringstream &stream) {
  stream << "KLEE: done:     getInterpolant = "
         << ((double)getInterpolantTime.getValue()) / 1000 << "\n";
  stream << "KLEE: done:     addConstraintTime = "
         << ((double)addConstraintTime.getValue()) / 1000 << "\n";
  stream << "KLEE: done:     splitTime = " << ((double)splitTime.getValue()) /
                                                  1000 << "\n";
  stream << "KLEE: done:     execute = " << ((double)executeTime.getValue()) /
                                                1000 << "\n";
  stream << "KLEE: done:     bindCallArguments = "
         << ((double)bindCallArgumentsTime.getValue()) / 1000 << "\n";
  stream << "KLEE: done:     bindReturnValue = "
         << ((double)bindReturnValueTime.getValue()) / 1000 << "\n";
  stream << "KLEE: done:     getStoredExpressions = "
         << ((double)getStoredExpressionsTime.getValue()) / 1000 << "\n";
  stream << "KLEE: done:     getStoredCoreExpressions = "
         << ((double)getStoredCoreExpressionsTime.getValue()) / 1000 << "\n";
}

ITreeNode::ITreeNode(ITreeNode *_parent, llvm::DataLayout *_targetData)
    : parent(_parent), left(0), right(0), nodeId(0), isSubsumed(false),
      storable(true), graph(_parent ? _parent->graph : 0),
      instructionsDepth(_parent ? _parent->instructionsDepth : 0),
      targetData(_targetData) {

  pathCondition = (_parent != 0) ? _parent->pathCondition : 0;

  // Inherit the abstract dependency or NULL
  dependency = new Dependency(_parent ? _parent->dependency : 0, _targetData);
}

ITreeNode::~ITreeNode() {
  // Only delete the path condition if it's not
  // also the parent's path condition
  PathCondition *ie = parent ? parent->pathCondition : 0;

  PathCondition *it = pathCondition;
  while (it != ie) {
    PathCondition *tmp = it;
    it = it->cdr();
    delete tmp;
  }

  if (dependency)
    delete dependency;
}

uintptr_t ITreeNode::getNodeId() { return nodeId; }

ref<Expr>
ITreeNode::getInterpolant(std::set<const Array *> &replacements) const {
  TimerStatIncrementer t(getInterpolantTime);
  ref<Expr> expr = this->pathCondition->packInterpolant(replacements);
  return expr;
}

void ITreeNode::addConstraint(ref<Expr> &constraint, llvm::Value *condition) {
  TimerStatIncrementer t(addConstraintTime);
  pathCondition =
      new PathCondition(constraint, dependency, condition, pathCondition);
  graph->addPathCondition(this, pathCondition, constraint);
}

void ITreeNode::split(ExecutionState *leftData, ExecutionState *rightData) {
  TimerStatIncrementer t(splitTime);
  assert(left == 0 && right == 0);
  leftData->itreeNode = left = new ITreeNode(this, targetData);
  rightData->itreeNode = right = new ITreeNode(this, targetData);
}

void ITreeNode::execute(llvm::Instruction *instr, std::vector<ref<Expr> > &args,
                        bool symbolicExecutionError) {
  TimerStatIncrementer t(executeTime);
  dependency->execute(instr, args, symbolicExecutionError);
}

void ITreeNode::bindCallArguments(llvm::Instruction *site,
                                  std::vector<ref<Expr> > &arguments) {
  TimerStatIncrementer t(bindCallArgumentsTime);
  dependency->bindCallArguments(site, arguments);
}

void ITreeNode::bindReturnValue(llvm::CallInst *site, llvm::Instruction *inst,
                                ref<Expr> returnValue) {
  // TODO: This is probably where we should simplify
  // the dependency graph by removing callee values.
  TimerStatIncrementer t(bindReturnValueTime);
  dependency->bindReturnValue(site, inst, returnValue);
}

std::pair<Dependency::ConcreteStore, Dependency::SymbolicStore>
ITreeNode::getStoredExpressions() const {
  TimerStatIncrementer t(getStoredExpressionsTime);
  std::pair<Dependency::ConcreteStore, Dependency::SymbolicStore> ret;
  std::set<const Array *> dummyReplacements;

  // Since a program point index is a first statement in a basic block,
  // the allocations to be stored in subsumption table should be obtained
  // from the parent node.
  if (parent)
    ret = parent->dependency->getStoredExpressions(dummyReplacements, false);
  return ret;
}

std::pair<Dependency::ConcreteStore, Dependency::SymbolicStore>
ITreeNode::getStoredCoreExpressions(std::set<const Array *> &replacements)
    const {
  TimerStatIncrementer t(getStoredCoreExpressionsTime);
  std::pair<Dependency::ConcreteStore, Dependency::SymbolicStore> ret;

  // Since a program point index is a first statement in a basic block,
  // the allocations to be stored in subsumption table should be obtained
  // from the parent node.
  if (parent)
    ret = parent->dependency->getStoredExpressions(replacements, true);
  return ret;
}

unsigned ITreeNode::getInstructionsDepth() { return instructionsDepth; }

void ITreeNode::incInstructionsDepth() { ++instructionsDepth; }

void ITreeNode::unsatCoreMarking(std::vector<ref<Expr> > unsatCore) {
  // State subsumed, we mark needed constraints on the path condition. We create
  // path condition marking structure to mark core constraints
  std::map<Expr *, PathCondition *> markerMap;
  for (PathCondition *it = pathCondition; it != 0; it = it->cdr()) {
    if (llvm::isa<OrExpr>(it->car())) {
      // FIXME: Break up disjunction into its components, because each disjunct
      // is solved separately. The or constraint was due to state merge.
      // Hence, the following is just a makeshift for when state merge is
      // properly implemented.
      markerMap[it->car()->getKid(0).get()] = it;
      markerMap[it->car()->getKid(1).get()] = it;
    }
    markerMap[it->car().get()] = it;
  }

  for (std::vector<ref<Expr> >::iterator it1 = unsatCore.begin(),
                                         ie1 = unsatCore.end();
       it1 != ie1; ++it1) {
    // FIXME: Sometimes some constraints are not in the PC. This is
    // because constraints are not properly added at state merge.
    PathCondition *cond = markerMap[it1->get()];
    if (cond)
      cond->setAsCore();
  }
}

void ITreeNode::dump() const {
  llvm::errs() << "------------------------- ITree Node "
                  "--------------------------------\n";
  this->print(llvm::errs());
  llvm::errs() << "\n";
}

void ITreeNode::print(llvm::raw_ostream &stream) const {
  this->print(stream, 0);
}

void ITreeNode::print(llvm::raw_ostream &stream,
                      const unsigned paddingAmount) const {
  std::string tabs = makeTabs(paddingAmount);
  std::string tabsNext = appendTab(tabs);

  stream << tabs << "ITreeNode\n";
  stream << tabsNext << "node Id = " << nodeId << "\n";
  stream << tabsNext << "pathCondition = ";
  if (pathCondition == 0) {
    stream << "NULL";
  } else {
    pathCondition->print(stream);
  }
  stream << "\n";
  stream << tabsNext << "Left:\n";
  if (!left) {
    stream << tabsNext << "NULL\n";
  } else {
    left->print(stream, paddingAmount + 1);
    stream << "\n";
  }
  stream << tabsNext << "Right:\n";
  if (!right) {
    stream << tabsNext << "NULL\n";
  } else {
    right->print(stream, paddingAmount + 1);
    stream << "\n";
  }
  if (dependency) {
    stream << tabsNext << "------- Abstract Dependencies ----------\n";
    dependency->print(stream, paddingAmount + 1);
  }
}
