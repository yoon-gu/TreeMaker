#include "fold_export.h"

#include <cmath>
#include <fstream>
#include <iterator>
#include <map>

#include "nlohmann/json.hpp"
#include "tmModel.h"

namespace {

const char* kStatus[] = {"HAS_FULL_CP",      "EDGES_TOO_SHORT",     "POLYS_NOT_VALID",
                         "POLYS_NOT_FILLED", "POLYS_MULTIPLE_IBPS", "VERTICES_LACK_DEPTH",
                         "FACETS_NOT_VALID", "NOT_LOCAL_ROOT_CONNECTABLE"};  // tmTree::CPStatus

}  // namespace

std::string WriteFold(tmTree* t, const std::string& path) {
  t->BuildPolysAndCreasePattern();
  tmArray<tmEdge*> be;
  tmArray<tmPoly*> bp;
  tmArray<tmVertex*> bv;
  tmArray<tmCrease*> bc;
  tmArray<tmFacet*> bf;
  std::string status = kStatus[t->GetCPStatus(be, bp, bv, bc, bf)];

  nlohmann::json coords = nlohmann::json::array(), edges = nlohmann::json::array(),
                 assign = nlohmann::json::array(), angles = nlohmann::json::array();
  std::map<const tmVertex*, size_t> index;
  auto vid = [&](const tmVertex* v) {
    auto it = index.find(v);
    if (it != index.end()) return it->second;
    coords.push_back({v->GetLoc().x, v->GetLoc().y});
    return index[v] = coords.size() - 1;
  };
  auto add = [&](size_t a, size_t b, const char* asg, int angle) {
    edges.push_back({a, b});
    assign.push_back(asg);
    angles.push_back(angle);
  };
  // Which paper sides (bitmask) a point lies on, and its position along the
  // perimeter (counter-clockwise from (0,0)), or -1 if it is not on the edge.
  double w = t->GetPaperWidth(), h = t->GetPaperHeight(), tol = tmPart::DistTol();
  auto sides = [&](const tmPoint& p) {
    return (std::fabs(p.y) < tol) | (std::fabs(p.x - w) < tol) << 1 |
           (std::fabs(p.y - h) < tol) << 2 | (std::fabs(p.x) < tol) << 3;
  };
  auto perim = [&](double x, double y) {
    if (std::fabs(y) < tol) return x;
    if (std::fabs(x - w) < tol) return w + y;
    if (std::fabs(y - h) < tol) return w + h + (w - x);
    if (std::fabs(x) < tol) return 2 * w + h + (h - y);
    return -1.0;
  };

  // TreeMaker BORDER creases on a paper side are replaced by the paper border
  // below; the ones inside the paper (convex hull of the tree) have no fold
  // assigned, so they become "U".
  static const char* kAsg[] = {"F", "M", "V", "U"};  // indexed by tmCrease::Fold
  static const int kAngle[] = {0, -180, 180, 0};
  for (size_t i = 0; i < t->GetCreases().size(); ++i) {
    const tmCrease* c = t->GetCreases()[i];
    const tmVertex *a = c->GetFrontVertex(), *b = c->GetBackVertex();
    int f = c->GetFold();
    if (f == tmCrease::BORDER && (sides(a->GetLoc()) & sides(b->GetLoc()))) continue;
    add(vid(a), vid(b), kAsg[f], kAngle[f]);
  }

  // Paper border: the 4 sides, split at every crease vertex lying on them.
  std::map<double, size_t> ring;
  for (size_t i = 0; i < coords.size(); ++i) {
    double s = perim(coords[i][0].get<double>(), coords[i][1].get<double>());
    if (s >= 0) ring.emplace(s, i);
  }
  for (auto [x, y] : {std::pair{0.0, 0.0}, {w, 0.0}, {w, h}, {0.0, h}}) {
    double s = perim(x, y);
    auto it = ring.lower_bound(s - tol);
    if (it != ring.end() && it->first <= s + tol) continue;
    coords.push_back({x, y});
    ring.emplace(s, coords.size() - 1);
  }
  for (auto it = ring.begin(); it != ring.end(); ++it) {
    auto next = std::next(it) == ring.end() ? ring.begin() : std::next(it);
    add(it->second, next->second, "B", 0);
  }

  nlohmann::json fold = {{"file_spec", 1.1},
                         {"file_creator", "treemaker-cli"},
                         {"frame_classes", {"creasePattern"}},
                         {"vertices_coords", coords},
                         {"edges_vertices", edges},
                         {"edges_assignment", assign},
                         {"edges_foldAngle", angles}};
  std::ofstream out(path);
  out << fold.dump() << "\n";
  return out ? status : "";
}
