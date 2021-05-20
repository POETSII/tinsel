APPNAME=hashmin-sync

function setup_file {
    mkdir -p $BATS_TMPDIR/hashmin-sync
    xzcat $BATS_TEST_DIRNAME/../asp-pc/hypercube.medium.edges.xz >  $BATS_TMPDIR/hashmin-sync/hypercube.medium.edges
    xzcat $BATS_TEST_DIRNAME/../asp-pc/hypercube.large.edges.xz >  $BATS_TMPDIR/hashmin-sync/hypercube.large.edges
}

@test "hashmin-sync-compile-hw" {
    [[ -f $BATS_TEST_DIRNAME/build/run ]] && rm $BATS_TEST_DIRNAME/build/run
    (cd $BATS_TEST_DIRNAME && make hw)
}

@test "hashmin-sync-compile-sim" {
    [[ -f $BATS_TEST_DIRNAME/build/sim ]] && rm $BATS_TEST_DIRNAME/build/sim
    (cd $BATS_TEST_DIRNAME && make sim)
}

@test "hashmin-sync-run-medium-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim $BATS_TMPDIR/hashmin-sync/hypercube.medium.edges)
}

@test "hashmin-sync-run-large-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim $BATS_TMPDIR/hashmin-sync/hypercube.large.edges)
}

@test "hashmin-sync-run-medium-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && ./run $BATS_TMPDIR/hashmin-sync/hypercube.medium.edges)
}

@test "hashmin-sync-run-large-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && ./run $BATS_TMPDIR/hashmin-sync/hypercube.large.edges)
}
