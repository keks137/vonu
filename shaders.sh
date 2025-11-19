#!/usr/bin/env bash
set -xe
python bin2c.py ./src/verts/vert1.glsl verts_vert1 > ./src/verts/vert1.c
python bin2c.py ./src/frags/frag1.glsl frags_frag1 > ./src/frags/frag1.c
