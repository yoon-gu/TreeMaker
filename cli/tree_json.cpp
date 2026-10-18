#include "tree_json.h"

#include <cmath>
#include <fstream>
#include <map>
#include <queue>
#include <vector>

#include "nlohmann/json.hpp"
#include "tmModel.h"

using nlohmann::json;

namespace {

struct Adj {
  int other;
  double length;
};

std::string Validate(const std::map<int, std::vector<Adj>>& adj, size_t numEdges) {
  if (adj.size() < 2) return "tree needs at least 2 nodes";
  if (numEdges != adj.size() - 1) return "edges must form a tree (expected nodes-1 edges)";
  for (const auto& [id, nbrs] : adj) {
    if (nbrs.size() == 2) return "degree-2 node " + std::to_string(id);
    for (const Adj& a : nbrs)
      if (!(a.length > 0)) return "edge length must be > 0";
  }
  std::map<int, bool> seen{{adj.begin()->first, true}};
  std::queue<int> q;
  q.push(adj.begin()->first);
  while (!q.empty()) {
    int n = q.front();
    q.pop();
    for (const Adj& a : adj.at(n))
      if (!seen[a.other]) seen[a.other] = true, q.push(a.other);
  }
  for (const auto& [id, nbrs] : adj)
    if (!seen[id]) return "tree is not connected (node " + std::to_string(id) + ")";
  return "";
}

}  // namespace

tmTree* TreeFromJson(const std::string& path, std::string& err) {
  json j;
  std::map<int, std::vector<Adj>> adj;
  std::map<int, tmPoint> loc;
  double w = 1.0, h = 1.0;
  try {
    std::ifstream in(path);
    if (!in) {
      err = "cannot read " + path;
      return nullptr;
    }
    j = json::parse(in);
    if (j.contains("paper")) {
      w = j["paper"].value("width", 1.0);
      h = j["paper"].value("height", 1.0);
    }
    if (!(w > 0 && h > 0)) {
      err = "paper width/height must be > 0";
      return nullptr;
    }
    const json& nodes = j.at("nodes");
    for (size_t k = 0; k < nodes.size(); ++k) {
      int id = nodes[k].at("id").get<int>();
      if (adj.count(id)) {
        err = "duplicate node id " + std::to_string(id);
        return nullptr;
      }
      adj[id];
      double t = 2 * M_PI * k / nodes.size();  // circular layout fallback
      loc[id] = tmPoint(nodes[k].value("x", w * (0.5 + 0.4 * std::cos(t))),
                        nodes[k].value("y", h * (0.5 + 0.4 * std::sin(t))));
    }
    for (const json& e : j.at("edges")) {
      int a = e.at("n1").get<int>(), b = e.at("n2").get<int>();
      double len = e.at("length").get<double>();
      if (!adj.count(a) || !adj.count(b) || a == b) {
        err = "edge references unknown or same node";
        return nullptr;
      }
      adj[a].push_back({b, len});
      adj[b].push_back({a, len});
    }
    err = Validate(adj, j.at("edges").size());
  } catch (const json::exception& e) {
    err = std::string("bad tree JSON: ") + e.what();
  }
  if (!err.empty()) return nullptr;

  // Same construction sequence as tmTree::MakeTreeUnoptimized(): first node
  // from NULL, then each new node grown from an existing one.
  tmTree* tree = tmTree::MakeTreeBlank();
  tree->SetPaperWidth(w);
  tree->SetPaperHeight(h);
  std::map<int, tmNode*> made;
  int root = adj.begin()->first;
  tmEdge* edge = nullptr;
  tree->AddNode(nullptr, loc[root], made[root], edge);
  made[root]->SetLabel(std::to_string(root).c_str());
  std::queue<int> q;
  q.push(root);
  while (!q.empty()) {
    int n = q.front();
    q.pop();
    for (const Adj& a : adj[n]) {
      if (made.count(a.other)) continue;
      tree->AddNode(made[n], loc[a.other], made[a.other], edge);
      made[a.other]->SetLabel(std::to_string(a.other).c_str());
      edge->SetLength(a.length);
      q.push(a.other);
    }
  }
  return tree;
}
