APPNAME=heat-app-native

@test "$APPNAME-compile-hw" {
    (
        cd $BATS_TEST_DIRNAME
        make clean
        make
        [[ -f data.v ]]
        [[ -f code.v ]]
        [[ -x run ]]
    )
}