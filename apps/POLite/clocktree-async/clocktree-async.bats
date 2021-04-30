APPNAME=clocktree-async

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
    (cd $BATS_TEST_DIRNAME && build/sim 4 4)
}

@test "$APPNAME-run-large-sim" {
    (cd $BATS_TEST_DIRNAME && make build/sim)
    (cd $BATS_TEST_DIRNAME && build/sim 7 7)
}

@test "$APPNAME-run-medium-hw" {
    if [[ "$TINSEL_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make build/run)
    (cd $BATS_TEST_DIRNAME && build/run 4 4)
}

@test "$APPNAME-run-large-hw" {
    if [[ "$TINSEL_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make build/run)
    (cd $BATS_TEST_DIRNAME && build/run 7 7)
}
