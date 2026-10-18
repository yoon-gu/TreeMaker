// Build the crease pattern of a tmTree and write it as a FOLD file.
#ifndef FOLD_EXPORT_H
#define FOLD_EXPORT_H

#include <string>

class tmTree;

// Builds polys + crease pattern, writes FOLD (creases + paper border as "B",
// split at crease vertices) and returns the tmTree::CPStatus name, or "" if
// the file cannot be written.
std::string WriteFold(tmTree* t, const std::string& path);

#endif
