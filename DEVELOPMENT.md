# Developing Tinsel

## Simulation only testing

To run a simulation of a Tinsel system locally, we need a hardware sim, hostlink, and a application. A simple hello world on all threads can be done as follows:

```
make clean
make -C rtl TOPFILE=DE5Top.bsv TOPMOD=mkDE5Top sim
make -C hostlink
make -C apps/hello sim all
bash -c "cd rtl && ./sim.sh" & bash -c "cd apps/hello && sleep 5 && echo starting app && ./sim"
```
Simulation performance is significantly improved by modifying the sim config:

```Python
if True: # simulate
    p["MailboxMeshXBits"] = 1
    p["MailboxMeshYBits"] = 1
    p["LogDCachesPerDRAM"] = 1
    p["MeshXLenWithinBox"] = 1
    p["MeshYLenWithinBox"] = 1
    p["BoxMeshXLen"] = 1
    p["BoxMeshYLen"] = 1
    p["BoxMesh"] = ('{'
        '{"tparks-optiplex-390",},'
      '}')
```
