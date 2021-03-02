#!/usr/bin/env python3
import math

ns=[round(2.0**log2n) for log2n in range(12,20)]
ps=[2**-10,2**-9,2**-8,2**-7,2**-6,2**-5,2**-4,2**-3,2**-2,1]
rs=[0,2**-8,2**-6,2**-2]

for n in ns:
    for p in ps:
        for r in rs:
            if n*n*p >= 1000000000:
                continue

            name=f"n{n}_p{p:.6g}_r{r:.6g}"
            print(
f"""
log_{name}.log :
\t../../bin/hardware_idle_poisson --log-file=$@ '--topology={{"type":"EdgeProbTopology","nNeurons":{n},"pConnect":{p} }}' \
        '--neuron-model={{"type":"Poisson", "firing_rate_per_step":{r} }}' --max-steps={2**14} \
            --user-key-value=n:{n} --user-key-value=p:{p:.6g} --user-key-value=r:{r:.6g}

all : log_{name}.log

"""
            )
