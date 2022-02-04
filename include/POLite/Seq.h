// SPDX-License-Identifier: BSD-2-Clause
// Sequence data type

#ifndef _SEQ_H_
#define _SEQ_H_

#include <stdlib.h>
#include <assert.h>
#include <cstring>

#include <utility>
#include <algorithm>
#include <mutex>

#include <POLite/SpinLock.h>

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
    T* elems;
    int maxElems;
    int numElems;
    SpinLock lock;

    // Constructors
    Seq() {
      maxElems=0;
      numElems=0;
      elems=nullptr;
    }
    Seq(int initialSize) { init(initialSize); }

    // Copy constructor
    /*Seq(const Seq<T>& seq) {
      init(seq.maxElems);
      numElems = seq.numElems;
      std::copy(seq.elems, seq.elems+seq.numElems, elems);
      //for (int i = 0; i < seq.numElems; i++)
      //  elems[i] = seq.elems[i];
    }*/

    Seq(const Seq &) = delete;

    Seq(Seq<T>&& seq) {
      maxElems=seq.maxElems;
      numElems=seq.numElems;
      elems=seq.elems;
      seq.maxElems=0;
      seq.numElems=0;
      seq.elems=nullptr;
    }

    Seq &operator=(const Seq &seq) = delete;

    Seq &operator=(Seq &&seq)
    {
      if(this!=&seq){
        if(elems){
          delete []elems;
        }
        maxElems=seq.maxElems;
        numElems=seq.numElems;
        elems=seq.elems;
        seq.maxElems=0;
        seq.numElems=0;
        seq.elems=nullptr;
      }
      return *this;
    }

    // Set capacity of sequence
    void setCapacity(int n) {
      assert(numElems <= n);
      maxElems = n;
      T* newElems = new T[maxElems];
      std::move(elems, elems+numElems, newElems);
      delete [] elems;
      elems = newElems;
    }

    // Extend size of sequence by N
    void extendBy(unsigned n)
    {
      int newNumElems = numElems + n;
      if (newNumElems  > maxElems){
        setCapacity(std::max(newNumElems, maxElems*2));
      }
      numElems=newNumElems;
    }

    void extendByWithZero(int n)
    {
      int newNumElems = numElems + n;
      if (newNumElems  > maxElems){
        setCapacity(std::max(newNumElems, maxElems*2));
      }
      std::memset(elems+numElems, 0, sizeof(T)*(newNumElems-numElems));
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
      int newSpace = numElems + n;
      if (newSpace > maxElems)
        setCapacity(std::max(newSpace, 2*maxElems));
    }

    // Append
    //! \retval The number of items in the Seq after the append
    size_t append(T &&x)
    {
      if(numElems==maxElems){
        setCapacity(std::max(maxElems*2, 16));
      }
      elems[numElems++] = std::move(x);
      return numElems;
    }

    // Append
    //! \retval The number of items in the Seq after the append
    size_t append(const T &x)
    {
      if(numElems==maxElems){
        setCapacity(std::max(maxElems*2, 16));
      }
      elems[numElems++] = x;
      return numElems;
    }

    template<bool DoLock=true>
    size_t append_locked(const T &x)
    {
      SpinLockGuard<DoLock> lk(lock);
      return append(x);
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

    // Clear the sequence, without freeing memory
    void clear()
    {
      numElems = 0;
    }

    // Is given value already in sequence?
    bool member(T x) const {
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
      for (int i = 0; i < numElems; i++){
        if (elems[i] == x) {
          std::move(elems+1,elems+numElems, elems);
          // (Ignore any mention of bugs. dt10 does not read code properly)
          numElems--;
          return;
        }
      }
    }

    // Destructor
    ~Seq()
    {
      if(elems){
        delete [] elems;
        elems=0;
      }
      numElems=0;
      maxElems=0;
    }

    const T &operator[](size_t i) const
    {
      assert(i<size_t(numElems));
      return elems[i];
    }

    T &operator[](size_t i)
    {
      assert(i<size_t(numElems));
      return elems[i];
    }

    size_t size() const
    { return numElems; }

    const T *begin() const
    { return elems; }

    const T *end() const
    { return elems+numElems; }

    T *begin()
    { return elems; }

    T *end()
    { return elems+numElems; }
};

// A small sequence is just a sequence with a small initial size
template <class T> class SmallSeq : public Seq<T> {
  public:
    SmallSeq() : Seq<T>(8) {};
};

#endif
