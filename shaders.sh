#!/usr/bin/env bash
set -xe
python bin2c.py ./src/verts/vert2.glsl verts_vert2 > ./src/verts/vert2.c
python bin2c.py ./src/frags/frag2.glsl frags_frag2 > ./src/frags/frag2.c
