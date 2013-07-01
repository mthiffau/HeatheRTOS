#!/usr/bin/env python
from __future__ import print_function
import sys
import re

if len(sys.argv) != 2:
    print('usage: {} NAME'.format(sys.argv[0]), file=sys.stderr)
    sys.exit(1)

track_name = sys.argv[1]

sensors        = []
sensors_by_num = { }
switches       = []
enters         = []
nodes_by_name  = { }
dists_by_name  = { }
dists          = { }

sensor_re = re.compile(
    r'^sensor\s+' +
    r'(?P<name>\w+)\s*:\s*(?P<num>\d+)\s*->\s*(?P<ahead>\w+)' +
    r'\s*@\s*'
    r'(?P<rev_name>\w+)\s*:\s*(?P<rev_num>\d+)\s*->\s*(?P<rev_ahead>\w+)' +
    r'$')

switch_re = re.compile(
    r'^switch\s+' +
    r'(?P<name>\w+)\s*:\s*(?P<num>\d+)' +
    r'\s*->\s*(?P<straight>\w+)' +
    r'\s*/\s*\(\s*(?P<curved>\w+)\s*\)'
    r'\s*@\s*'
    r'(?P<merge_name>\w+)\s*->\s*(?P<merge_ahead>\w+)' +
    r'$')

enter_re = re.compile(
    r'^enter\s+' +
    r'(?P<name>\w+)\s*->\s*(?P<ahead>\w+)' +
    r'\s*@\s*' +
    r'(?P<exit_name>\w+)$')

dist_re = re.compile(
    r'^dist\s+' +
    r'(?P<src>\w+)\s+' +
    r'(?P<dest>\w+)\s+' +
    r'(?P<dist>\d+)\s*mm$')

def register_node(node):
    if node.name in nodes_by_name:
        print('error: duplicate node name', node.name, file=sys.stderr)
        sys.exit(1)
    nodes_by_name[node.name] = node

class Edge(object):
    def __init__(self, src, dest):
        global nodes
        self.src  = src
        self.dest = dest
        rev_src  = nodes[nodes[src].reverse]
        rev_dest = nodes[nodes[dest].reverse]
        if not isinstance(rev_dest, Branch):
            self.rev = 'TRACK_EDGE_AHEAD'
        elif rev_dest.straight == rev_src.node_id:
            self.rev = 'TRACK_EDGE_STRAIGHT'
        else:
            self.rev = 'TRACK_EDGE_CURVED'

class Node(object):
    def __init__(self, type, num, name, reverse):
        self.type    = type
        self.num     = num
        self.name    = name
        self.reverse = reverse
        register_node(self)

    def resolve_ids(self):
        self.reverse = nodes_by_name[self.reverse].node_id

    def make_edges(self):
        pass

    def render(self):
        template = '''    {{
        TRACK_NODE_{type},
        "{name}",
        {num},
        &{track_name}_nodes[{reverse}],
        {{
            {{
                &{track_name}_nodes[{src0}],
                &{track_name}_nodes[{dest0}],
                &{track_name}_nodes[{rdest0}].edge[{re0}],
                {dist0}
            }},
            {{
                &{track_name}_nodes[{src1}],
                &{track_name}_nodes[{dest1}],
                &{track_name}_nodes[{rdest1}].edge[{re1}],
                {dist1}
            }}
        }}
    }}'''
        return template.format(
            track_name = track_name,
            type = self.type,
            name = self.name,
            num  = self.num,
            reverse = self.reverse,
            node_id = self.node_id,
            src0    = self.edges[0].src,
            src1    = self.edges[1].src,
            dest0   = self.edges[0].dest,
            dest1   = self.edges[1].dest,
            rdest0  = nodes[self.edges[0].dest].reverse,
            rdest1  = nodes[self.edges[1].dest].reverse,
            re0     = self.edges[0].rev,
            re1     = self.edges[1].rev,
            dist0   = dists[(self.edges[0].src, self.edges[0].dest)],
            dist1   = dists[(self.edges[1].src, self.edges[1].dest)])

class Sensor(Node):
    def __init__(self, num, name, ahead, reverse):
        super(Sensor, self).__init__('SENSOR', num, name, reverse)
        self.ahead = ahead
        if num in sensors_by_num:
            print('error: duplicate sensor number', num, file=sys.stderr)
            sys.exit(1)
        sensors_by_num[num] = self

    def resolve_ids(self):
        super(Sensor, self).resolve_ids()
        self.ahead = nodes_by_name[self.ahead].node_id

    def make_edges(self):
        self.edges = [Edge(src=self.node_id, dest=self.ahead), Edge(0, 0)]

class Branch(Node):
    def __init__(self, num, name, straight, curved, reverse):
        super(Branch, self).__init__('BRANCH', num, name, reverse)
        self.straight = straight
        self.curved   = curved

    def resolve_ids(self):
        super(Branch, self).resolve_ids()
        self.straight = nodes_by_name[self.straight].node_id
        self.curved   = nodes_by_name[self.curved].node_id

    def make_edges(self):
        self.edges = [Edge(src=self.node_id, dest=self.straight),
                      Edge(src=self.node_id, dest=self.curved)]

