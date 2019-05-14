// SPDX-License-Identifier: BSD-2-Clause
// Pseudo Least Recently Used (PLRU) Cache Replacement Policy
// ==========================================================

import List        :: *;
import Globals     :: *;
import DCacheTypes :: *;

// PLRU state
typedef TSub#(TExp#(`DCacheLogNumWays), 1) NumPLRUBits;
typedef Bit#(NumPLRUBits) PLRUState;

// Split a list into two halves
function Tuple2#(List#(t), List#(t)) halve(List#(t) xs);
  Integer n = List::length(xs) / 2;
  return tuple2(List::take(n, xs), List::drop(n, xs));
endfunction

// Determine way based on PLRU state (untyped)
function List#(Bit#(1)) plruUntyped(List#(Bit#(1)) xs);
  Integer n = List::length(xs);
  if (n <= 1)
    return xs;
  else begin
   match {.a, .b} = halve(List::tail(xs));
   let as = plruUntyped(a);
   let bs = plruUntyped(b);
   function mux(x, y) = List::head(xs) == 1 ? y : x;
   return Cons(List::head(xs), zipWith(mux, as, bs));
  end
endfunction

// Convert list to way
function Way listToWay(List#(Bit#(1)) xs);
  Way w;
  for (Integer i = 0; i < `DCacheLogNumWays; i=i+1)
    w[i] = xs[i];
  return w;
endfunction

// Convert PLRU state to list
function List#(Bit#(1)) plruToList(PLRUState s);
  List#(Bit#(1)) list = Nil;
  for (Integer i = valueOf(NumPLRUBits)-1; i >= 0; i=i-1)
    list = Cons(s[i], list);
  return list;
endfunction

// Determine way based on PLRU state (typed)
function Way plru(PLRUState s) =
  listToWay(List::reverse(plruUntyped(plruToList(s))));

// Compute next PLRU state when given way is accessed (untyped)
function List#(Bit#(1)) plruNextUntyped(List#(Bit#(1)) w, List#(Bit#(1)) s);
  Integer n = List::length(w);
  if (n == 0)
    return Nil;
  else begin
    Bit#(1) s0 = List::head(s);
    Bit#(1) w0 = List::head(w);
    match {.a, .b} = halve(List::tail(s));
    let as = plruNextUntyped(List::tail(w), a);
    let bs = plruNextUntyped(List::tail(w), b);
    Bit#(1) msb = s0 == w0 ? ~s0 : s0;
    function choose(sel, x, y) = sel == 1 ? x : y;
    return Cons(msb, List::append(
      List::zipWith(choose(~w0), as, a),
      List::zipWith(choose(w0), bs, b)));
  end
endfunction

// Convert way to list
function List#(Bit#(1)) wayToList(Way w);
  List#(Bit#(1)) list = Nil;
  for (Integer i = `DCacheLogNumWays-1; i >= 0; i=i-1)
    list = Cons(w[i], list);
  return list;
endfunction

// Convert list to PLRU state
function PLRUState listToPLRU(List#(Bit#(1)) xs);
  PLRUState s;
  for (Integer i = 0; i < valueOf(NumPLRUBits); i=i+1)
    s[i] = xs[i];
  return s;
endfunction

// Compute next PLRU state when given way is accessed (typed)
function PLRUState plruNext(Way w, PLRUState s) =
  listToPLRU(plruNextUntyped(List::reverse(wayToList(w)), plruToList(s)));
