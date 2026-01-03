#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in uint aNorIndex;
layout (location = 3) in uint aLight;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

out vec3 LightColor; 
out float LightLevel;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const vec3[6] NORMALS = vec3[6](
	vec3( 0.0,  0.0, -1.0),  // FACE_BACK
	vec3( 0.0,  0.0,  1.0),  // FACE_FRONT
	vec3(-1.0,  0.0,  0.0),  // FACE_LEFT
	vec3( 1.0,  0.0,  0.0),  // FACE_RIGHT
	vec3( 0.0, -1.0,  0.0),  // FACE_BOTTOM
	vec3( 0.0,  1.0,  0.0)   // FACE_TOP
);


void main()
{
	gl_Position = projection * view * model * vec4(aPos, 1.0);
	TexCoord = aTexCoord;
	FragPos = vec3(model * vec4(aPos, 1.0));
	uint aFaceIndex= aNorIndex& 0x07u;
	vec3 aNormal = NORMALS[aFaceIndex];
	// Normal = mat3(transpose(inverse(model))) * aNormal;
	Normal = aNormal;
	uint r = (aLight) & 0xFu;
	uint g = (aLight >> 4) & 0xFu;
	uint b = (aLight >> 8) & 0xFu;

	LightLevel = (aLight & 0xFu) / 15.0;

	LightColor = vec3(float(r) / 16.0, float(g) / 16.0, float(b) / 16.0);
}
