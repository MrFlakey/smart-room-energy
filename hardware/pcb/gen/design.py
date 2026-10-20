"""Smart Room carrier board: parts, nets and placement.

Author: Mohamed Khamis

Source of truth for the pins: docs/esp32-wiring.md and firmware/main/board_pins.h.
Every module plugs into a 3-pin header (G, V, S). A 3-pin jumper next to it picks
the module's VCC: 5V (jumper on the two pins nearest "5V") or 3V3.
Every signal reaches the ESP32 through a series resistor and a BAT54S clamp to
GND and 3V3, so a module on 5 V can't push a GPIO above about 3.6 V.
"""

G = 0.254  # placement and routing grid (mm)


def snap(v):
    return round(round(v / G) * G, 4)


# ---------------------------------------------------------------- footprints
# Each footprint: pads = [(number, kind, shape, x, y, sx, sy, drill)], plus
# silk/fab/courtyard graphics as ('line'|'rect'|'circle', layer, params...).

def _header(n):
    pads = []
    for i in range(n):
        pads.append((str(i + 1), 'thru', 'rect' if i == 0 else 'circle',
                     0.0, i * 2.54, 1.7, 1.7, 1.0))
    h = (n - 1) * 2.54
    gfx = [('rect', 'F.SilkS', -1.33, -1.33, 1.33, h + 1.33, 0.12),
           ('rect', 'F.CrtYd', -1.8, -1.8, 1.8, h + 1.8, 0.05),
           ('rect', 'F.Fab', -1.27, -1.27, 1.27, h + 1.27, 0.1)]
    return dict(pads=pads, gfx=gfx, attr='through_hole',
                ref_at=(0, -2.4), val_at=(0, h + 2.4),
                descr=f'Pin header 1x{n:02d}, 2.54 mm pitch')


def _two_pad_0805():
    pads = [('1', 'smd', 'roundrect', -1.0, 0.0, 1.2, 1.4, 0),
            ('2', 'smd', 'roundrect', 1.0, 0.0, 1.2, 1.4, 0)]
    gfx = [('line', 'F.SilkS', -0.25, -0.86, 0.25, -0.86, 0.12),
           ('line', 'F.SilkS', -0.25, 0.86, 0.25, 0.86, 0.12),
           ('rect', 'F.CrtYd', -1.85, -0.95, 1.85, 0.95, 0.05),
           ('rect', 'F.Fab', -1.0, -0.625, 1.0, 0.625, 0.1)]
    return dict(pads=pads, gfx=gfx, attr='smd', ref_at=(0, -1.75), val_at=(0, 1.75),
                descr='0805 (2012 metric), hand-solder pads')


def _sot23():
    pads = [('1', 'smd', 'roundrect', -0.9375, -0.95, 1.475, 0.6, 0),
            ('2', 'smd', 'roundrect', -0.9375, 0.95, 1.475, 0.6, 0),
            ('3', 'smd', 'roundrect', 0.9375, 0.0, 1.475, 0.6, 0)]
    gfx = [('line', 'F.SilkS', 0.76, -1.56, 0.76, -0.65, 0.12),
           ('line', 'F.SilkS', 0.76, 1.56, 0.76, 0.65, 0.12),
           ('line', 'F.SilkS', -0.7, -1.56, 0.76, -1.56, 0.12),
           ('line', 'F.SilkS', -0.7, 1.56, 0.76, 1.56, 0.12),
           ('rect', 'F.CrtYd', -1.92, -1.7, 1.92, 1.7, 0.05),
           ('rect', 'F.Fab', -0.65, -1.45, 0.65, 1.45, 0.1)]
    return dict(pads=pads, gfx=gfx, attr='smd', ref_at=(0, -2.5), val_at=(0, 2.5),
                descr='SOT-23')


def _cp_d8():
    pads = [('1', 'thru', 'rect', 0.0, 0.0, 1.8, 1.8, 0.9),
            ('2', 'thru', 'circle', 3.5, 0.0, 1.8, 1.8, 0.9)]
    gfx = [('circle', 'F.SilkS', 1.75, 0.0, 4.12, 0.12),
           ('circle', 'F.CrtYd', 1.75, 0.0, 4.25, 0.05),
           ('circle', 'F.Fab', 1.75, 0.0, 4.0, 0.1),
           ('line', 'F.SilkS', -2.6, -1.0, -1.6, -1.0, 0.12),   # "+" mark
           ('line', 'F.SilkS', -2.1, -1.5, -2.1, -0.5, 0.12)]
    return dict(pads=pads, gfx=gfx, attr='through_hole', ref_at=(1.75, -5.0),
                val_at=(1.75, 5.0), descr='Electrolytic, D8 mm, 3.5 mm pitch')


