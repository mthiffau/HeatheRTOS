#!/usr/bin/env python
from __future__ import print_function
import sys
import re

if len(sys.argv) != 2:
    print('usage: {} NAME'.format(sys.argv[0]), file=sys.stderr)
    sys.exit(1)

track_name = sys.argv[1]
if not track_name.startswith('track_'):
    print("error: filename must begin with 'track_'", file=sys.stderr)
    sys.exit(1)

track_shortname = track_name[6:] # strip track_ from beginning

sensors        = []
sensors_by_num = { }
switches       = []
enters         = []
separators     = []
nodes_by_name  = { }
dists_by_name  = { }
dists          = { }
mutexes        = []
calib_cycle    = []
total_edge_count = 0

sensor_re = re.compile(
    r'^sensor\s+' +
    r'(?P<name>\w+)\s*:\s*(?P<num>\d+)\s*->\s*(?P<ahead>\w+)' +
    r'\s*@\s*'
    r'(?P<rev_name>\w+)\s*:\s*(?P<rev_num>\d+)\s*->\s*(?P<rev_ahead>\w+)' +
    r'$')

sep_re = re.compile(
    r'^sep\s+' +
    r'(?P<name>\w+)\s*->\s*(?P<ahead>\w+)' +
    r'\s*@\s*'
    r'(?P<rev_name>\w+)\s*->\s*(?P<rev_ahead>\w+)' +
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

mutex_re = re.compile(
    r'^mutex\s+' +
    r'(?P<edges>'+
    r'\w+\s+\w+(\s*:\s*\w+\s+\w+)*' +
    r')$')

calib_re = re.compile(r'^calib\s+(?P<name>\w+)$')

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
                {dist0},
                &{track_name}_edge_groups[{group_start0}],
                {group_size0}
            }},
            {{
                &{track_name}_nodes[{src1}],
                &{track_name}_nodes[{dest1}],
                &{track_name}_nodes[{rdest1}].edge[{re1}],
                {dist1},
                &{track_name}_edge_groups[{group_start1}],
                {group_size1}
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
            dist1   = dists[(self.edges[1].src, self.edges[1].dest)],
            group_start0 = self.edges[0].group_start,
            group_size0  = self.edges[0].group_size,
            group_start1 = self.edges[1].group_start,
            group_size1  = self.edges[1].group_size)

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
        global total_edge_count
        total_edge_count += 1
        self.edges = [Edge(src=self.node_id, dest=self.ahead), Edge(0, 0)]

class Separator(Node):
    def __init__(self, name, ahead, reverse):
        super(Separator, self).__init__('SEP', 0, name, reverse)
        self.ahead = ahead

    def resolve_ids(self):
        super(Separator, self).resolve_ids()
        self.ahead = nodes_by_name[self.ahead].node_id

    def make_edges(self):
        global total_edge_count
        total_edge_count += 1
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
        global total_edge_count
        total_edge_count += 2
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
        global total_edge_count
        total_edge_count += 1
        self.edges = [Edge(src=self.node_id, dest=self.ahead), Edge(0, 0)]

class Enter(Node):
    def __init__(self, name, ahead, reverse):
        super(Enter, self).__init__('ENTER', 0, name, reverse)
        self.ahead   = ahead

    def resolve_ids(self):
        super(Enter, self).resolve_ids()
        self.ahead   = nodes_by_name[self.ahead].node_id

    def make_edges(self):
        global total_edge_count
        total_edge_count += 1
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

    md = sep_re.match(line)
    if md:
        separators.append(Separator(
            name=md.group('name'),
            ahead=md.group('ahead'),
            reverse=md.group('rev_name')))
        separators.append(Separator(
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

    md = mutex_re.match(line)
    if md:
        mutexes.append([s.split() for s in md.group('edges').split(':')])
        continue

    md = calib_re.match(line)
    if md:
        calib_cycle.append(md.group('name'))
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
nodes += separators

for i, node in enumerate(nodes):
    node.node_id = i

for node in nodes:
    node.resolve_ids()

for node in nodes:
    node.make_edges()

for node in nodes:
    for edge in node.edges:
        if edge.src == 0 and edge.dest == 0:
            continue
        if edge.src != node.node_id:
            print('node {} (id={}) has edge leaving from node id={}'.format(
                node.name,
                node.node_id),
                file=sys.stderr)
            sys.exit(1)
        for rev_edge in nodes[nodes[edge.dest].reverse].edges:
            if rev_edge.dest == node.reverse:
                break
        else:
            print('node {} (id={}) has stranded edge (no reverse)'.format(
                node.name,
                node.node_id),
                file=sys.stderr)
            sys.exit(1)

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

def get_edge_from_names(src_name, dest_name):
    if src_name not in nodes_by_name:
        print('error: no node', src_name, file=sys.stderr)
        sys.exit(1)
    if dest_name not in nodes_by_name:
        print('error: no node', dest_name, file=sys.stderr)
        sys.exit(1)
    src  = nodes_by_name[src_name]
    dest = nodes_by_name[dest_name]
    return get_edge_from_nodes(src, dest)

def get_edge_from_nodeids(src_id, dest_id):
    return get_edge_from_nodes(nodes[src_id], nodes[dest_id])

def get_reverse_edge(edge):
    return get_edge_from_nodeids(
        nodes[edge.dest].reverse,
        nodes[edge.src].reverse)

def get_edge_from_nodes(src, dest):
    for edge in src.edges:
        if edge.dest == dest.node_id:
            return edge
    else:
        print('error: calibration cycle invalid: no edge from',
            calib_cycle[i - 1], 'to', calib_cycle[i],
            file=sys.stderr)
        sys.exit(1)

mutex_groups = []
mutex_group_start = 0

def add_mutex_group(mutex):
    global mutex_groups, mutex_group_start
    mutex_group = set()
    for mutex_edge_names in mutex:
        mutex_group.add(get_edge_from_names(*mutex_edge_names))

    mutex_group_extra = mutex_group
    while len(mutex_group_extra) > 0:
        mutex_group_extra = set()
        for mutex_edge in mutex_group:
            mutex_edge_reverse = get_reverse_edge(mutex_edge)
            if mutex_edge_reverse not in mutex_group:
                mutex_group_extra.add(mutex_edge_reverse)
            if isinstance(nodes[mutex_edge.src], Branch):
                for mutex_branch_edge in nodes[mutex_edge.src].edges:
                    if mutex_branch_edge not in mutex_group:
                        mutex_group_extra.add(mutex_branch_edge)
        mutex_group.update(mutex_group_extra)

    for mutex_edge in mutex_group:
        mutex_edge.group_start = mutex_group_start
        mutex_edge.group_size  = len(mutex_group)

    mutex_group_start += len(mutex_group)
    mutex_groups.append(mutex_group)

for mutex in mutexes:
    add_mutex_group(mutex)

for node in nodes:
    for edge in node.edges:
        if edge.src == 0 and edge.dest == 0:
            edge.group_start = 0
            edge.group_size  = 0
            continue # skip garbage edges
        if not hasattr(edge, 'group_start'):
            add_mutex_group([[nodes[edge.src].name, nodes[edge.dest].name]])

calib_edges = []
def add_calib_edge(calib_prev, calib_cur):
    global calib_edges
    calib_edges.append(get_edge_from_nodes(calib_prev, calib_cur))

for i in range(len(calib_cycle)):
    calib_cycle[i] = nodes_by_name[calib_cycle[i]]
    if i > 0:
        add_calib_edge(calib_cycle[i - 1], calib_cycle[i])

add_calib_edge(calib_cycle[-1], calib_cycle[0])

calib_sensors  = []
calib_dists    = []
calib_switches = {}
calib_dist     = 0
for node, edge in zip(calib_cycle, calib_edges):
    if isinstance(node, Sensor):
        calib_sensors.append(node)
        calib_dists.append(calib_dist)
        calib_dist = 0
    elif isinstance(node, Branch):
        calib_switches[node.node_id] = edge == node.edges[1]
    calib_dist += dists[(edge.src, edge.dest)]

calib_dists[0] += calib_dist

# PRINT IT OUT
print('''/* GENERATED FILE. DO NOT EDIT */
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"

static const struct track_node {track_name}_nodes[{n_nodes}];
static const struct track_edge *{track_name}_edge_groups[{n_edges}];
static const struct track_node *{track_name}_calib_sensors[{n_calib_sensors}];
static const int                {track_name}_calib_mm[{n_calib_sensors}];
static const struct track_node *{track_name}_calib_switches[{n_calib_switches}];
static const bool               {track_name}_calib_curved[{n_calib_switches}];

const struct track_graph {track_name} = {{
    .name      = "{track_shortname}",
    .nodes     = {track_name}_nodes,
    .n_nodes   = {n_nodes},
    .n_sensors = {n_sensors},
    .n_calib_sensors  = {n_calib_sensors},
    .calib_sensors    = {track_name}_calib_sensors,
    .calib_mm         = {track_name}_calib_mm,
    .n_calib_switches = {n_calib_switches},
    .calib_switches   = {track_name}_calib_switches,
    .calib_curved     = {track_name}_calib_curved
}};

static const struct track_node {track_name}_nodes[{n_nodes}] = {{'''.format(
    track_name = track_name,
    track_shortname = track_shortname,
    n_nodes = len(nodes),
    n_edges = total_edge_count,
    n_sensors = len(sensors),
    n_calib_sensors = len(calib_sensors),
    n_calib_switches = len(calib_switches)))

for i, node in enumerate(nodes):
    if i != 0:
        print(',')
    print(node.render(), end='')

print('\n};')

# Print edge references grouped into mutex groups
print('''static const struct track_edge*
{track_name}_edge_groups[{n_edges}] = {{'''.format(
    track_name = track_name,
    n_edges = total_edge_count))

for mutex_group in mutex_groups:
    for edge in mutex_group:
        which_edge = 'TRACK_EDGE_AHEAD'
        if isinstance(nodes[edge.src], Branch):
            if edge.dest is nodes[edge.src].straight:
                which_edge = 'TRACK_EDGE_STRAIGHT'
            else:
                which_edge = 'TRACK_EDGE_CURVED'
        print('    &{track_name}_nodes[{src_id}].edge[{which}],'.format(
            track_name = track_name,
            src_id     = edge.src,
            which      = which_edge))

print('\n};')

# Print calibration cycle sensors
print('''static const struct track_node*
{track_name}_calib_sensors[{n_calib_sensors}] = {{'''.format(
    track_name = track_name,
    n_calib_sensors = len(calib_sensors)))

for node in calib_sensors:
    print('    &{track_name}_nodes[{num}],'.format(
        track_name = track_name,
        num        = node.num))

print('};')

# Print calibration cycle sensor distances
print('''static const int
{track_name}_calib_mm[{n_calib_sensors}] = {{'''.format(
    track_name = track_name,
    n_calib_sensors = len(calib_sensors)))

for calib_dist in calib_dists:
    print('    {dist},'.format(dist=calib_dist))

print('};')


# Print calibration cycle switches
print('''static const struct track_node*
{track_name}_calib_switches[{n_calib_switches}] = {{'''.format(
    track_name = track_name,
    n_calib_switches = len(calib_switches)))

for switch_id in calib_switches:
    print('    &{track_name}_nodes[{num}],'.format(
        track_name = track_name,
        num        = switch_id))

print('};')

# Print calibration cycle switch orientations
print('''static const bool
{track_name}_calib_curved[{n_calib_switches}] = {{'''.format(
    track_name = track_name,
    n_calib_switches = len(calib_switches)))

for switch_id in calib_switches:
    curved = 'true' if calib_switches[switch_id] else 'false'
    print('    {curved},'.format(curved = curved))

print('};')


'''
static const struct track_node *{track_name}_calib_curved[{n_calib_switches}];
'''
