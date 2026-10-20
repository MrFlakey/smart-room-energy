"""Build the Smart Room carrier board KiCad project from design.py.

Author: Mohamed Khamis

Run:  python hardware/pcb/gen/build.py
Writes the schematic, board (placed and routed), symbol and footprint libraries
into hardware/pcb/.
"""
import json
import math
import os
import sys
import time
import uuid

sys.path.insert(0, os.path.dirname(__file__))
import design as D          # noqa: E402
import route as R           # noqa: E402

OUT = os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))
NAME = 'smart-room-carrier'
LIB = 'SmartRoom'
OX, OY = 50.0, 40.0          # board origin on the PCB page
NS = uuid.UUID('6c1d2a0e-7a3b-4f0e-9a51-5d0c0ffee001')


def uid(*k):
    return str(uuid.uuid5(NS, '/'.join(str(x) for x in k)))


def q(s):
    return '"' + str(s).replace('\\', '\\\\').replace('"', '\\"') + '"'


def f(v):
    s = f'{v:.4f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


ROOT = uid('root-sheet')
TITLE = 'Smart Room carrier board'
AUTHOR = 'Mohamed Khamis'
DATE = '2026-10-20'
REV = '1.1'


def title_block():
    return (f'(title_block (title {q(TITLE)}) (date {q(DATE)}) (rev {q(REV)}) '
            f'(company {q(AUTHOR)}) (comment 1 {q("ESP32 DevKit V1 carrier for the DFRobot Gravity modules")}) '
            f'(comment 2 {q("Pins: docs/esp32-wiring.md, firmware/main/board_pins.h")}))')


# ================================================================== symbols
def sym_geom(name):
    left, right, w, prefix, descr = D.SYMBOLS[name]
    n = max(len(left), len(right), 1)
    h = n * 2.54
    top = (n - 1) / 2 * 2.54
    pins = []
    for i, (num, pname) in enumerate(left):
        pins.append((num, pname, -w / 2 - 2.54, top - i * 2.54, 0))
    for i, (num, pname) in enumerate(right):
        pins.append((num, pname, w / 2 + 2.54, top - i * 2.54, 180))
    return pins, w, h, top, prefix, descr


def sym_def(name, full):
    pins, w, h, top, prefix, descr = sym_geom(name)
    hide_names = all(p[1] == '~' for p in pins) if pins else True
    out = [f'(symbol {q(full)}']
    if hide_names:
        out.append('(pin_names (offset 0) hide)')
    else:
        out.append('(pin_names (offset 0.508))')
    out.append('(exclude_from_sim no) (in_bom yes) (on_board yes)')
    yb = top + 1.27 + 1.27
    out.append(f'(property "Reference" {q(prefix)} (at 0 {f(yb)} 0) (effects (font (size 1.27 1.27))))')
    out.append(f'(property "Value" {q(name)} (at 0 {f(-(h - top) - 1.27)} 0) (effects (font (size 1.27 1.27))))')
    out.append('(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))')
    out.append('(property "Datasheet" "~" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))')
    out.append(f'(property "Description" {q(descr)} (at 0 0 0) (effects (font (size 1.27 1.27)) hide))')
    base = name
    out.append(f'(symbol {q(base + "_0_1")} (rectangle (start {f(-w / 2)} {f(top + 1.27)}) '
               f'(end {f(w / 2)} {f(top - h + 1.27)}) (stroke (width 0.254) (type default)) '
               f'(fill (type background))))')
    out.append(f'(symbol {q(base + "_1_1")}')
    for num, pname, x, y, ang in pins:
        out.append(f'(pin passive line (at {f(x)} {f(y)} {ang}) (length 2.54) '
                   f'(name {q(pname)} (effects (font (size 1.016 1.016)))) '
                   f'(number {q(num)} (effects (font (size 1.016 1.016)))))')
    out.append(')')
    out.append(')')
    return '\n'.join(out)


def pin_points(p):
    """Schematic connection points of a placed symbol: {pin: (x, y, side)}."""
    pins = sym_geom(p['sym'])[0]
    return {num: (p['sx'] + x, p['sy'] - y, 'L' if ang == 0 else 'R')
            for num, _, x, y, ang in pins}


