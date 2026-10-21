#!/usr/bin/env python3
"""Plot teaching outputs; optional matplotlib dependency, no estimator code."""
import argparse
import csv
import os
from pathlib import Path
os.environ.setdefault("MPLCONFIGDIR", "/tmp/kf-toy-matplotlib")
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

def read(path):
    with path.open() as f:
        return list(csv.DictReader(f))

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("directory",type=Path)
    args=parser.parse_args()
    directory=args.directory
    traces=read(directory/"trace.csv")
    scenarios=list(dict.fromkeys(row["scenario"] for row in traces))
    for scenario in scenarios:
        rows=[row for row in traces if row["scenario"]==scenario and row["backend"]=="KF"]
        fig,axs=plt.subplots(2,2,figsize=(12,8))
        names=list(dict.fromkeys(row["recipe"] for row in rows))
        for name in names:
            samples=[row for row in rows if row["recipe"]==name]
            t=[float(row["t"]) for row in samples]
            axs[0,0].plot([float(row["x"]) for row in samples],[float(row["y"]) for row in samples],label=name)
            axs[0,1].plot(t,[float(row["position_error"]) for row in samples],label=name)
            axs[1,0].plot(t,[float(row["bg_error"]) for row in samples],label=name)
            axs[1,1].plot(t,[float(row["nees15"])/15 for row in samples],label=name)
        samples=[row for row in rows if row["recipe"]==names[0]]
        axs[0,0].plot([float(row["truth_x"]) for row in samples],[float(row["truth_y"]) for row in samples],"k--",label="truth")
        axs[0,0].set(xlabel="x [m]",ylabel="y [m]",title="Trajectory, seed 0")
        axs[0,0].axis("equal")
        axs[0,1].set(xlabel="time [s]",ylabel="position error [m]")
        axs[1,0].set(xlabel="time [s]",ylabel="gyro bias error [rad/s]")
        axs[1,1].set(xlabel="time [s]",ylabel="NEES15 / 15")
        axs[1,1].axhline(1,color="k",linestyle=":")
        for ax in axs.flat:
            ax.grid(alpha=.25)
        axs[0,0].legend(fontsize=8)
        fig.suptitle(scenario+" — KF; other backends in metrics.csv")
        fig.tight_layout()
        fig.savefig(directory/(scenario+".png"),dpi=160)
        plt.close(fig)
    rows=[row for row in read(directory/"remainder.csv") if row["direction"]=="1"]
    fig,ax=plt.subplots(figsize=(6,4))
    scales=[float(row["scale"]) for row in rows]
    for field in ["ordinary_remainder","midpoint_remainder"]:
        ax.loglog(scales,[float(row[field]) for row in rows],"o-",label=field)
    ax.set(xlabel="perturbation scale",ylabel="noiseless output remainder")
    ax.grid(alpha=.25);ax.legend();fig.tight_layout();fig.savefig(directory/"remainder.png",dpi=160);plt.close(fig)
    rows=read(directory/"constraints.csv")
    fig,ax=plt.subplots(figsize=(6,4))
    for field in ["tg_finite_bnu_z","reduced_bnu_z","subspace_prediction"]:
        ax.plot([float(row["scale"]) for row in rows],[float(row[field]) for row in rows],"o-",label=field)
    ax.set(xlabel="position correction x [m]",ylabel="physical virtual bias z [m/s]")
    ax.grid(alpha=.25);ax.legend();fig.tight_layout();fig.savefig(directory/"constraints.png",dpi=160);plt.close(fig)
    print("Wrote",len(scenarios)+2,"PNG figures in",directory)

if __name__=="__main__":
    main()
