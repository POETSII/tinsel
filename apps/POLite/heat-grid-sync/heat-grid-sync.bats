APPNAME=heat-grid-sync

@test "heat-grid-sync-compile-hw" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make hw)
}

@test "heat-grid-sync-compile-sim" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make sim)
}

@test "heat-grid-sync-run-small-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim 32 32 100)
}

@test "heat-grid-sync-run-medium-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim 64 64 200)
}

@test "heat-grid-sync-run-default-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && ./run)
}

