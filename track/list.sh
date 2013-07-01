#!/bin/bash
set -o errexit
set -o nounset

N=0
MAX_NODES=0
TRACK_FILES=(*.in)
for TRACK_FILE in "${TRACK_FILES[@]}"; do
    N=$((N + 1))
    K="$(grep '^ *\(sensor\|switch\|enter\) ' "$TRACK_FILE" | wc -l)"
    K=$((2 * K)) # each such line specifies two nodes
    if [[ "$K" -gt "$MAX_NODES" ]]; then
        MAX_NODES="$K"
    fi
done

# all further output to list.h
exec >list.h

cat <<EOF
/* GENERATED FILE. DO NOT EDIT.
 * DO NOT INCLUDE DIRECTLY - use track_graph.h */
#ifdef TRACK_LIST_H
#error "double-included track/list.h"
#endif

#define TRACK_LIST_H

#define TRACK_COUNT         $N
#define TRACK_NODES_MAX     $MAX_NODES

EOF

for TRACK_FILE in "${TRACK_FILES[@]}"; do
    TRACK_FILE="${TRACK_FILE%.in}"
    echo "extern const struct track_graph $TRACK_FILE;"
done

echo
echo "static const struct track_graph * const all_tracks[TRACK_COUNT] = {";
for TRACK_FILE in "${TRACK_FILES[@]}"; do
    TRACK_FILE="${TRACK_FILE%.in}"
    echo "    &$TRACK_FILE,"
done
echo "};"
