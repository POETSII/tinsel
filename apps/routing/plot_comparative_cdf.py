#!/usr/bin/env python3
import matplotlib.pyplot as plt
import pandas
import glob
import sys
import pathlib
import numpy as np

from typing import *

def load_datasets(pattern:str):
    res={} # type: Dict[str,pandas.DataFrame]
    for f in glob.glob(pattern):
        p=pathlib.Path(f)
        name=p.name
        name=name.replace("placement.","")
        name=name.replace(".csv.","")
        res[name]=pandas.read_csv(f, skipinitialspace=True)
    return res


def plot_comparitive_cdf(datasets:Dict[str,pandas.DataFrame], domain, aspect, row, nrows):
    lines={} # type: Dict[str,Tuple[any,any]]
    for (name,dataset) in datasets.items():
        vals=[]
        samples=[]
        max_val=-1
        for (_,_,_,index,value) in dataset.loc[ (dataset["# Domain"]==domain) & (dataset["Aspect"]==aspect) ].itertuples():
            try:
                i=int(index)
            except ValueError:
                continue
            assert(i>=0)
            count=int(value)
            assert(count>=0)
            max_val=max(max_val,i)
            vals.append((i,count))
            samples += [i] * count
        pdf=np.zeros(shape=(max_val+1,))
        for (i,count) in vals:
            pdf[i]=count
        xx=np.arange(max_val+1)
        cdf=np.cumsum(pdf) / np.sum(pdf)
        lines[name]=(xx,pdf,cdf,samples)

    plt.subplot(nrows,3,3*row+1)
    plt.title(f"{domain}/{aspect}")
    for (name,(xx,_,cdf,_)) in lines.items():    
        plt.semilogx(xx,cdf, label=name)
    plt.legend()

    plt.subplot(nrows,3,3*row+2)
    plt.title(f"{domain}/{aspect}")
    for (name,(xx,_,cdf,_)) in lines.items():    
        plt.plot(xx,cdf, label=name)
    plt.legend(loc="lower right")

    plt.subplot(nrows,3,3*row+3)
    plt.title(f"{domain}/{aspect}")
    names=[]
    expanded=[]
    for (name,(xx,_,_,samples)) in lines.items():    
        expanded.append(samples)
        names.append(name)
    plt.boxplot(expanded, showmeans=True, labels=names)
    plt.yscale("log")
    plt.xticks(rotation=30,ha='right', rotation_mode='anchor')
    fig=plt.gcf()
    fig.subplots_adjust(bottom=0.2)

    
        

if __name__=="__main__":
    datasets=load_datasets(sys.argv[1])

    plt.figure(figsize=(20,7*5))
    for (i,aspect) in enumerate(["RouteCost","RouteHops","RouteFPGAHops","LinkLoad","FPGALinkLoad"]):
        plot_comparitive_cdf(datasets, "Routing", aspect, i, 5)
    plt.savefig(sys.argv[2])