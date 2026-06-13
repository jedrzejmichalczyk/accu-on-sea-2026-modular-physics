"""Energy-conservation plot for the ACCU deck, styled to match the slide theme."""
import csv
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BG = "#081626"
FG = "#D8E2EC"
CYAN = "#35C4E8"
YELLOW = "#F2C744"
GRID = "#1E3A55"

t, x1, x2, drift = [], [], [], []
with open("oscillator_data.csv") as f:
    for row in csv.DictReader(f):
        t.append(float(row["t"]))
        x1.append(float(row["x1"]))
        x2.append(float(row["x2"]))
        drift.append(float(row["drift"]))

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 5.4), dpi=200, sharex=True)
fig.patch.set_facecolor(BG)

for ax in (ax1, ax2):
    ax.set_facecolor(BG)
    ax.grid(color=GRID, linewidth=0.6)
    for s in ax.spines.values():
        s.set_color(GRID)
    ax.tick_params(colors=FG, labelsize=9)

ax1.plot(t, x1, color=CYAN, lw=1.4, label="mass 1 position")
ax1.plot(t, x2, color=YELLOW, lw=1.4, label="mass 2 position")
ax1.set_ylabel("position [m]", color=FG, fontsize=10)
ax1.legend(facecolor=BG, edgecolor=GRID, labelcolor=FG, fontsize=9, loc="upper right")

ax2.plot(t, [d * 1e13 for d in drift], color="#7EE08A", lw=1.2)
ax2.set_ylabel(r"energy drift  $(E-E_0)/E_0$  [$\times 10^{-13}$]", color=FG, fontsize=10)
ax2.set_xlabel("time [s]", color=FG, fontsize=10)

fig.suptitle("Coupled oscillator — 20 001 RK4 steps, dt = 1 ms   (g++ 14, -O2, real run)",
             color=FG, fontsize=11)
fig.tight_layout(rect=(0, 0, 1, 0.96))
fig.savefig("energy_conservation.png", facecolor=BG, bbox_inches="tight")
print("saved energy_conservation.png")
