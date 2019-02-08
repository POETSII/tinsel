#ifndef _BOX_CONFIG_H_
#define _BOX_CONFIG_H_

#include <vector>

// Use variadic templates to make a function
//
//   vec(v, a, b, c, ...);
//
// which inserts values a, b, c, ... onto the front of vector v.

// Base case
template<typename T> inline void vec(std::vector<T> &acc, T v) {
  acc.insert(acc.begin(), v);
}

// Recusive case
template<typename T, typename... Args>
inline void vec(std::vector<T> &acc, T first, Args... args) {
  vec(acc, args...);
  acc.insert(acc.begin(), first);
}

// A class for creating a configuration of boxes
struct BoxConfig {
  // 2D vector of box names
  std::vector< std::vector<const char*> > rows;

  // Add a row of boxes to the config
  template <typename... Args> void addRow(Args... args) {
    std::vector<const char*> row;
    vec(row, args...);
    // Check that the length of the new row is consistent
    if (rows.size() > 0 && rows[0].size() != row.size()) {
      fprintf(stderr, "BoxConfig: all rows must be the same length!\n");
      exit(EXIT_FAILURE);
    }
    rows.push_back(row);
  }

  void requireNonEmpty() {
    if (rows.size() == 0 || rows[0].size() == 0) {
      fprintf(stderr, "BoxConfig: box mesh contains no boxes!\n");
      exit(EXIT_FAILURE);
    }
  }

  // X length of mesh
  int lenX() {
    requireNonEmpty();
    return rows[0].size();
  }

  // Y length of mesh
  int lenY() {
    requireNonEmpty();
    return rows.size();
  }

  // Determine name of the master box (the top-left box)
  const char* master() {
    requireNonEmpty();
    return rows[0][0];
  }
};

#endif
