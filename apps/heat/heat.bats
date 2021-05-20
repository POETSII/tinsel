APPNAME=heat-app-native

@test "heat-app-native-compile-hw" {
    (
        cd $BATS_TEST_DIRNAME
        make clean
        make
        [[ -f data.v ]]
        [[ -f code.v ]]
        [[ -x run ]]
    )
}