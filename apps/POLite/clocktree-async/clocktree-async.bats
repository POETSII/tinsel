APPNAME=clocktree-async

@test "clocktree-async-compile-hw" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make hw)
}

@test "clocktree-async-compile-sim" {
    (cd $BATS_TEST_DIRNAME && make clean)
    (cd $BATS_TEST_DIRNAME && make sim)
}

@test "clocktree-async-run-medium-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim 4 4)
}

@test "clocktree-async-run-large-sim" {
    (cd $BATS_TEST_DIRNAME && make sim)
    (cd $BATS_TEST_DIRNAME/build && ./sim 7 7)
}

@test "clocktree-async-run-medium-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && run 4 4)
}

@test "clocktree-async-run-large-hw" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make hw)
    (cd $BATS_TEST_DIRNAME/build && run 7 7)
}