def _net_tie():
    pads = [('1', 'smd', 'circle', -0.75, 0.0, 1.0, 1.0, 0),
            ('2', 'smd', 'circle', 0.75, 0.0, 1.0, 1.0, 0)]
    gfx = [('line', 'F.Cu', -0.75, 0.0, 0.75, 0.0, 0.3),
           ('rect', 'F.CrtYd', -1.5, -0.75, 1.5, 0.75, 0.05)]
    return dict(pads=pads, gfx=gfx, attr='smd exclude_from_bom', ref_at=(0, -1.5), val_at=(0, 1.5),
                net_tie='1, 2', descr='Net tie (star ground point)')


def _hole():
    pads = [('', 'npth', 'circle', 0.0, 0.0, 3.2, 3.2, 3.2)]
    gfx = [('circle', 'F.CrtYd', 0.0, 0.0, 3.45, 0.05),
           ('circle', 'Cmts.User', 0.0, 0.0, 3.2, 0.15)]
    return dict(pads=pads, gfx=gfx, attr='exclude_from_pos_files exclude_from_bom',
                ref_at=(0, -4.2), val_at=(0, 4.2), descr='M3 mounting hole', hide_ref=True)


def _terminal_2():
    pads = [('1', 'thru', 'rect', 0.0, 0.0, 2.6, 2.6, 1.3),
            ('2', 'thru', 'circle', 0.0, 5.08, 2.6, 2.6, 1.3)]
    gfx = [('rect', 'F.SilkS', -3.6, -2.7, 4.2, 7.78, 0.12),
           ('line', 'F.SilkS', 3.0, -2.7, 3.0, 7.78, 0.12),     # wire-entry side
           ('rect', 'F.CrtYd', -3.9, -3.0, 4.3, 8.08, 0.05),
           ('rect', 'F.Fab', -3.5, -2.6, 4.1, 7.68, 0.1)]
    return dict(pads=pads, gfx=gfx, attr='through_hole', ref_at=(0, -3.8), val_at=(0, 9.0),
                descr='Screw terminal 1x02, 5.08 mm pitch, wire entry on the +x side')


def _d_sma():
    pads = [('1', 'smd', 'roundrect', -2.0, 0.0, 2.5, 1.8, 0),
            ('2', 'smd', 'roundrect', 2.0, 0.0, 2.5, 1.8, 0)]
    gfx = [('line', 'F.SilkS', -3.51, -1.65, 2.0, -1.65, 0.12),
           ('line', 'F.SilkS', -3.51, 1.65, 2.0, 1.65, 0.12),
           ('line', 'F.SilkS', -3.51, -1.65, -3.51, 1.65, 0.12),   # cathode side
           ('rect', 'F.CrtYd', -3.5, -1.75, 3.5, 1.75, 0.05),
           ('rect', 'F.Fab', -2.25, -1.4, 2.25, 1.4, 0.1)]
    return dict(pads=pads, gfx=gfx, attr='smd', ref_at=(0, -2.6), val_at=(0, 2.6),
                descr='SMA (DO-214AC), pad 1 = cathode')


# ESP32 DevKit V1, 30 pins, viewed from the top, antenna up, USB down.
DEVKIT_LEFT = ['EN', 'VP', 'VN', 'D34', 'D35', 'D32', 'D33', 'D25', 'D26', 'D27',
               'D14', 'D12', 'D13', 'GND', 'VIN']
DEVKIT_RIGHT = ['D23', 'D22', 'TX0', 'RX0', 'D21', 'D19', 'D18', 'D5', 'TX2', 'RX2',
                'D4', 'D2', 'D15', 'GND', '3V3']
ROW_SPACING = 25.4      # official spacing
ROW_SPACING_ALT = 22.86  # common clone spacing


