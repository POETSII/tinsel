APPNAME=heat-gals

# TODO : Not sure what form the edges file should take for this app.
function setup_file {
    mkdir -p $BATS_TMPDIR/heat-gals
    xzcat $BATS_TEST_DIRNAME/../asp-pc/hypercube.medium.edges.xz >  $BATS_TMPDIR/heat-gals/hypercube.medium.edges
    xzcat $BATS_TEST_DIRNAME/../asp-pc/hypercube.large.edges.xz >  $BATS_TMPDIR/heat-gals/hypercube.large.edges
}

@test "heat-gals-compile-hw" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make hw)
}

@test "heat-gals-compile-sim" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make sim)
}

@test "heat-gals-run-medium-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim $BATS_TMPDIR/heat-gals/hypercube.medium.edges)
}

@test "heat-gals-run-large-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim $BATS_TMPDIR/heat-gals/hypercube.large.edges)
}

@test "heat-gals-run-medium-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && ./run $BATS_TMPDIR/heat-gals/hypercube.medium.edges)
}

@test "heat-gals-run-large-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && ./run $BATS_TMPDIR/heat-gals/hypercube.large.edges)
}
