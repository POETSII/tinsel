#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
APPS_DIR=$(realpath $SCRIPT_DIR/..)

COMPILE_APPS="asp-gals asp-sync asp-tiles-sync clocktree-async \
    hashmin-sync heat-cube-sync heat-gals heat-grid-sync \
    heat-sync \
    izhikevich-gals izhikevich-sync \
    pagerank-gals pagerank-sync \
    sssp-async sssp-sync \
    pressure-sync nhood-sync"

echo "TAP version 13"

TN=1

function record_ok {
    echo "ok $TN $1"
    TN=$((TN+1))
}

function record_not_ok {
    if [[ "$2" != "" ]] ; then
        echo "# Error in  $1"
        echo "# "
        echo "$2" | while read LINE ; do
            echo "# > $LINE"
        done
    fi
    echo "not ok $TN $1"
    TN=$((TN+1))
}

for APP in $COMPILE_APPS ; do
   (cd $APPS_DIR/$APP && make clean) > /dev/null

    OUTPUT=$(cd $APPS_DIR/$APP && make sim 2>&1)
    RES=$?
    if [[ $RES -eq 0 ]] ; then
        record_ok "Compile $APP with POLiteSWSim"
    else 
        record_not_ok "Compile $APP POLiteSWSim" "$OUTPUT"
    fi

    OUTPUT=$(cd $APPS_DIR/$APP && make sim-release 2>&1)
    RES=$?
    if [[ $RES -eq 0 ]] ; then
        record_ok "Compile $APP with POLiteSWSim (release)"
    else 
        record_not_ok "Compile $APP POLiteSWSim (release)" "$OUTPUT"
    fi
done

function test_run {
    APP="$1"
    shift

    NAME="Run $APP with args $@"

    if [[ ! -x $APPS_DIR/$APP/build/sim ]] ; then 
        record_not_ok "$NAME" "Sim executable not build by earlier test"
    else 
        OUTPUT=$(cd $APPS_DIR/$APP/build && ./sim $@ 2>&1)
        RES=$?
        if [[ $RES -eq 0 ]] ; then
            record_ok "$NAME"
        else 
            record_not_ok "$NAME" "$OUTPUT"
        fi
    fi

    NAME="Run $APP with args $@ (release)"

    if [[ ! -x $APPS_DIR/$APP/build/sim-release ]] ; then 
        record_not_ok "$NAME" "Sim executable not build by earlier test"
    else 
        OUTPUT=$(cd $APPS_DIR/$APP/build && ./sim-release $@ 2>&1)
        RES=$?
        if [[ $RES -eq 0 ]] ; then
            record_ok "$NAME"
        else 
            record_not_ok "$NAME" "$OUTPUT"
        fi
    fi
}

test_run "clocktree-async" 5 5
test_run "pressure-sync" 10
test_run "nhood-sync"