def _devkit():
    pads, gfx = [], []
    for i in range(15):
        pads.append((str(i + 1), 'thru', 'rect' if i == 0 else 'circle',
                     0.0, i * 2.54, 1.7, 1.7, 1.0))
    for i in range(15):
        for x in (ROW_SPACING, ROW_SPACING_ALT):
            pads.append((str(16 + i), 'thru', 'circle', x, i * 2.54, 1.7, 1.7, 1.0))
    top, bot = -7.75, 14 * 2.54 + 7.75
    left, right = -1.5, ROW_SPACING + 1.5
    gfx += [('rect', 'F.SilkS', left, top, right, bot, 0.15),
            ('rect', 'F.CrtYd', left - 0.3, top - 0.3, right + 0.3, bot + 0.3, 0.05),
            ('rect', 'F.Fab', left, top, right, bot, 0.1),
            ('rect', 'F.SilkS', 9.0, bot - 5.5, 16.4, bot, 0.12)]   # USB connector
    texts = [('USB', 10.2 + 2.5, bot - 2.6, 1.0, 'F.SilkS'),
             ('ESP32 DevKit V1', 12.7, top + 2.0, 1.0, 'F.SilkS'),
             ('antenna', 12.7, top + 4.0, 0.8, 'F.SilkS')]
    for i, n in enumerate(DEVKIT_LEFT):
        texts.append((n, 2.3 + 1.2, i * 2.54, 0.7, 'F.SilkS'))
    for i, n in enumerate(DEVKIT_RIGHT):
        texts.append((n, ROW_SPACING_ALT - 2.3 - 1.2, i * 2.54, 0.7, 'F.SilkS'))
    return dict(pads=pads, gfx=gfx, attr='through_hole', ref_at=(12.7, top - 1.5),
                val_at=(12.7, bot + 1.5), texts=texts,
                descr='ESP32 DevKit V1 30-pin socket. Right row has holes at 25.4 mm '
                      'and 22.86 mm; solder the female header into the one that fits.')


FOOTPRINTS = {
    'PinHeader_1x02_P2.54mm': _header(2),
    'PinHeader_1x03_P2.54mm': _header(3),
    'PinHeader_1x04_P2.54mm': _header(4),
    'R_0805_HandSolder': _two_pad_0805(),
    'C_0805_HandSolder': _two_pad_0805(),
    'L_0805_HandSolder': _two_pad_0805(),
    'SOT-23': _sot23(),
    'CP_Radial_D8.0mm_P3.50mm': _cp_d8(),
    'NetTie_2_Pad1.0mm': _net_tie(),
    'MountingHole_3.2mm_M3': _hole(),
    'TerminalBlock_1x02_P5.08mm': _terminal_2(),
    'D_SMA': _d_sma(),
    'ESP32_DevKitV1_30pin_Socket': _devkit(),
}

# ---------------------------------------------------------------- symbols
# Box symbols: name -> (left pins [(num, name)], right pins, width, prefix, descr)
SYMBOLS = {
    'Conn_G_V_S': ([], [('1', 'G'), ('2', 'V'), ('3', 'S')], 5.08, 'J',
                   'Gravity 3-pin module header (GND, VCC, signal)'),
    'Conn_I2C': ([], [('1', '-'), ('2', '+'), ('3', 'C'), ('4', 'D')], 5.08, 'J',
                 'Gravity I2C header (GND, VCC, SCL, SDA)'),
    'Conn_IN': ([], [('1', 'IN+'), ('2', 'IN-')], 7.62, 'J',
                'Wattmeter power side (to the SEN0291 screw terminals)'),
    'Jumper_VCC': ([], [('1', '5V'), ('2', 'VCC'), ('3', '3V3')], 7.62, 'JP',
                   '3-pin header + jumper cap: module VCC from 5V (1-2) or 3V3 (2-3)'),
    'Jumper_2': ([('1', '~')], [('2', '~')], 5.08, 'JP', '2-pin jumper (open by default)'),
    'R': ([('1', '~')], [('2', '~')], 5.08, 'R', 'Resistor'),
    'C': ([('1', '~')], [('2', '~')], 5.08, 'C', 'Capacitor'),
    'CP': ([('1', '+')], [('2', '-')], 5.08, 'C', 'Polarised capacitor'),
    'FerriteBead': ([('1', '~')], [('2', '~')], 5.08, 'FB', 'Ferrite bead'),
    'NetTie': ([('1', '~')], [('2', '~')], 5.08, 'NT', 'Net tie: star ground point'),
    'BAT54S': ([('3', 'IO')], [('2', 'K→3V3'), ('1', 'A→GND')], 12.7, 'D',
               'Dual Schottky clamp (series pair)'),
    'BSS138': ([('2', 'S')], [('3', 'D'), ('1', 'G')], 7.62, 'Q',
               'N-MOSFET (I2C level shifter)'),
    'ESP32_DevKitV1_30': ([(str(i + 1), n) for i, n in enumerate(DEVKIT_LEFT)],
                          [(str(16 + i), n) for i, n in enumerate(DEVKIT_RIGHT)],
                          20.32, 'U', 'ESP32 DevKit V1 (30-pin) in two 1x15 female headers'),
    'MountingHole': ([], [], 5.08, 'H', 'M3 mounting hole'),
    'Conn_BAT': ([], [('1', '+'), ('2', '-')], 5.08, 'J', '5 V battery input (screw terminal)'),
    'D_Schottky': ([('2', 'A')], [('1', 'K')], 5.08, 'D',
                   'Schottky diode: reverse-polarity protection and USB/battery isolation'),
}

