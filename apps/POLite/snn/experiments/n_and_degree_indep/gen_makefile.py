#!/usr/bin/env python3

methods=["random", "metis", "bfs"]

edges=1024

log2_n=10
while log2_n <= 20:
    n=int(2**log2_n)
    pConn=min(1,edges/n)

    for method in methods:

        print(
f"""
log_n{n}_pConn{pConn:.6g}_m{method}.log :
\tsleep 4s
\t../../bin/hardware_idle_izhikevich_fix --log-file=$@ --placer={method} '--topology={{"type":"EdgeProbTopology","nNeurons":{n},"pConnect":{pConn:.1f} }}'

all : log_n{n}_pConn{pConn:.6g}_m{method}.log

"""
        )
    
    log2_n += 0.5

n=2**16

log2_e=4
while log2_e <= 16:
    edges=int(2**log2_e)
    pConn=min(1,edges/n)

    for method in methods:

        print(
f"""
log_n{n}_pConn{pConn:.6g}_m{method}.log :
\tsleep 4s
\t../../bin/hardware_idle_izhikevich_fix --log-file=$@ --placer={method} '--topology={{"type":"EdgeProbTopology","nNeurons":{n},"pConnect":{pConn:.1f} }}'

all : log_n{n}_pConn{pConn:.6g}_m{method}.log

"""
        )
    
    log2_e += 0.5

