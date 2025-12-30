#!/usr/bin/env bash
set -xe
python bin2c.py ./src/shaders/vert2.glsl verts_vert2 > ./src/shaders/vert2.c
python bin2c.py ./src/shaders/frag2.glsl frags_frag2 > ./src/shaders/frag2.c
