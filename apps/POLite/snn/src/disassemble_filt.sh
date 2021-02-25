#!/bin/bash

RUNNER_FROM="snn::HardwareIdleRunner<Neuron, Stats>"
RUNNER_TO="Runner"

c++filt | sed \
    -e "s/snn::izhikevich_neuron_model/Neuron/g" \
    -e "s/snn::stats_minimal/Stats/g" \
    -e "s/${RUNNER_FROM}/${RUNNER_TO}/g" \
    -e "s/PThread<Runner::device_type, Runner::state_type, Neuron::weight_type, Runner::message_type/PThread/g"