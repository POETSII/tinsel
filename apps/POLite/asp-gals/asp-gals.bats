APPNAME=asp-gals

function setup_file {
    mkdir -p $BATS_TMPDIR/asp-gals
    xzcat $BATS_TEST_DIRNAME/../asp-pc/hypercube.medium.edges.xz >  $BATS_TMPDIR/asp-gals/hypercube.medium.edges
    xzcat $BATS_TEST_DIRNAME/../asp-pc/hypercube.large.edges.xz >  $BATS_TMPDIR/asp-gals/hypercube.large.edges
}

@test "asp-gals-compile-hw" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make hw)
}

@test "asp-gals-compile-sim" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make sim)
}

@test "asp-gals-run-medium-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim $BATS_TMPDIR/asp-gals/hypercube.medium.edges)
}

@test "asp-gals-run-large-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim $BATS_TMPDIR/asp-gals/hypercube.large.edges)
}

@test "asp-gals-run-medium-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && ./run $BATS_TMPDIR/asp-gals/hypercube.medium.edges)
}

@test "asp-gals-run-large-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && ./run $BATS_TMPDIR/asp-gals/hypercube.large.edges)
}