# ================================================================== schematic
def write_schematic():
    used = sorted({p['sym'] for p in D.PARTS})
    o = [f'(kicad_sch (version 20231120) (generator "eeschema") (generator_version "8.0") '
         f'(uuid {q(ROOT)}) (paper "A3") {title_block()}',
         '(lib_symbols']
    for s in used:
        o.append(sym_def(s, f'{LIB}:{s}'))
    o.append(')')
    for p in D.PARTS:
        pins, w, h, top, prefix, descr = sym_geom(p['sym'])
        su = uid('sym', p['ref'])
        x, y = p['sx'], p['sy']
        o.append(f'(symbol (lib_id {q(LIB + ":" + p["sym"])}) (at {f(x)} {f(y)} 0) (unit 1) '
                 f'(exclude_from_sim no) (in_bom {"no" if p["ref"].startswith(("H", "NT")) else "yes"}) '
                 f'(on_board yes) (dnp no) (uuid {q(su)})')
        o.append(f'(property "Reference" {q(p["ref"])} (at {f(x)} {f(y - top - 2.54)} 0) '
                 f'(effects (font (size 1.27 1.27))))')
        o.append(f'(property "Value" {q(p["value"])} (at {f(x)} {f(y + (h - top) + 0.254)} 0) '
                 f'(effects (font (size 1.27 1.27))))')
        o.append(f'(property "Footprint" {q(LIB + ":" + p["fp"])} (at {f(x)} {f(y)} 0) '
                 f'(effects (font (size 1.27 1.27)) hide))')
        o.append(f'(property "Datasheet" "~" (at {f(x)} {f(y)} 0) (effects (font (size 1.27 1.27)) hide))')
        o.append(f'(property "Description" {q(descr)} (at {f(x)} {f(y)} 0) '
                 f'(effects (font (size 1.27 1.27)) hide))')
        if p.get('mpn'):
            o.append(f'(property "Part" {q(p["mpn"])} (at {f(x)} {f(y)} 0) '
                     f'(effects (font (size 1.27 1.27)) hide))')
        for num, *_ in pins:
            o.append(f'(pin {q(num)} (uuid {q(uid("pin", p["ref"], num))}))')
        o.append(f'(instances (project {q(NAME)} (path {q("/" + ROOT)} '
                 f'(reference {q(p["ref"])}) (unit 1))))')
        o.append(')')
        for num, (px, py, side) in pin_points(p).items():
            net = p['pins'].get(num)
            if net is None:
                o.append(f'(no_connect (at {f(px)} {f(py)}) (uuid {q(uid("nc", p["ref"], num))}))')
                continue
            ang, just = (180, 'right') if side == 'L' else (0, 'left')
            o.append(f'(label {q(net)} (at {f(px)} {f(py)} {ang}) (fields_autoplaced yes) '
                     f'(effects (font (size 1.27 1.27)) (justify {just} bottom)) '
                     f'(uuid {q(uid("lbl", p["ref"], num))}))')
    texts = list(D.SCH_TEXT) + [
        ('ESP32 DevKit V1 (30-pin) - sockets for the DevKit', 25.4, 33.02, 1.5),
        ('Notes', 25.4, 215.9, 2.0),
        ('1. Each module VCC jumper: cap on 5V+VCC = 5 V, cap on VCC+3V3 = 3.3 V. Defaults: TEMP on 5V, the rest on 3V3.', 25.4, 221.0, 1.27),
        ('2. Every signal goes through a series resistor and a BAT54S clamp, so a 5 V module cannot lift a GPIO above ~3.6 V.', 25.4, 224.8, 1.27),
        ('3. Wattmeter I2C goes through a BSS138 level shifter, so the SEN0291 may run on 5 V or 3V3.', 25.4, 228.6, 1.27),
        ('4. The fan takes 5 V only through the wattmeter (IN+ -> IN-). Close BYPASS only if the wattmeter is not fitted.', 25.4, 232.4, 1.27),
        ('5. Fan ground (GND_FAN) joins logic GND only at NT1, next to the DevKit GND pin (star ground).', 25.4, 236.2, 1.27),
        ('6. Sensor 5 V (+5V_S) is filtered from VIN by FB1, so fan noise stays off the sensor rail.', 25.4, 240.0, 1.27),
        (f'Author: {AUTHOR}', 25.4, 246.4, 1.5),
    ]
    for t, x, y, s in texts:
        o.append(f'(text {q(t)} (exclude_from_sim no) (at {f(x)} {f(y)} 0) '
                 f'(effects (font (size {f(s)} {f(s)})) (justify left bottom)) (uuid {q(uid("txt", t, x, y))}))')
    o.append('(sheet_instances (path "/" (page "1")))')
    o.append(')')
    with open(os.path.join(OUT, NAME + '.kicad_sch'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(o) + '\n')


def write_symbol_lib():
    o = ['(kicad_symbol_lib (version 20231120) (generator "kicad_symbol_editor") (generator_version "8.0")']
    for s in D.SYMBOLS:
        o.append(sym_def(s, s))
    o.append(')')
    with open(os.path.join(OUT, LIB + '.kicad_sym'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(o) + '\n')


# ================================================================== geometry
def xform(x, y, rot):
    a = math.radians(rot)
    c, s = round(math.cos(a), 9), round(math.sin(a), 9)
    return x * c + y * s, -x * s + y * c


def abs_pads(p):
    fp = D.FOOTPRINTS[p['fp']]
    out = []
    for (num, kind, shape, x, y, sx, sy, drill) in fp['pads']:
        dx, dy = xform(x, y, p['rot'])
        if p['rot'] % 180 == 90:
            sx, sy = sy, sx
        out.append((num, kind, shape, p['x'] + dx, p['y'] + dy, sx, sy, drill))
    return out


# ================================================================== footprints
def gfx_sexpr(gfx, k):
    out = []
    for i, g in enumerate(gfx):
        t, layer = g[0], g[1]
        u = q(uid('gfx', k, i))
        if t == 'line':
            _, _, x1, y1, x2, y2, w = g
            out.append(f'(fp_line (start {f(x1)} {f(y1)}) (end {f(x2)} {f(y2)}) '
                       f'(stroke (width {f(w)}) (type solid)) (layer {q(layer)}) (uuid {u}))')
        elif t == 'rect':
            _, _, x1, y1, x2, y2, w = g
            out.append(f'(fp_rect (start {f(x1)} {f(y1)}) (end {f(x2)} {f(y2)}) '
                       f'(stroke (width {f(w)}) (type solid)) (fill none) (layer {q(layer)}) (uuid {u}))')
        elif t == 'circle':
            _, _, cx, cy, r, w = g
            out.append(f'(fp_circle (center {f(cx)} {f(cy)}) (end {f(cx + r)} {f(cy)}) '
                       f'(stroke (width {f(w)}) (type solid)) (fill none) (layer {q(layer)}) (uuid {u}))')
    return out


def pad_sexpr(pad, rot, net, k):
    num, kind, shape, x, y, sx, sy, drill = pad
    netpart = f' (net {net[0]} {q(net[1])})' if net else ''
    u = q(uid('pad', k, num, x, y))
    at = f'(at {f(x)} {f(y)}{" " + f(rot) if rot else ""})'
    if kind == 'thru':
        return (f'(pad {q(num)} thru_hole {shape} {at} (size {f(sx)} {f(sy)}) (drill {f(drill)}) '
                f'(layers "*.Cu" "*.Mask") (remove_unused_layers no){netpart} (uuid {u}))')
    if kind == 'npth':
        return (f'(pad "" np_thru_hole circle {at} (size {f(sx)} {f(sy)}) (drill {f(drill)}) '
                f'(layers "*.Cu" "*.Mask") (uuid {u}))')
    extra = ' (roundrect_rratio 0.25)' if shape == 'roundrect' else ''
    layers = '"F.Cu" "F.Mask"' if shape == 'circle' else '"F.Cu" "F.Paste" "F.Mask"'
    return (f'(pad {q(num)} smd {shape} {at} (size {f(sx)} {f(sy)}) (layers {layers}){extra}'
            f'{netpart} (uuid {u}))')


def fp_body(name, fp, ref, value, rot, nets, k, board, ref_layer='F.SilkS', extra=None):
    o = []
    hide_ref = ' hide' if fp.get('hide_ref') else ''
    rx, ry = fp['ref_at']
    vx, vy = fp['val_at']
    ra = f' {f(rot)}' if rot else ''
    o.append(f'(property "Reference" {q(ref)} (at {f(rx)} {f(ry)}{ra}) (layer {q(ref_layer)}) '
             f'(uuid {q(uid("ref", k))}) (effects (font (size 1 1) (thickness 0.15)){hide_ref}))')
    o.append(f'(property "Value" {q(value)} (at {f(vx)} {f(vy)}{ra}) (layer "F.Fab") '
             f'(uuid {q(uid("val", k))}) (effects (font (size 1 1) (thickness 0.15))))')
    o.append(f'(property "Footprint" {q(LIB + ":" + name)} (at 0 0{ra}) (layer "F.Fab") hide '
             f'(uuid {q(uid("fpp", k))}) (effects (font (size 1.27 1.27) (thickness 0.15))))')
    o.append(f'(property "Datasheet" "" (at 0 0{ra}) (layer "F.Fab") hide '
             f'(uuid {q(uid("dsp", k))}) (effects (font (size 1.27 1.27) (thickness 0.15))))')
    o.append(f'(property "Description" {q(fp["descr"])} (at 0 0{ra}) (layer "F.Fab") hide '
             f'(uuid {q(uid("dep", k))}) (effects (font (size 1.27 1.27) (thickness 0.15))))')
    for fk, fv in (extra or {}).items():
        o.append(f'(property {q(fk)} {q(fv)} (at 0 0{ra}) (layer "F.Fab") hide '
                 f'(uuid {q(uid("xp", k, fk))}) (effects (font (size 1.27 1.27) (thickness 0.15))))')
    if fp.get('net_tie'):
        o.append(f'(net_tie_pad_groups {q(fp["net_tie"])})')
    o.append(f'(attr {fp["attr"]})')
    o += gfx_sexpr(fp['gfx'], k)
    for i, (t, x, y, s, layer) in enumerate(fp.get('texts', [])):
        o.append(f'(fp_text user {q(t)} (at {f(x)} {f(y)}{ra}) (layer {q(layer)}) '
                 f'(uuid {q(uid("fpt", k, i))}) (effects (font (size {f(s)} {f(s)}) (thickness 0.12))))')
    for pad in fp['pads']:
        o.append(pad_sexpr(pad, rot, nets.get(pad[0]) if nets else None, k))
    return o


def write_fp_lib():
    d = os.path.join(OUT, LIB + '.pretty')
    os.makedirs(d, exist_ok=True)
    for name, fp in D.FOOTPRINTS.items():
        o = [f'(footprint {q(name)} (version 20240108) (generator "pcbnew") (generator_version "8.0") '
             f'(layer "F.Cu") (descr {q(fp["descr"])})']
        o += fp_body(name, fp, 'REF**', name, 0, None, 'lib-' + name, False)
        o.append(')')
        with open(os.path.join(d, name + '.kicad_mod'), 'w', encoding='utf-8', newline='\n') as fh:
            fh.write('\n'.join(o) + '\n')


# ================================================================== routing
def do_route(log):
    b = R.Board(D.BOARD_W, D.BOARD_H, D.G)
    pad_objs = {}     # net -> list of Obj (pads)
    for p in D.PARTS:
        for (num, kind, shape, x, y, sx, sy, drill) in abs_pads(p):
            net = p['pins'].get(num) if num else None
            layers = (0,) if kind == 'smd' else (0, 1)
            if kind == 'npth':
                o = R.Obj('circle', (x, y, 3.0), -1, (0, 1))   # screw head keep-out
            elif shape == 'circle':
                o = R.Obj('circle', (x, y, sx / 2), net or f'NC-{p["ref"]}-{num}', layers)
            else:
                o = R.Obj('rect', (x - sx / 2, y - sy / 2, x + sx / 2, y + sy / 2),
                          net or f'NC-{p["ref"]}-{num}', layers)
            b.add(o)
            if net:
                pad_objs.setdefault(net, []).append((p['ref'], num, o))
    # net tie copper bar: each half belongs to its own pad's net
    nt = next(p for p in D.PARTS if p['ref'] == 'NT1')
    b.add(R.Obj('seg', (nt['x'] - 0.75, nt['y'], nt['x'], nt['y'], 0.15), 'GND_FAN', (0,)))
    b.add(R.Obj('seg', (nt['x'], nt['y'], nt['x'] + 0.75, nt['y'], 0.15), 'GND', (0,)))

    # pre-route: join the two right-hand DevKit rows (25.4 and 22.86 mm)
    u1 = next(p for p in D.PARTS if p['ref'] == 'U1')
    for i in range(15):
        net = u1['pins'].get(str(16 + i)) or f'NC-U1-{16 + i}'
        if net:
            y = u1['y'] + i * 2.54
            b.add_track(u1['x'] + D.ROW_SPACING_ALT, y, u1['x'] + D.ROW_SPACING, y,
                        min(D.NET_WIDTH.get(net, D.DEFAULT_WIDTH), 0.5), 0, net)

    order = ['BAT_IN', 'BAT_SW', '+5V', 'FAN_5V', 'GND_FAN', '+3V3', '+5V_S']
    sig = sorted([n for n in pad_objs if n not in order and n != 'GND'],
                 key=lambda n: span(pad_objs[n]))
    order += sig + ['GND']
    failed = []
    for net in order:
        w = D.NET_WIDTH.get(net, D.DEFAULT_WIDTH)
        lc = (1.0, 1.3) if net in ('+5V', 'FAN_5V', 'GND_FAN', 'BAT_IN', 'BAT_SW') else (1.0, 1.8)
        if net == 'GND':
            lc = (1.0, 1.1)
        t0 = time.time()
        ok = route_net(b, net, w, lc)
        log(f'{net:10s} {"ok" if ok else "FAILED"}  {time.time() - t0:5.1f}s')
        if not ok:
            failed.append(net)
    return b, failed


def span(objs):
    xs = [o.bbox()[0] for _, _, o in objs]
    ys = [o.bbox()[1] for _, _, o in objs]
    return (max(xs) - min(xs)) + (max(ys) - min(ys))


def route_net(b, net, w, lc):
    # components: start with copper objects of this net grouped by touching
    objs = [o for o in b.objs if o.net == net]
    comp = {id(o): i for i, o in enumerate(objs)}

    def touching(a, c):
        # cheap test: some sample point of a lies inside c
        x1, y1, x2, y2 = a.bbox()
        for px, py in ((x1 + x2) / 2, (y1 + y2) / 2), (x1 + 0.05, (y1 + y2) / 2), \
                (x2 - 0.05, (y1 + y2) / 2), ((x1 + x2) / 2, y1 + 0.05), ((x1 + x2) / 2, y2 - 0.05):
            if c.dist(px, py) <= 0 and set(a.layers) & set(c.layers):
                return True
        return False

    def merge(a, c):
        ca, cc = comp[id(a)], comp[id(c)]
        if ca != cc:
            for k, v in comp.items():
                if v == cc:
                    comp[k] = ca

    for a in objs:
        for c in objs:
            if a is not c and touching(a, c):
                merge(a, c)
    while True:
        groups = {}
        for o in objs:
            groups.setdefault(comp[id(o)], []).append(o)
        if len(groups) <= 1:
            return True
        # try the smallest-span component first
        cand = sorted(groups.values(), key=lambda g: (len(g), 0))
        done = False
        for g in cand:
            path = b.route(net, g, w, lc)
            if path is None:
                continue
            before = len(b.objs)
            b.commit(path, w, net)
            new = b.objs[before:]
            for o in new:
                objs.append(o)
                comp[id(o)] = comp[id(g[0])]
            # find the component reached by the path end
            if path:
                L, ix, iy = path[-1]
                px, py = ix * b.g, iy * b.g
            else:
                # source already overlaps a target
                L, px, py = None, None, None
            for o in objs:
                if comp[id(o)] == comp[id(g[0])]:
                    continue
                if L is None:
                    if any(touching(s, o) or touching(o, s) for s in g):
                        merge(g[0], o)
                elif L in o.layers and o.dist(px, py) <= 0:
                    merge(g[0], o)
            done = True
            break
        if not done:
            return False


# ================================================================== board file
def write_board(b):
    nets = ['']
    for p in D.PARTS:
        for n in p['pins'].values():
            if n and n not in nets:
                nets.append(n)
    netid = {n: i for i, n in enumerate(nets)}
    # unconnected symbol pins get KiCad's own 'unconnected-(...)' net names
    nc_nets, extra_nets = {}, []
    for p in D.PARTS:
        pins = sym_geom(p['sym'])[0]
        for num, pname, *_ in pins:
            if p['pins'].get(num) is None:
                nm = f'unconnected-({p["ref"]}-{pname}-Pad{num})'
                nid = len(nets) + len(extra_nets)
                extra_nets.append(nm)
                nc_nets.setdefault(p['ref'], {})[num] = (nm, nid)
    o = [f'(kicad_pcb (version 20240108) (generator "pcbnew") (generator_version "8.0")',
         '(general (thickness 1.6) (legacy_teardrops no))', '(paper "A4")', title_block(),
         '(layers (0 "F.Cu" signal) (31 "B.Cu" signal) (32 "B.Adhes" user "B.Adhesive") '
         '(33 "F.Adhes" user "F.Adhesive") (34 "B.Paste" user) (35 "F.Paste" user) '
         '(36 "B.SilkS" user "B.Silkscreen") (37 "F.SilkS" user "F.Silkscreen") (38 "B.Mask" user) '
         '(39 "F.Mask" user) (40 "Dwgs.User" user "User.Drawings") (41 "Cmts.User" user "User.Comments") '
         '(42 "Eco1.User" user "User.Eco1") (43 "Eco2.User" user "User.Eco2") (44 "Edge.Cuts" user) '
         '(45 "Margin" user) (46 "B.CrtYd" user "B.Courtyard") (47 "F.CrtYd" user "F.Courtyard") '
         '(48 "B.Fab" user) (49 "F.Fab" user))',
         '(setup (pad_to_mask_clearance 0) (allow_soldermask_bridges_in_footprints no) '
         '(pcbplotparams (layerselection 0x00010fc_ffffffff) (plot_on_all_layers_selection 0x0000000_00000000) '
         '(disableapertmacros no) (usegerberextensions no) (usegerberattributes yes) '
         '(usegerberadvancedattributes yes) (creategerberjobfile yes) (dashed_line_dash_ratio 12.000000) '
         '(dashed_line_gap_ratio 3.000000) (svgprecision 4) (plotframeref no) (viasonmask no) (mode 1) '
         '(useauxorigin no) (hpglpennumber 1) (hpglpenspeed 20) (hpglpendiameter 15.000000) '
         '(pdf_front_fp_property_popups yes) (pdf_back_fp_property_popups yes) (dxfpolygonmode yes) '
         '(dxfimperialunits yes) (dxfusepcbnewfont yes) (psnegative no) (psa4output no) '
         '(plotreference yes) (plotvalue yes) (plotfptext yes) (plotinvisibletext no) '
         '(sketchpadsonfab no) (subtractmaskfromsilk no) (outputformat 1) (mirror no) (drillshape 0) '
         '(scaleselection 1) (outputdirectory "gerbers/")))']
    for n, i in netid.items():
        o.append(f'(net {i} {q("/" + n if n else "")})')
    for i, nm in enumerate(extra_nets):
        o.append(f'(net {len(nets) + i} {q(nm)})')
    for p in D.PARTS:
        fp = D.FOOTPRINTS[p['fp']]
        k = 'pcb-' + p['ref']
        nets_ = {num: (netid[n], '/' + n) for num, n in p['pins'].items() if n}
        for num, (pname, nid) in nc_nets.get(p['ref'], {}).items():
            nets_[num] = (nid, pname)
        o.append(f'(footprint {q(LIB + ":" + p["fp"])} (layer "F.Cu") (uuid {q(uid("fp", p["ref"]))}) '
                 f'(at {f(p["x"] + OX)} {f(p["y"] + OY)}{" " + f(p["rot"]) if p["rot"] else ""})')
        o.append(f'(descr {q(fp["descr"])})')
        o.append(f'(path {q("/" + uid("sym", p["ref"]))}) (sheetname "Root") (sheetfile {q(NAME + ".kicad_sch")})')
        rl = 'F.Fab' if p['ref'].startswith('J') else 'F.SilkS'
        ex = {'Part': p['mpn']} if p.get('mpn') else None
        fpd = dict(fp, descr=D.SYMBOLS[p['sym']][4])
        o += fp_body(p['fp'], fpd, p['ref'], p['value'], p['rot'], nets_, k, True, rl, ex)
        o.append(')')
    # edge
    o.append(f'(gr_rect (start {f(OX)} {f(OY)}) (end {f(OX + D.BOARD_W)} {f(OY + D.BOARD_H)}) '
             f'(stroke (width 0.1) (type default)) (fill none) (layer "Edge.Cuts") (uuid {q(uid("edge"))}))')
    for i, (t, x, y, s, j) in enumerate(D.SILK):
        just = '' if j == 'center' else f' (justify {j})'
        o.append(f'(gr_text {q(t)} (at {f(x + OX)} {f(y + OY)}) (layer "F.SilkS") (uuid {q(uid("silk", i))}) '
                 f'(effects (font (size {f(s)} {f(s)}) (thickness {f(max(0.1, s * 0.15))})){just}))')
    for n_ref, d_ in nc_nets.items():
        for num, (nm, nid) in d_.items():
            netid[f'NC-{n_ref}-{num}'] = nid
    for i, (x1, y1, x2, y2, w, L, net) in enumerate(b.tracks):
        o.append(f'(segment (start {f(x1 + OX)} {f(y1 + OY)}) (end {f(x2 + OX)} {f(y2 + OY)}) '
                 f'(width {f(w)}) (layer {q("F.Cu" if L == 0 else "B.Cu")}) (net {netid[net]}) '
                 f'(uuid {q(uid("trk", i, x1, y1, x2, y2))}))')
    for i, (x, y, net) in enumerate(b.vias):
        o.append(f'(via (at {f(x + OX)} {f(y + OY)}) (size {R.VIA_D}) (drill {R.VIA_DRILL}) '
                 f'(layers "F.Cu" "B.Cu") (net {netid[net]}) (uuid {q(uid("via", i, x, y))}))')
    # GND pour on the bottom layer
    m = 0.3
    pts = [(m, m), (D.BOARD_W - m, m), (D.BOARD_W - m, D.BOARD_H - m), (m, D.BOARD_H - m)]
    o.append(f'(zone (net {netid["GND"]}) (net_name "/GND") (layer "B.Cu") (uuid {q(uid("zone-gnd"))}) '
             f'(hatch edge 0.5) (connect_pads (clearance 0.3)) (min_thickness 0.25) '
             f'(filled_areas_thickness no) (fill yes (thermal_gap 0.5) (thermal_bridge_width 0.5) '
             f'(island_removal_mode 0)) (polygon (pts '
             + ' '.join(f'(xy {f(x + OX)} {f(y + OY)})' for x, y in pts) + ')))')
    o.append(')')
    with open(os.path.join(OUT, NAME + '.kicad_pcb'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(o) + '\n')
    return netid


def write_project():
    classes = [
        dict(name='Default', clearance=0.2, track_width=0.25, via_diameter=0.6, via_drill=0.3,
             microvia_diameter=0.3, microvia_drill=0.1, diff_pair_width=0.2, diff_pair_gap=0.25,
             diff_pair_via_gap=0.25, wire_width=6, bus_width=12, line_style=0, pcb_color='rgba(0, 0, 0, 0.000)',
             schematic_color='rgba(0, 0, 0, 0.000)', priority=2147483647),
        dict(name='Power', clearance=0.2, track_width=0.8, via_diameter=0.8, via_drill=0.4,
             microvia_diameter=0.3, microvia_drill=0.1, diff_pair_width=0.2, diff_pair_gap=0.25,
             diff_pair_via_gap=0.25, wire_width=6, bus_width=12, line_style=0, pcb_color='rgba(0, 0, 0, 0.000)',
             schematic_color='rgba(0, 0, 0, 0.000)', priority=0),
    ]
    pro = {
        'meta': {'filename': NAME + '.kicad_pro', 'version': 1},
        'board': {'design_settings': {
            'defaults': {},
            'rules': {'min_clearance': 0.2, 'min_track_width': 0.2, 'min_via_diameter': 0.5,
                      'min_through_hole_diameter': 0.3, 'min_copper_edge_clearance': 0.3,
                      'min_hole_clearance': 0.25, 'min_hole_to_hole': 0.25,
                      'solder_mask_to_copper_clearance': 0.0, 'min_text_height': 0.5,
                      'min_text_thickness': 0.08},
            'track_widths': [0.0, 0.25, 0.4, 0.5, 0.8],
            'via_dimensions': [{'diameter': 0.0, 'drill': 0.0}, {'diameter': 0.6, 'drill': 0.3}],
        }},
        'net_settings': {'classes': classes, 'meta': {'version': 3},
                         'netclass_patterns': [{'netclass': 'Power', 'pattern': n}
                                               for n in ('+5V', 'FAN_5V', 'GND_FAN', 'BAT_IN', 'BAT_SW')]},
        'schematic': {'legacy_lib_dir': '', 'legacy_lib_list': []},
        'sheets': [[ROOT, 'Root']],
        'text_variables': {},
    }
    with open(os.path.join(OUT, NAME + '.kicad_pro'), 'w', encoding='utf-8', newline='\n') as fh:
        json.dump(pro, fh, indent=2)
    with open(os.path.join(OUT, 'sym-lib-table'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(f'(sym_lib_table\n  (version 7)\n  (lib (name "{LIB}")(type "KiCad")'
                 f'(uri "${{KIPRJMOD}}/{LIB}.kicad_sym")(options "")(descr "Smart Room carrier symbols"))\n)\n')
    with open(os.path.join(OUT, 'fp-lib-table'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(f'(fp_lib_table\n  (version 7)\n  (lib (name "{LIB}")(type "KiCad")'
                 f'(uri "${{KIPRJMOD}}/{LIB}.pretty")(options "")(descr "Smart Room carrier footprints"))\n)\n')


def write_preview(b):
    """SVG preview of the routed board (for a quick look without KiCad)."""
    s = 8
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{D.BOARD_W * s}" height="{D.BOARD_H * s}" '
         f'viewBox="-1 -1 {D.BOARD_W + 2} {D.BOARD_H + 2}" style="background:#0b2a17">',
         f'<rect x="0" y="0" width="{D.BOARD_W}" height="{D.BOARD_H}" fill="#123d22" stroke="#ff0" stroke-width="0.1"/>']
    for x1, y1, x2, y2, w, L, net in b.tracks:
        if L == 1:
            o.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#3a7bd5" stroke-opacity="0.8" '
                     f'stroke-width="{w}" stroke-linecap="round"/>')
    for x1, y1, x2, y2, w, L, net in b.tracks:
        if L == 0:
            o.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#d64541" stroke-opacity="0.85" '
                     f'stroke-width="{w}" stroke-linecap="round"/>')
    for p in D.PARTS:
        for (num, kind, shape, x, y, sx, sy, drill) in abs_pads(p):
            col = '#c9a227' if kind != 'npth' else '#222'
            if shape == 'circle':
                o.append(f'<circle cx="{x}" cy="{y}" r="{sx / 2}" fill="{col}"/>')
            else:
                o.append(f'<rect x="{x - sx / 2}" y="{y - sy / 2}" width="{sx}" height="{sy}" fill="{col}"/>')
            if drill:
                o.append(f'<circle cx="{x}" cy="{y}" r="{drill / 2}" fill="#111"/>')
        o.append(f'<text x="{p["x"]}" y="{p["y"] - 1.2}" font-size="1.1" fill="#fff" '
                 f'font-family="sans-serif">{p["ref"]}</text>')
    for x, y, net in b.vias:
        o.append(f'<circle cx="{x}" cy="{y}" r="0.3" fill="#ddd"/><circle cx="{x}" cy="{y}" r="0.15" fill="#111"/>')
    for t, x, y, sz, j in D.SILK:
        anchor = {'center': 'middle', 'left': 'start', 'right': 'end'}[j]
        o.append(f'<text x="{x}" y="{y + sz * 0.35}" font-size="{sz}" fill="#eee" text-anchor="{anchor}" '
                 f'font-family="sans-serif">{t}</text>')
    o.append('</svg>')
    with open(os.path.join(OUT, 'preview.svg'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(o))


def main():
    os.makedirs(OUT, exist_ok=True)
    write_project()
    write_symbol_lib()
    write_fp_lib()
    write_schematic()
    b, failed = do_route(print)
    write_board(b)
    write_preview(b)
    print('tracks', len(b.tracks), 'vias', len(b.vias), 'failed', failed)


if __name__ == '__main__':
    main()
