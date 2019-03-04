#ifndef _BOX_CONFIG_H_
#define _BOX_CONFIG_H_

#include <vector>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  // String buffer for file reader
  // (Free these in destructor)
  std::vector< char* > words;

  // Load config from file
  void loadFromFile(const char* filename) {
    FILE* fp = fopen(filename, "rt");
    if (fp == NULL) {
      fprintf(stderr, "Can't open box config file '%s'\n", filename);
      exit(EXIT_FAILURE);
    }

    rows.clear();

    // Loop over lines
    char line[1024];
    int lineCount = 0;
    while (fgets(line, sizeof(line)-1, fp)) {
      std::vector<const char*> row;
      char* p = line;
      // Loop over words
      while (1) {
        // Ignore whitespace
        while (isspace(*p)) p++;
        char* word = p;
        while (isalnum(*p)) p++;
        bool endOfLine = *p == '\0';
        *p = '\0';
        int len = strlen(word);
        char* buffer = new char [len];
        strcpy(buffer, word);
        // Add word to box config
        row.push_back(buffer);
        words.push_back(buffer);
        if (endOfLine) break;
        p++;
      }
      // Check that the length of the new row is consistent
      if (row.size() > 0) {
        if (rows.size() > 0 && rows[0].size() != row.size()) {
          fprintf(stderr, "BoxConfig: all rows must be the same length!\n");
          exit(EXIT_FAILURE);
        }
        rows.insert(rows.begin(), row);
      }
    }
    
  }

  void display() {
    for (int i = 0; i < rows.size(); i++) {
      for (int j = 0; j < rows[i].size(); j++) {
        printf("%s ", rows[i][j]);
      }
      printf("\n");
    }
  }

  // Add a row of boxes to the config
  template <typename... Args> void addRow(Args... args) {
    std::vector<const char*> row;
    vec(row, args...);
    // Check that the length of the new row is consistent
    if (rows.size() > 0 && rows[0].size() != row.size()) {
      fprintf(stderr, "BoxConfig: all rows must be the same length!\n");
      exit(EXIT_FAILURE);
    }
    rows.insert(rows.begin(), row);
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
    return rows[lenY()-1][0];
  }

  // Destructor
  ~BoxConfig() {
    for (int i = 0; i < words.size(); i++)
      delete [] words[i];
  }
};

inline void defaultBoxConfig(BoxConfig* boxConfig)
{
  char* str = getenv("TINSEL_BOX_CONFIG");
  if (str) {
    boxConfig->loadFromFile(str);
  }
  else {
    boxConfig->rows.clear();
    boxConfig->addRow("localhost");
  }
}

#endif
