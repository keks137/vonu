// #version 300 es
// precision mediump float;
#version 330 core
out vec4 FragColor;
  
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec3 LightColor; 


uniform sampler2D texture1;
// uniform vec3 sunDirection = vec3(0.2, 1.0, 0.3);
// uniform vec3 lightPos;
// uniform vec3 lightColor;
// uniform float lightIntensity;




void main()
{
	vec4 tex = texture(texture1, TexCoord);
	// vec4 tex = vec4(1.0,1.0,1.0,1.0)
	vec3 objectColor = tex.rgb;

	// float faceLight = max(dot(normalize(Normal), normalize(sunDirection)), 0.0);
	// faceLight = floor(faceLight * 3.0) / 3.0;
	float faceLight = 1.0;
	if (Normal.y > 0.5) {
		faceLight = 1.0;        // Top face
	} else if (Normal.y < -0.5) {
		faceLight = 0.5;        // Bottom face
	} else {
		faceLight = 0.8;        // Side faces
	}



	// float brightness = LightLevel;
	// float totalLight = brightness * faceLight;
	float totalLight = faceLight;

	vec3 finalLight = LightColor * totalLight;

	finalLight = max(finalLight, vec3(0.2, 0.2, 0.2));
	FragColor = vec4(tex.rgb * finalLight, tex.a);
	// vec3 lightPos = vec3(0.0, 10.0, 0.0);
	// vec3 lightColor = vec3(1.0, 1.0, 0.0);
	// float lightRadius = 20.0;
	//
	// float distance = length(FragPos - lightPos);
	//
	// float strength = 1.0 - clamp(distance / lightRadius, 0.0, 1.0);
	// vec3 lightDir = normalize(lightPos - FragPos);
	// float diff = max(dot(Normal, lightDir), 0.0);
	//
	// vec3 ambient = 0.1 * objectColor;
	// vec3 diffuse = diff * lightColor * strength;
	//
	// vec3 result = (ambient + diffuse) * objectColor;
	// FragColor = vec4(result, tex.a);

}

