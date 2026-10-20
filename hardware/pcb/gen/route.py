"""Small two-layer grid router (A* with exact clearance checks).

Author: Mohamed Khamis
"""
import heapq
import math

CLR = 0.2          # copper clearance the board is checked against
MARGIN = 0.1       # extra margin: covers the points between two grid nodes
EDGE_CLR = 0.5     # copper to board edge
VIA_D, VIA_DRILL = 0.6, 0.3
VIA_COST = 12.0
BUCKET = 2.0
REACH = 1.6


def pt_seg_dist(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    l2 = dx * dx + dy * dy
    if l2 == 0:
        return math.hypot(px - x1, py - y1)
    t = max(0.0, min(1.0, ((px - x1) * dx + (py - y1) * dy) / l2))
    return math.hypot(px - (x1 + t * dx), py - (y1 + t * dy))


class Obj:
    __slots__ = ('kind', 'g', 'net', 'layers')

    def __init__(self, kind, g, net, layers):
        self.kind, self.g, self.net, self.layers = kind, g, net, layers

    def dist(self, px, py):
        k, g = self.kind, self.g
        if k == 'circle':
            return math.hypot(px - g[0], py - g[1]) - g[2]
        if k == 'rect':
            x1, y1, x2, y2 = g
            dx = max(x1 - px, 0.0, px - x2)
            dy = max(y1 - py, 0.0, py - y2)
            if dx == 0 and dy == 0:
                return -min(px - x1, x2 - px, py - y1, y2 - py)
            return math.hypot(dx, dy)
        x1, y1, x2, y2, hw = g
        return pt_seg_dist(px, py, x1, y1, x2, y2) - hw

    def bbox(self):
        k, g = self.kind, self.g
        if k == 'circle':
            return g[0] - g[2], g[1] - g[2], g[0] + g[2], g[1] + g[2]
        if k == 'rect':
            return g
        x1, y1, x2, y2, hw = g
        return min(x1, x2) - hw, min(y1, y2) - hw, max(x1, x2) + hw, max(y1, y2) + hw


class Board:
    def __init__(self, w, h, grid):
        self.w, self.h, self.g = w, h, grid
        self.nx, self.ny = int(round(w / grid)) + 1, int(round(h / grid)) + 1
        self.buckets = {}
        self.objs = []
        self.tracks = []   # (x1, y1, x2, y2, width, layer, net)
        self.vias = []     # (x, y, net)

    def add(self, obj):
        self.objs.append(obj)
        x1, y1, x2, y2 = obj.bbox()
        for bx in range(int((x1 - REACH) // BUCKET), int((x2 + REACH) // BUCKET) + 1):
            for by in range(int((y1 - REACH) // BUCKET), int((y2 + REACH) // BUCKET) + 1):
                for L in obj.layers:
                    self.buckets.setdefault((L, bx, by), []).append(obj)

    def near(self, L, px, py):
        return self.buckets.get((L, int(px // BUCKET), int(py // BUCKET)), ())

    def clearance(self, L, px, py, net):
        best = 1e9
        for o in self.near(L, px, py):
            if o.net != net:
                d = o.dist(px, py)
                if d < best:
                    best = d
        return best

    def inside_own(self, L, px, py, net, inset=0.05):
        for o in self.near(L, px, py):
            if o.net == net and o.dist(px, py) <= -inset:
                return True
        return False

    def add_track(self, x1, y1, x2, y2, w, L, net):
        self.tracks.append((x1, y1, x2, y2, w, L, net))
        self.add(Obj('seg', (x1, y1, x2, y2, w / 2), net, (L,)))

    def add_via(self, x, y, net):
        self.vias.append((x, y, net))
        self.add(Obj('circle', (x, y, VIA_D / 2), net, (0, 1)))

    # ------------------------------------------------------------ routing
    def route(self, net, src_objs, width, layer_cost=(1.0, 1.6), max_exp=600000):
        """Route from the copper of src_objs to any other copper of `net`."""
        g = self.g
        hw = width / 2
        need = hw + CLR + MARGIN
        vneed = VIA_D / 2 + CLR + 0.03
        edge = hw + EDGE_CLR
        cache, mcache, vcache = {}, {}, {}

        def ok(L, ix, iy):
            k = (L, ix, iy)
            v = cache.get(k)
            if v is None:
                px, py = ix * g, iy * g
                if px < edge or py < edge or px > self.w - edge or py > self.h - edge:
                    v = False
                else:
                    v = self.clearance(L, px, py, net) >= need
                cache[k] = v
            return v

        def ok_mid(L, jx, jy):  # half-grid coordinates
            k = (L, jx, jy)
            v = mcache.get(k)
            if v is None:
                v = self.clearance(L, jx * g / 2, jy * g / 2, net) >= need
                mcache[k] = v
            return v

        def ok_via(ix, iy):
            k = (ix, iy)
            v = vcache.get(k)
            if v is None:
                px, py = ix * g, iy * g
                e = VIA_D / 2 + EDGE_CLR
                v = (e <= px <= self.w - e and e <= py <= self.h - e and
                     self.clearance(0, px, py, net) >= vneed and
                     self.clearance(1, px, py, net) >= vneed)
                vcache[k] = v
            return v

        # source nodes: inside the source copper
        src = set()
        for o in src_objs:
            x1, y1, x2, y2 = o.bbox()
            for ix in range(int(math.ceil(x1 / g)), int(math.floor(x2 / g)) + 1):
                for iy in range(int(math.ceil(y1 / g)), int(math.floor(y2 / g)) + 1):
                    if o.dist(ix * g, iy * g) <= -0.05:
                        for L in o.layers:
                            if ok(L, ix, iy):
                                src.add((L, ix, iy))
        srcset = set(src_objs)
        # target nodes: inside any other copper of this net
        tgt = set()
        tpts = []
        for o in self.objs:
            if o.net != net or o in srcset:
                continue
            x1, y1, x2, y2 = o.bbox()
            tpts.append(((x1 + x2) / 2, (y1 + y2) / 2))
            for ix in range(int(math.ceil(x1 / g)), int(math.floor(x2 / g)) + 1):
                for iy in range(int(math.ceil(y1 / g)), int(math.floor(y2 / g)) + 1):
                    if o.dist(ix * g, iy * g) <= -0.05:
                        for L in o.layers:
                            if ok(L, ix, iy):
                                tgt.add((L, ix, iy))
        if not src or not tgt:
            return None
        if src & tgt:
            return []
        # heuristic: distance to the nearest target object centre
        tg = [(x / g, y / g) for x, y in tpts]

        def h(ix, iy):
            best = 1e18
            for tx, ty in tg:
                dx, dy = abs(ix - tx), abs(iy - ty)
                d = max(dx, dy) + 0.4142 * min(dx, dy)
                if d < best:
                    best = d
            return best * 1.15

        dirs = [(1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1)]
        openq = []
        best = {}
        parent = {}
        for (L, ix, iy) in src:
            s = (L, ix, iy, 8)
            best[s] = 0.0
            parent[s] = None
            heapq.heappush(openq, (h(ix, iy), 0.0, s))
        exp = 0
        end = None
        while openq:
            f, c, s = heapq.heappop(openq)
            if c > best.get(s, 1e18):
                continue
            L, ix, iy, d = s
            if (L, ix, iy) in tgt:
                end = s
                break
            exp += 1
            if exp > max_exp:
                break
            lc = layer_cost[L]
            for di, (dx, dy) in enumerate(dirs):
                nx, ny = ix + dx, iy + dy
                if not ok(L, nx, ny) or not ok_mid(L, 2 * ix + dx, 2 * iy + dy):
                    continue
                step = (1.4142 if dx and dy else 1.0) * lc
                if d != 8 and d != di:
                    turn = min((di - d) % 8, (d - di) % 8)
                    if turn > 2:
                        continue
                    step += 0.6 * turn
                ns = (L, nx, ny, di)
                nc = c + step
                if nc < best.get(ns, 1e18):
                    best[ns] = nc
                    parent[ns] = s
                    heapq.heappush(openq, (nc + h(nx, ny), nc, ns))
            # layer change
            if ok_via(ix, iy) and ok(1 - L, ix, iy):
                ns = (1 - L, ix, iy, 8)
                nc = c + VIA_COST
                if nc < best.get(ns, 1e18):
                    best[ns] = nc
                    parent[ns] = s
                    heapq.heappush(openq, (nc + h(ix, iy), nc, ns))
        if end is None:
            return None
        path = []
        s = end
        while s is not None:
            path.append(s[:3])
            s = parent[s]
        path.reverse()
        return path

    def commit(self, path, width, net):
        """Turn a node path into tracks and vias."""
        g = self.g
        if not path:
            return
        i = 0
        n = len(path)
        while i < n - 1:
            L, x0, y0 = path[i]
            if path[i + 1][0] != L:
                self.add_via(x0 * g, y0 * g, net)
                i += 1
                continue
            j = i + 1
            ddx, ddy = path[j][1] - x0, path[j][2] - y0
            while (j + 1 < n and path[j + 1][0] == L and
                   path[j + 1][1] - path[j][1] == ddx and path[j + 1][2] - path[j][2] == ddy):
                j += 1
            self.add_track(round(x0 * g, 4), round(y0 * g, 4),
                           round(path[j][1] * g, 4), round(path[j][2] * g, 4), width, L, net)
            i = j
