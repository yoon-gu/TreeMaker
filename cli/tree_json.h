// Build a tmTree from origami-lab tree JSON:
// {"paper":{"width":W,"height":H},"nodes":[{"id":1,"x":..,"y":..}],"edges":[{"n1":1,"n2":2,"length":1.0}]}
// Node ids are stored as node labels so they survive the .tmd5 round trip.
#ifndef TREE_JSON_H
#define TREE_JSON_H

#include <string>

class tmTree;

// Returns nullptr and sets err on invalid input.
tmTree* TreeFromJson(const std::string& path, std::string& err);

#endif
