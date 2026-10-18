// treemaker-cli: headless front end to the TreeMaker model.
//   treemaker-cli convert  --in tree.json --out out.tmd5
//   treemaker-cli optimize --in <tree.json|in.tmd5> --optimizer scale|edge|strain
//                          --out out.tmd5 --cp out.fold [--paper W H]
// stdout: one JSON object on success; diagnostics go to stderr.
// Exit codes: 0 ok, 2 input error, 3 optimization failed.
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "fold_export.h"
#include "nlohmann/json.hpp"
#include "tmModel.h"
#include "tree_json.h"

namespace {

enum { OK = 0, INPUT_ERROR = 2, OPT_FAILED = 3 };

int Fail(int code, const std::string& msg) {
  std::cerr << "treemaker-cli: " << msg << std::endl;
  return code;
}

bool EndsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

tmTree* LoadTree(const std::string& path, std::string& err) {
  if (!EndsWith(path, ".tmd5")) return TreeFromJson(path, err);
  std::ifstream in(path);
  if (!in) {
    err = "cannot read " + path;
    return nullptr;
  }
  tmTree* tree = new tmTree();
  try {
    tree->GetSelf(in);
  } catch (...) {
    delete tree;
    err = "cannot parse " + path;
    return nullptr;
  }
  return tree;
}

bool SaveTree(tmTree* tree, const std::string& path) {
  std::ofstream out(path);
  tree->PutSelf(out);
  return bool(out);
}

// Same call pattern as Source/test/tmModelTester (Do*Optimization). Engine is
// ALM (TreeMaker's default): the bundled wnlib wrapper stops at its iteration
// cap and reports WN_SUBOPTIMAL with infeasible results on the upstream test
// trees. The optimizer is scoped so its tmTreeCleaner cleans up the tree before
// we return. Returns "" on success, else a failure message.
std::string Optimize(tmTree* tree, const std::string& kind) {
  tmNLCO_alm nlco;
  tmDpptrArray<tmNode> nodes = tree->GetOwnedNodes();
  tmDpptrArray<tmEdge> edges = tree->GetOwnedEdges();
  try {
    if (kind == "scale") {
      tmScaleOptimizer opt(tree, &nlco);
      opt.Initialize();
      opt.Optimize();
    } else if (kind == "edge") {
      tmEdgeOptimizer opt(tree, &nlco);
      opt.Initialize(nodes, edges);
      opt.Optimize();
    } else {
      tmStrainOptimizer opt(tree, &nlco);
      opt.Initialize(nodes, edges);
      opt.Optimize();
    }
  } catch (const tmNLCO::EX_BAD_CONVERGENCE& ex) {
    return kind + " optimization did not converge (reason " + std::to_string(ex.GetReason()) + ")";
  } catch (const tmScaleOptimizer::EX_BAD_SCALE&) {
    return "scale optimization failed: scale too small";
  } catch (const tmEdgeOptimizer::EX_NO_MOVING_NODES&) {
    return "edge optimization needs unpinned nodes";
  } catch (const tmEdgeOptimizer::EX_NO_MOVING_EDGES&) {
    return "edge optimization needs unpinned edges";
  } catch (const tmStrainOptimizer::EX_NO_MOVING_NODES_OR_EDGES&) {
    return "strain optimization needs unpinned nodes or edges";
  } catch (...) {
    return kind + " optimization failed";
  }
  return "";
}

int Run(int argc, char** argv) {
  if (argc < 2) return Fail(INPUT_ERROR, "usage: treemaker-cli convert|optimize --in ... --out ...");
  const std::string cmd = argv[1];
  std::map<std::string, std::string> opt;
  bool paper = false;
  double paperW = 0, paperH = 0;
  for (int i = 2; i < argc; ++i) {
    std::string key = argv[i];
    if (key == "--paper") {
      if (i + 2 >= argc) return Fail(INPUT_ERROR, "--paper needs W H");
      paper = true;
      paperW = std::atof(argv[++i]);
      paperH = std::atof(argv[++i]);
    } else if (key.rfind("--", 0) == 0 && i + 1 < argc) {
      opt[key.substr(2)] = argv[++i];
    } else {
      return Fail(INPUT_ERROR, "bad argument " + key);
    }
  }
  const std::string kind = opt.count("optimizer") ? opt["optimizer"] : "scale";
  if (cmd != "convert" && cmd != "optimize") return Fail(INPUT_ERROR, "unknown subcommand " + cmd);
  if (!opt.count("in") || !opt.count("out") || (cmd == "optimize" && !opt.count("cp")))
    return Fail(INPUT_ERROR, "missing --in/--out" + std::string(cmd == "optimize" ? "/--cp" : ""));
  if (kind != "scale" && kind != "edge" && kind != "strain")
    return Fail(INPUT_ERROR, "--optimizer must be scale|edge|strain");

  std::string err;
  tmTree* tree = LoadTree(opt["in"], err);
  if (!tree) return Fail(INPUT_ERROR, err);
  if (paper) {
    if (!(paperW > 0 && paperH > 0)) return Fail(INPUT_ERROR, "--paper W H must be positive");
    tree->SetPaperWidth(paperW);
    tree->SetPaperHeight(paperH);
  }

  nlohmann::json result;
  if (cmd == "convert") {
    result = {{"nodes", tree->GetNumNodes()}, {"edges", tree->GetNumEdges()}};
  } else {
    err = Optimize(tree, kind);
    if (!err.empty()) return Fail(OPT_FAILED, err);
    std::string status = WriteFold(tree, opt["cp"]);
    if (status.empty()) return Fail(INPUT_ERROR, "cannot write " + opt["cp"]);
    nlohmann::json leaves = nlohmann::json::array();
    for (size_t i = 0; i < tree->GetOwnedNodes().size(); ++i) {
      tmNode* n = tree->GetOwnedNodes()[i];
      if (!n->IsLeafNode()) continue;
      char* end;
      long id = std::strtol(n->GetLabel(), &end, 10);
      if (end == n->GetLabel() || *end) id = long(n->GetIndex());  // unlabeled .tmd5 node
      leaves.push_back({{"id", id}, {"x", n->GetLocX()}, {"y", n->GetLocY()}});
    }
    result = {{"scale", tree->GetScale()},
              {"feasible", tree->IsFeasible()},
              {"cp_status", status},
              {"leaves", leaves}};
  }
  if (!SaveTree(tree, opt["out"])) return Fail(INPUT_ERROR, "cannot write " + opt["out"]);
  std::cout << result.dump() << std::endl;
  return OK;
}

}  // namespace

int main(int argc, char** argv) {
  tmPart::InitTypes();
  return Run(argc, argv);
}
