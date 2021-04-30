APPNAME=heat-grid-sync

@test "$APPNAME-compile-hw" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make build/run)
}

@test "$APPNAME-compile-sim" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make build/sim)
}

@test "$APPNAME-run-small-sim" {
    (cd $BATS_TEST_DIRNAME && make build/sim)
    (cd $BATS_TEST_DIRNAME && build/sim 32 32 100)
}

@test "$APPNAME-run-medium-sim" {
    (cd $BATS_TEST_DIRNAME && make build/sim)
    (cd $BATS_TEST_DIRNAME && build/sim 64 64 200)
}

@test "$APPNAME-run-default-hw" {
    if [[ "$TINSEL_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make build/run)
    (cd $BATS_TEST_DIRNAME && build/run)
}

