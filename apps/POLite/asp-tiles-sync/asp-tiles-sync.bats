APPNAME=asp-tiles-sync

function setup_file {
    mkdir -p $BATS_TMPDIR/$APPNAME
    xzcat $BATS_TEST_DIRNAME/../asp-pc/hypercube.medium.edges.xz >  $BATS_TMPDIR/$APPNAME/hypercube.medium.edges
    xzcat $BATS_TEST_DIRNAME/../asp-pc/hypercube.large.edges.xz >  $BATS_TMPDIR/$APPNAME/hypercube.large.edges
}

@test "$APPNAME-compile-hw" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make build/run)
}

@test "$APPNAME-compile-sim" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make build/sim)
}

@test "$APPNAME-run-medium-sim" {
    (cd $BATS_TEST_DIRNAME && make build/sim)
    (cd $BATS_TEST_DIRNAME && build/sim $BATS_TMPDIR/$APPNAME/hypercube.medium.edges)
}

@test "$APPNAME-run-large-sim" {
    (cd $BATS_TEST_DIRNAME && make build/sim)
    (cd $BATS_TEST_DIRNAME && build/sim $BATS_TMPDIR/$APPNAME/hypercube.large.edges)
}

@test "$APPNAME-run-medium-hw" {
    if [[ "$TINSEL_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make build/run)
    (cd $BATS_TEST_DIRNAME && build/run $BATS_TMPDIR/$APPNAME/hypercube.medium.edges)
}

@test "$APPNAME-run-large-hw" {
    if [[ "$TINSEL_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make build/run)
    (cd $BATS_TEST_DIRNAME && build/run $BATS_TMPDIR/$APPNAME/hypercube.large.edges)
}
