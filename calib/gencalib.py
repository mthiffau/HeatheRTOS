#!/usr/bin/env python

# NOTE: input files are as follows:
#
# ??-vel.in:
#   velocities for speeds 0 through 13 in um/tick (one per line)
#
# ??-stopdist.in:
#   Stopping distances for speeds 0 through 13 in mm.
#   Each is paired with the velocity (which comes first) in um/tick.
#   One entry per line.
#
# ??-accel.in:
#   Acceleration curve. One (t, x) sample per line.
#   t is in ticks, x is in mm.
#
# ??-stop.in:
#   Stopping curve. One (t, x) sample per line.
#   t is in ticks, x is in mm.
#
# ??-delay.in:
#   A single line with number of ticks before motion starts from a standstill
#
# ??-acutoff.in:
#   Cutoff times for acceleration curve for speeds 0 through 13, in ticks.
#   One per line.

import sys
import os
import subprocess
import re

SENSOR_DELAY_TICKS = 3

STOPDIST_DEGREE    = 2
ACCEL_DEGREE       = 4
STOP_DEGREE        = 4

def die(msg):
    print('error:', msg, file=sys.stderr)
    sys.exit(1)

def list_trains():
    filename_re = re.compile(
        r'(?P<train>\d\d)-(?:vel|stopdist|accel|decel|delay|acutoff)\.in')
    files  = os.listdir('.')
    trains = set()
    for filename in files:
        md = filename_re.match(filename)
        if not md:
            continue
        trains.add(int(md.group('train')))
    return sorted(trains)

def read_vel(train):
    vel = []
    with open('{:02d}-vel.in'.format(train)) as f:
        for line in f:
            vel.append(int(line))
    if len(vel) != 14:
        die('wrong number of velocities for train {:02d}'.format(train))
    return vel

def read_stopdist(train):
    stopdist = []
    with open('{:02d}-stopdist.in'.format(train)) as f:
        for line in f:
            v, x = map(int, line.split())
            stopdist.append((v, 1000 * x))
    if len(stopdist) != 14:
        die('wrong number of stopping distances for train {:02d}'.format(train))
    return stopdist

def fit_stopdist(train, stopdist):
    adjusted = []
    for v, x in stopdist:
        x -= SENSOR_DELAY_TICKS * v
        adjusted.append('{}, {}\n'.format(v, x))
    p = subprocess.Popen(
        ['node', '../polysolve.js', str(STOPDIST_DEGREE)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        universal_newlines=True)
    stdout, stderr = p.communicate(input=''.join(adjusted))
    coeffs = list(map(float, stdout.split()))
    if len(coeffs) != STOPDIST_DEGREE + 1:
        die('stop distance fitting failed for train {:02d}'.format(train))
    return coeffs

def read_accel(train):
    curve = []
    with open('{:02d}-accel.in'.format(train)) as f:
        for line in f:
            t, x = map(int, line.split())
            curve.append((t, 1000 * x))
    return curve

def fit_accel(train, accel):
    data_str = ''.join('{}, {}\n'.format(t, x) for t, x in accel)
    p = subprocess.Popen(
        ['node', '../polysolve.js', str(ACCEL_DEGREE)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        universal_newlines=True)
    stdout, stderr = p.communicate(input=data_str)
    coeffs = list(map(float, stdout.split()))
    if len(coeffs) != ACCEL_DEGREE + 1:
        die('acceleration curve fitting failed for train {:02d}'.format(train))
    return coeffs

def read_acutoff(train):
    cutoff = []
    with open('{:02d}-acutoff.in'.format(train)) as f:
        for line in f:
            cutoff.append(int(line))
    if len(cutoff) != 14:
        die('wrong number of cutoff times for train {:02d}'.format(train))
    return cutoff

def read_delay(train):
    with open('{:02d}-delay.in'.format(train)) as f:
        return int(f.read())

def read_stop(train):
    curve = []
    with open('{:02d}-stop.in'.format(train)) as f:
        for line in f:
            t, x = map(int, line.split())
            curve.append((t, 1000 * x))
    return curve

def fit_stop(train, stop):
    data_str = ''.join('{}, {}\n'.format(t, x) for t, x in stop)
    p = subprocess.Popen(
        ['node', '../polysolve.js', str(STOP_DEGREE)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        universal_newlines=True)
    stdout, stderr = p.communicate(input=data_str)
    coeffs = list(map(float, stdout.split()))
    if len(coeffs) != STOP_DEGREE + 1:
        die('stopping curve fitting failed for train {:02d}'.format(train))
    return coeffs

def main():
    for train in list_trains():
        vel      = read_vel(train)
        stopdist = fit_stopdist(train, read_stopdist(train))
        accel    = fit_accel(train, read_accel(train))
        delay    = read_delay(train)
        acutoff  = read_acutoff(train)
        stop     = fit_stop(train, read_stop(train))

        # Generate us some C code
        print('{')
        print('    .train_id = {},'.format(train))
        print('    .calib = {')
        # Velocities at each speed
        print('        .vel_umpt = {')
        for v in vel:
            print('            {},'.format(v))
        print('        },')
        # Stopping distance polynomial
        print('        .stop_um = {')
        print('            .deg = {},'.format(len(stopdist) - 1))
        print('            .a = {')
        for coeff in stopdist:
            print('                {},'.format(coeff))
        print('            }')
        print('        },')
        # Acceleration position polynomial
        print('        .accel = {')
        print('            .deg = {},'.format(len(accel) - 1))
        print('            .a = {')
        for coeff in accel:
            print('                {},'.format(coeff))
        print('            }')
        print('        },')
        # Acceleration delay
        print('        .accel_delay = {},'.format(delay))
        # Acceleration curve cutoff times
        print('        .accel_cutoff = {')
        for t in acutoff:
            print('            {},'.format(t))
        print('        },')
        # Acceleration position polynomial
        print('        .stop = {')
        print('            .deg = {},'.format(len(stop) - 1))
        print('            .a = {')
        for coeff in stop:
            print('                {},'.format(coeff))
        print('            }')
        print('        },')
        # Done
        print('    },')
        print('},')

if __name__ == '__main__':
    main()
