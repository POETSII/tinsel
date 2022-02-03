

@test "tinsel-opcode-tests-build" {
    (cd $BATS_TEST_DIRNAME && make)
}

@test "tinsel-opcode-tests-run" {
    if [[ "$TINSEL_TEST_NO_HARDWARE" -eq 1 ]] ; then skip ; fi
    (cd $BATS_TEST_DIRNAME && make)
    (cd $BATS_TEST_DIRNAME && run)
}
