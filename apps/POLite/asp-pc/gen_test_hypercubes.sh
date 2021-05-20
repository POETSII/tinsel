#!/bin/bash

# Used to create the pre-generated inputs for bats test. Otherwise
# we need ghc installed, which can be difficult to install as
# non-admin.

ghc  GenHypercube.hs -o GenHypercube
./GenHypercube 4 5 > hypercube.medium.edges
./GenHypercube 5 6  > hypercube.large.edges

xz hypercube.medium.edges
xz hypercube.large.edges
