APPNAME=asp-tiles-sync

function setup_file {
    mkdir -p $BATS_TMPDIR/asp-tiles-sync
    xzcat $BATS_TEST_DIRNAME/../asp-pc/hypercube.medium.edges.xz >  $BATS_TMPDIR/asp-tiles-sync/hypercube.medium.edges
    xzcat $BATS_TEST_DIRNAME/../asp-pc/hypercube.large.edges.xz >  $BATS_TMPDIR/asp-tiles-sync/hypercube.large.edges
}

@test "asp-tiles-sync-compile-hw" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make hw)
}

@test "asp-tiles-sync-compile-sim" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make sim)
}

@test "asp-tiles-sync-run-medium-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim $BATS_TMPDIR/asp-tiles-sync/hypercube.medium.edges)
}

@test "asp-tiles-sync-run-large-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim $BATS_TMPDIR/asp-tiles-sync/hypercube.large.edges)
}

@test "asp-tiles-sync-run-medium-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && ./run $BATS_TMPDIR/asp-tiles-sync/hypercube.medium.edges)
}

@test "asp-tiles-sync-run-large-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && ./run $BATS_TMPDIR/asp-tiles-sync/hypercube.large.edges)
}