# ---------------------------------------------------------------- parts
PARTS = []   # dicts: ref, value, sym, fp, x, y, rot, pins{num: net}, sx, sy, mpn


def part(ref, value, sym, fp, x, y, rot, pins, sx, sy, mpn=''):
    PARTS.append(dict(ref=ref, value=value, sym=sym, fp=fp, x=snap(x), y=snap(y),
                      rot=rot, pins=pins, sx=sx, sy=sy, mpn=mpn))


BOARD_W, BOARD_H = 110.0, 82.0

# DevKit: left row at x=43.18, pins from y=33.02
DK_X, DK_Y = 43.18, 33.02
dk_pins = {}
LEFT_NETS = [None, None, None, 'IO34', 'IO35', 'IO32', 'IO33', 'IO25', 'IO26', 'IO27',
             'IO14', None, 'IO13', 'GND', '+5V']
RIGHT_NETS = ['IO23', 'IO22', None, None, 'IO21', 'IO19', None, None, None, None,
              None, None, None, 'GND', '+3V3']
for i, n in enumerate(LEFT_NETS):
    dk_pins[str(i + 1)] = n
for i, n in enumerate(RIGHT_NETS):
    dk_pins[str(16 + i)] = n
part('U1', 'ESP32 DevKit V1 (30-pin)', 'ESP32_DevKitV1_30', 'ESP32_DevKitV1_30pin_Socket',
     DK_X, DK_Y, 0, dk_pins, 50.8, 81.28, '2x 1x15 female header, 2.54 mm')


def dk_pin_xy(num):
    n = int(num)
    if n <= 15:
        return DK_X, DK_Y + (n - 1) * 2.54
    return DK_X + ROW_SPACING, DK_Y + (n - 16) * 2.54


# Module channels: key, label, sku, gpio, kind, default VCC, side, slot
CHANNELS = [
    ('TEMP', 'TEMP', 'DFR0023', 34, 'ain', '5V', 'top', 0),
    ('LIGHT', 'LIGHT', 'DFR0026', 35, 'ain', '3V3', 'top', 1),
    ('ROT', 'ROT', 'DFR0058', 32, 'ain', '3V3', 'top', 2),
    ('US', 'ULTRA', 'SEN0307', 33, 'ain', '3V3', 'top', 3),
    ('PIR', 'PIR', 'SEN0018', 27, 'din', '3V3', 'top', 4),
    ('BTN3', 'BTN3 A/M', 'DFR0029', 23, 'din', '3V3', 'top', 5),
    ('LED1', 'LED1', 'DFR0021', 26, 'out', '3V3', 'left', 0),
    ('BTN1', 'BTN1 LIGHT', 'DFR0029', 14, 'din', '3V3', 'left', 1),
    ('BTN2', 'BTN2 AC', 'DFR0029', 13, 'din', '3V3', 'left', 2),
    ('FAN', 'FAN', 'DFR0411', 25, 'fan', 'metered 5V', 'left', 3),
    ('LED2', 'LED2', 'DFR0021', 19, 'out', '3V3', 'right', 0),
]

SILK = []    # (text, x, y, size, justify)
counters = {'J': 0, 'JP': 0, 'R': 0, 'D': 0, 'C': 0}


def nref(p):
    counters[p] += 1
    return f'{p}{counters[p]}'


# schematic block grid
SCH_COLS = [127.0, 215.9, 304.8]
SCH_ROW0, SCH_ROW_DY = 30.48, 40.64


