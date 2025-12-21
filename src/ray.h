#ifndef INCLUDE_SRC_RAY_H_
#define INCLUDE_SRC_RAY_H_

#include "../vendor/cglm/cglm.h"

typedef struct {
	vec3 origin;
	vec3 direction;
	float max_distance;
} Ray;

#endif // INCLUDE_SRC_RAY_H_
