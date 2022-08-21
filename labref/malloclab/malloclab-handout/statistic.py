import os
import matplotlib.pyplot as plt

def show_scatter_graph(filepath, filename):
    with open(os.path.join(filepath, filename)) as f:
            sizes = []
            while True:
                line = f.readline()
                if not line:
                    break
                if line.startswith("a") or line.startswith("r"):
                    sizes.append(int(line.split()[2]))
            sizes.sort()
            print(filename, ": min", sizes[0], ": max", sizes[-1])
            x, y = [], []
            cx, count = sizes[0], 1
            for s in sizes:
                if cx == s:
                    count += 1
                else:
                    x.append(cx)
                    y.append(count)
                    cx = s
                    count = 1
            if cx != sizes[0]:
                x.append(cx)
                y.append(count)

            plt.figure(figsize=(12, 6))
            plt.title(filename)
            # plt.xscale("log")
            plt.yscale("log")
            plt.scatter(x, y, s=10)
            plt.show()

if __name__ == "__main__":
    for filepath, dirnames, filenames in os.walk("./traces"):
        for filename in filenames:
            show_scatter_graph(filepath, filename)