class Merge(Node):
    def __init__(self, num, name, ahead, reverse):
        super(Merge, self).__init__('MERGE', num, name, reverse)
        self.ahead = ahead

    def resolve_ids(self):
        super(Merge, self).resolve_ids()
        self.ahead   = nodes_by_name[self.ahead].node_id

    def make_edges(self):
        self.edges = [Edge(src=self.node_id, dest=self.ahead), Edge(0, 0)]

class Enter(Node):
    def __init__(self, name, ahead, reverse):
        super(Enter, self).__init__('ENTER', 0, name, reverse)
        self.ahead   = ahead

    def resolve_ids(self):
        super(Enter, self).resolve_ids()
        self.ahead   = nodes_by_name[self.ahead].node_id

    def make_edges(self):
        self.edges = [Edge(src=self.node_id, dest=self.ahead), Edge(0, 0)]

class Exit(Node):
    def __init__(self, name, reverse):
        super(Exit, self).__init__('EXIT', 0, name, reverse)

    def make_edges(self):
        self.edges   = [Edge(0, 0), Edge(0, 0)]

class DummyNode(Node):
    def __init__(self):
        self.type    = 'NONE'
        self.num     = 0
        self.name    = ''
        self.reverse = 0
    def resolve_ids(self):
        pass
    def make_edges(self):
        self.edges = [Edge(0, 0), Edge(0, 0)]

# PARSE INPUT
for line in sys.stdin:
    line = line.strip()
    if line == '' or line.startswith('#'):
        continue

    md = sensor_re.match(line)
    if md:
        sensors.append(Sensor(
            num=int(md.group('num')),
            name=md.group('name'),
            ahead=md.group('ahead'),
            reverse=md.group('rev_name')))
        sensors.append(Sensor(
            num=int(md.group('rev_num')),
            name=md.group('rev_name'),
            ahead=md.group('rev_ahead'),
            reverse=md.group('name')))
        continue

    md = switch_re.match(line)
    if md:
        switches.append(Branch(
            num      = int(md.group('num')),
            name     = md.group('name'),
            straight = md.group('straight'),
            curved   = md.group('curved'),
            reverse  = md.group('merge_name')))
        switches.append(Merge(
            num      = int(md.group('num')),
            name     = md.group('merge_name'),
            ahead    = md.group('merge_ahead'),
            reverse  = md.group('name')))
        continue

    md = enter_re.match(line)
    if md:
        enters.append(Enter(
            name    = md.group('name'),
            ahead   = md.group('ahead'),
            reverse = md.group('exit_name')))
        enters.append(Exit(
            name    = md.group('exit_name'),
            reverse = md.group('name')))
        continue

    md = dist_re.match(line)
    if md:
        src, dest, dist = md.group('src', 'dest', 'dist')
        dist = int(dist)
        dists_by_name[(src, dest)] = dist
        rev_src  = nodes_by_name[src].reverse
        rev_dest = nodes_by_name[dest].reverse
        dists_by_name[(rev_dest, rev_src)] = dist
        continue

    print('error: bad line:', line, file=sys.stderr)
    sys.exit(1)

# ASSIGN NODES TO THEIR POSITIONS
nodes = []
max_sensor = max(sensors_by_num.keys())
for i in range(max_sensor + 1):
    if i in sensors_by_num:
        nodes.append(sensors_by_num[i])
    else:
        nodes.append(DummyNode())

nodes += switches
nodes += enters

for i, node in enumerate(nodes):
    node.node_id = i

for node in nodes:
    node.resolve_ids()

for node in nodes:
    node.make_edges()

dists[(0, 0)] = 0 # for dummy edges
for src, dest in dists_by_name.keys():
    src_id  = nodes_by_name[src].node_id
    dest_id = nodes_by_name[dest].node_id
    dists[(src_id, dest_id)] = dists_by_name[(src, dest)]

for src, dest in dists.keys():
    # check that it's used
    if nodes[src].edges[0].dest != dest and nodes[src].edges[1].dest != dest:
        print('error: distance for nonexistent edge from',
            nodes[src].name, 'to', nodes[dest].name,
            file=sys.stderr)
        sys.exit(1)

# PRINT IT OUT
print('''/* GENERATED FILE. DO NOT EDIT */
#include "xbool.h"
#include "xint.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"

static const struct track_node {track_name}_nodes[{n_nodes}];

const struct track_graph {track_name} = {{
    .nodes     = {track_name}_nodes,
    .n_nodes   = {n_nodes},
    .n_sensors = {n_sensors}
}};

static const struct track_node {track_name}_nodes[{n_nodes}] = {{'''.format(
    track_name = track_name,
    n_nodes = len(nodes),
    n_sensors = len(sensors)))

for i, node in enumerate(nodes):
    if i != 0:
        print(',')
    print(node.render(), end='')

print('\n};')
