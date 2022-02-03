#!/usr/bin/env python3

log2_edges=22
edges=2**log2_edges
log2_n = 8
while log2_n <= 24:
    n=int(2**log2_n)
    pConn=2.0**(-log2_n+2)

    while pConn <= 1.0 and pConn * n <= 16384:

        print(
f"""
log_n{n}_pConn{pConn:.6g}.log :
\tsleep 4s
\t../../bin/hardware_idle_izhikevich_fix --log-file=$@ '--topology={{"type":"EdgeProbTopology","nNeurons":{n},"pConnect":{pConn} }}'

all : log_n{n}_pConn{pConn:.6g}.log

"""
        )
        pConn = pConn*2;
    
    log2_n += 0.5