def sch_block(i):
    return SCH_COLS[i % 3], SCH_ROW0 + (i // 3) * SCH_ROW_DY


SCH_TEXT = []  # (text, x, y, size)

for ci, (key, label, sku, gpio, kind, dflt, side, slot) in enumerate(CHANNELS):
    io, sig, vcc = f'IO{gpio}', f'{key}_S', f'{key}_VCC'
    gnd = 'GND_FAN' if kind == 'fan' else 'GND'
    if kind == 'fan':
        vcc = 'FAN_5V'
    rval = '220' if kind in ('out', 'fan') else '1k'
    bx, by = sch_block(ci)
    SCH_TEXT.append((f'{label.split()[0]} - {sku} - GPIO{gpio} - VCC default: {dflt}',
                     bx - 22.86, by - 7.62, 1.5))

    if side == 'top':
        xc = 11.43 + 12.7 * slot
        jx, jy, jrot = xc - 2.54, 3.81, 90     # rot 90: pins run left -> right
        jpx, jpy = xc - 2.54, 9.398
        rx, ry, rrot = xc + 2.54, 14.732, 270
        dx, dy = xc, 19.05
        cx, cy = xc - 2.54, 14.732
        SILK.append((f'{label} {gpio}', xc, 1.3, 0.8, 'center'))
        for k, t in enumerate('GVS'):
            SILK.append((t, xc - 2.54 + 2.54 * k, 6.35, 0.7, 'center'))
        SILK.append(('5V', xc - 2.54, 11.6, 0.6, 'center'))
        SILK.append(('3V3', xc + 2.54, 11.6, 0.6, 'center'))
    elif side == 'left':
        yc = 30.48 + 10.16 * slot
        jx, jy, jrot = 3.81, yc - 2.54, 0
        jpx, jpy = 9.652, yc - 2.54
        rx, ry, rrot = 15.24, yc + 2.54, 0
        dx, dy = 20.32, yc
        cx, cy = 15.24, yc - 1.27
        SILK.append((f'{label} {gpio}', 1.0, yc - 4.6, 0.8, 'left'))
        for k, t in enumerate('GVS'):
            SILK.append((t, 6.0, yc - 2.54 + 2.54 * k, 0.7, 'center'))
        if kind != 'fan':
            SILK.append(('5V', 11.0, yc - 2.54, 0.6, 'left'))
            SILK.append(('3V3', 11.0, yc + 2.54, 0.6, 'left'))
    else:
        yc = 48.26
        jx, jy, jrot = BOARD_W - 3.81, yc - 2.54, 0
        jpx, jpy = BOARD_W - 9.652, yc - 2.54
        rx, ry, rrot = BOARD_W - 15.24, yc + 2.54, 180
        dx, dy = BOARD_W - 20.32, yc
        cx, cy = BOARD_W - 15.24, yc - 1.27
        SILK.append((f'{label} {gpio}', BOARD_W - 1.0, yc - 4.6, 0.8, 'right'))
        for k, t in enumerate('GVS'):
            SILK.append((t, BOARD_W - 6.0, yc - 2.54 + 2.54 * k, 0.7, 'center'))
        SILK.append(('5V', BOARD_W - 11.0, yc - 2.54, 0.6, 'right'))
        SILK.append(('3V3', BOARD_W - 11.0, yc + 2.54, 0.6, 'right'))

    part(nref('J'), f'{label.split()[0]} ({sku})', 'Conn_G_V_S', 'PinHeader_1x03_P2.54mm',
         jx, jy, jrot, {'1': gnd, '2': vcc, '3': sig}, bx, by, '1x03 male header 2.54 mm')
    if kind != 'fan':
        part(nref('JP'), f'{key} VCC ({dflt})', 'Jumper_VCC', 'PinHeader_1x03_P2.54mm',
             jpx, jpy, jrot, {'1': '+5V_S', '2': vcc, '3': '+3V3'}, bx, by + 15.24,
             '1x03 male header + jumper cap')
    part(nref('R'), rval, 'R', 'R_0805_HandSolder', rx, ry, rrot,
         {'1': sig, '2': io}, bx + 27.94, by, '0805 1%')
    part(nref('D'), 'BAT54S', 'BAT54S', 'SOT-23', dx, dy, 0,
         {'1': 'GND', '2': '+3V3', '3': io}, bx + 55.88, by, 'BAT54S')
    if kind == 'ain':
        part(nref('C'), '10nF', 'C', 'C_0805_HandSolder', cx, cy, 270,
             {'1': io, '2': 'GND'}, bx + 27.94, by + 10.16, '0805 X7R')

# ---- wattmeter I2C (block 11)
bx, by = sch_block(11)
SCH_TEXT.append(('WATTMETER I2C - SEN0291 - SDA GPIO21 / SCL GPIO22 - VCC default: 3V3',
                 bx - 22.86, by - 7.62, 1.5))
jw = nref('J')
part(jw, 'WATT I2C (SEN0291)', 'Conn_I2C', 'PinHeader_1x04_P2.54mm', BOARD_W - 3.81, 27.94, 0,
     {'1': 'GND', '2': 'WATT_VCC', '3': 'SCL_M', '4': 'SDA_M'}, bx, by,
     '1x04 male header 2.54 mm')
part(nref('JP'), 'WATT VCC (3V3)', 'Jumper_VCC', 'PinHeader_1x03_P2.54mm',
     BOARD_W - 9.652, 27.94, 0, {'1': '+5V_S', '2': 'WATT_VCC', '3': '+3V3'}, bx, by + 15.24,
     '1x03 male header + jumper cap')
part('Q1', 'BSS138', 'BSS138', 'SOT-23', 91.44, 35.56, 0,
     {'1': '+3V3', '2': 'IO21', '3': 'SDA_M'}, bx + 27.94, by + 12.7, 'BSS138')
part('Q2', 'BSS138', 'BSS138', 'SOT-23', 91.44, 30.48, 0,
     {'1': '+3V3', '2': 'IO22', '3': 'SCL_M'}, bx + 27.94, by + 25.4, 'BSS138')
for (net, pull, x, sx_, sy_) in [('IO22', '+3V3', 80.01, 27.94, -2.54),
                                 ('IO21', '+3V3', 83.82, 55.88, -2.54),
                                 ('SCL_M', 'WATT_VCC', 91.44, 55.88, 7.62),
                                 ('SDA_M', 'WATT_VCC', 95.25, 55.88, 17.78)]:
    part(nref('R'), '4.7k', 'R', 'R_0805_HandSolder', x, 24.13, 270,
         {'1': pull, '2': net}, bx + sx_, by + sy_, '0805 1%')
SILK += [('WATT I2C', BOARD_W - 1.0, 25.0, 0.8, 'right'),
         ('-', BOARD_W - 6.0, 27.94, 0.7, 'center'), ('+', BOARD_W - 6.0, 30.48, 0.7, 'center'),
         ('C', BOARD_W - 6.0, 33.02, 0.7, 'center'), ('D', BOARD_W - 6.0, 35.56, 0.7, 'center'),
         ('5V', BOARD_W - 11.0, 27.94, 0.6, 'right'), ('3V3', BOARD_W - 11.0, 33.02, 0.6, 'right')]

# ---- fan power, wattmeter power side, star ground (block 12, under the ESP32)
px, py = 25.4, 152.4
SCH_TEXT.append(('POWER - fan path is metered and has its own ground back to the star point',
                 px - 12.7, py - 7.62, 1.5))
part(nref('J'), 'WATT IN+/IN- (SEN0291)', 'Conn_IN', 'PinHeader_1x02_P2.54mm', 3.81, 67.31, 0,
     {'1': '+5V', '2': 'FAN_5V'}, px + 12.7, py, '1x02 male header 2.54 mm')
part(nref('JP'), 'BYPASS (no wattmeter)', 'Jumper_2', 'PinHeader_1x02_P2.54mm', 9.652, 67.31, 0,
     {'1': '+5V', '2': 'FAN_5V'}, px + 12.7, py + 12.7, '1x02 male header + jumper cap')
part(nref('C'), '470uF 10V', 'CP', 'CP_Radial_D8.0mm_P3.50mm', 15.24, 76.2, 0,
     {'1': 'FAN_5V', '2': 'GND_FAN'}, px + 12.7, py + 25.4, 'Electrolytic 470uF 10V D8')
part(nref('C'), '100nF', 'C', 'C_0805_HandSolder', 15.24, 67.31, 0,
     {'1': 'FAN_5V', '2': 'GND_FAN'}, px + 12.7, py + 35.56, '0805 X7R')
part('NT1', 'STAR GND', 'NetTie', 'NetTie_2_Pad1.0mm', 36.83, 66.04, 0,
     {'1': 'GND_FAN', '2': 'GND'}, px + 12.7, py + 45.72, '(copper only)')
part('FB1', '600R@100MHz', 'FerriteBead', 'L_0805_HandSolder', 30.48, 72.39, 0,
     {'1': '+5V', '2': '+5V_S'}, px + 50.8, py, '0805 ferrite bead, >=500 mA')
part(nref('C'), '10uF', 'C', 'C_0805_HandSolder', 30.48, 76.2, 0,
     {'1': '+5V_S', '2': 'GND'}, px + 50.8, py + 12.7, '0805 X5R 16V')
part(nref('C'), '10uF', 'C', 'C_0805_HandSolder', 78.74, 68.58, 0,
     {'1': '+3V3', '2': 'GND'}, px + 50.8, py + 25.4, '0805 X5R 16V')
part(nref('C'), '100nF', 'C', 'C_0805_HandSolder', 78.74, 64.77, 0,
     {'1': '+3V3', '2': 'GND'}, px + 50.8, py + 35.56, '0805 X7R')
SILK += [('IN+', 5.4, 67.31, 0.7, 'left'), ('IN-', 5.4, 69.85, 0.7, 'left'), ('WATT', 11.2, 64.6, 0.7, 'left'),
         ('BYPASS', 11.2, 71.6, 0.6, 'left'), ('STAR', 36.83, 67.7, 0.6, 'center')]

# ---- 5 V battery input (block 13): terminal -> BAT ON header -> SS34 -> VIN
bx, by = 127.0, 193.04
SCH_TEXT.append(('BATTERY - regulated 5 V (power bank, 4x NiMH) - SS34 blocks reverse polarity and USB back-feed',
                 bx - 22.86, by - 7.62, 1.5))
part(nref('J'), 'BAT 5V IN', 'Conn_BAT', 'TerminalBlock_1x02_P5.08mm', BOARD_W - 4.318, 57.912, 0,
     {'1': 'BAT_IN', '2': 'GND'}, bx, by, '2-pin screw terminal 5.08 mm')
part(nref('JP'), 'BAT ON', 'Jumper_2', 'PinHeader_1x02_P2.54mm', 99.06, 57.912, 0,
     {'1': 'BAT_IN', '2': 'BAT_SW'}, bx + 27.94, by, '1x02 male header + jumper cap (or external switch)')
part(nref('D'), 'SS34', 'D_Schottky', 'D_SMA', 92.964, 63.754, 0,
     {'1': '+5V', '2': 'BAT_SW'}, bx + 55.88, by, 'SS34 (3 A, 40 V Schottky)')
SILK += [('BAT 5V', BOARD_W - 1.0, 54.6, 0.8, 'right'),
         ('+', BOARD_W - 9.0, 57.912, 0.9, 'center'), ('-', BOARD_W - 9.0, 62.992, 0.9, 'center'),
         ('ON', 99.06, 55.3, 0.6, 'center')]

for i, (hx, hy) in enumerate([(3.5, 3.5), (BOARD_W - 3.5, 3.5),
                              (3.5, BOARD_H - 3.5), (BOARD_W - 3.5, BOARD_H - 3.5)]):
    PARTS.append(dict(ref=f'H{i + 1}', value='M3', sym='MountingHole',
                      fp='MountingHole_3.2mm_M3', x=hx, y=hy, rot=0, pins={},
                      sx=330.2 + 12.7 * i, sy=236.22, mpn=''))

SILK += [('SMART ROOM CARRIER v1.1', 92.0, 74.0, 1.2, 'center'),
         ('Mohamed Khamis - 2026', 92.0, 76.4, 1.0, 'center'),
         ('Jumpers: 5V side = 5V, 3V3 side = 3V3', 92.0, 78.6, 0.7, 'center'),
         ('Right row: 25.4 | 22.86', 67.31, 77.6, 0.7, 'center')]

# Net classes: name -> (track width, nets)
NET_WIDTH = {'BAT_IN': 0.8, 'BAT_SW': 0.8, '+5V': 0.8, 'FAN_5V': 0.8, 'GND_FAN': 0.8, '+5V_S': 0.5, '+3V3': 0.5,
             'GND': 0.4}
DEFAULT_WIDTH = 0.25
