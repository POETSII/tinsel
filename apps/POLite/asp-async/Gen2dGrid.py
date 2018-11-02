import sys

GRID_SIZE = int(sys.argv[1])

edges = []

def get_n(x,y):
    return x * GRID_SIZE + y 


for x in range(GRID_SIZE):
    for y in range(GRID_SIZE):
        n = get_n(x,y)

        if x + 1 < GRID_SIZE:
            edges.append((get_n(x,y), get_n(x+1,y)))
            edges.append((get_n(x+1,y), get_n(x,y)))

        if y + 1 < GRID_SIZE:
            edges.append((get_n(x,y), get_n(x,y+1)))
            edges.append((get_n(x,y+1), get_n(x,y)))

for e in edges:
    print("{} {}".format(e[0], e[1]))