#/usr/bin/env python3
import sys
import glob
import pandas
import collections

src_dir=sys.argv[1]

columns=collections.OrderedDict()

rows={}

for file in glob.glob(f"{src_dir}/*.log"):
    index=file
    row={}
    with open(file, "rt") as src:
        for l in src:
            l=l.strip()
            if l=="":
                continue
            parts=l.split(",")
            parts=[p.strip() for p in parts]
            #print(parts)
            assert(3<=len(parts)<=4)
            time=float(parts[0])
            mode=parts[1]
            key=parts[2]
            if len(parts)>3 and key!="":
                value=parts[3]
                if mode in ["VAL","EXIT"]:
                    columns.setdefault(key,len(columns))
                row[key]=value
        rows[index]=row

print("id", end="")
for (k,i) in columns.items():
    print(f",{k}", end="")
print()

for (k,r) in rows.items():
    vals=[""]*(len(columns)+1)
    vals[0]=k
    for (n,v) in r.items():
        assert n!=""
        vals[columns[n]+1]=v
    print(",".join(vals))