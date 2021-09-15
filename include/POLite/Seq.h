// SPDX-License-Identifier: BSD-2-Clause
// Sequence data type

#ifndef _SEQ_H_
#define _SEQ_H_

#include <stdlib.h>
#include <assert.h>

template <class T> class Seq
{
  private:
    // Initialisation
    void init(int initialSize)
    {
      maxElems = initialSize;
      numElems = 0;
      elems    = new T[initialSize];
    }

  public:
    unsigned maxElems;
    unsigned numElems;
    T* elems;

    // Constructors
    Seq() { init(4096); }
    Seq(int initialSize) { init(initialSize); }

    // Copy constructor
    Seq(const Seq<T>& seq) {
      init(seq.maxElems);
      numElems = seq.numElems;
      for (int i = 0; i < seq.numElems; i++)
        elems[i] = seq.elems[i];
    }

    // Set capacity of sequence
    void setCapacity(int n) {
      maxElems = n;
      T* newElems = new T[maxElems];
      for (unsigned i = 0; i < std::min<unsigned>(n, numElems); i++)
        newElems[i] = elems[i];
      delete [] elems;
      elems = newElems;
    }

    // Extend size of sequence by N
    void extendBy(int n)
    {
      unsigned newNumElems = numElems + n;
      if (newNumElems > maxElems)
        setCapacity(newNumElems*2);
      assert(newNumElems <= maxElems);
      numElems=newNumElems;
    }

    // Extend size of sequence by one
    void extend()
    {
      extendBy(1);
    }

    // Ensure space for a further N elements
    void ensureSpaceFor(unsigned n)
    {
      unsigned newNumElems = numElems + n;
      if (newNumElems > maxElems)
        setCapacity(newNumElems*2);
    }

    // Append
    void append(T x)
    {
      extend();
      elems[numElems-1] = x;
    }

    // Delete last element
    void deleteLast()
    {
      numElems--;
    }

    // Push
    void push(T x) { append(x); }

    // Pop
    T pop() {
      numElems--;
      return elems[numElems];
    }

    // Clear the sequence
    void clear()
    {
      numElems = 0;
    }

    // Is given value already in sequence?
    bool member(T x) {
      for (unsigned i = 0; i < numElems; i++)
        if (elems[i] == x) return true;
      return false;
    }

    // Insert element into sequence if not already present
    bool insert(T x) {
      bool alreadyPresent = member(x);
      if (!alreadyPresent) append(x);
      return !alreadyPresent;
    }

    // Remove an element from a sequence
    void remove(T x) {
      for (int i = 0; i < numElems; i++)
        if (elems[i] == x) {
          for (int j = i; j < numElems-1; j++)
            elems[j] = elems[j+1];
          numElems--;
          return;
        }
    }

    // Destructor
    ~Seq()
    {
      delete [] elems;
    }
};

// A small sequence is just a sequence with a small initial size
template <class T> class SmallSeq : public Seq<T> {
  public:
    SmallSeq() : Seq<T>(8) {};
};

#endif
