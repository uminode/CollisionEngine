#shader vertex
#version 460 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 aNormal;

layout (std140, binding = 0) uniform Matrices {
	mat4 u_MVP;
	mat4 model;
	mat3 normalModel;
};

//uniform mat4 u_MVP;
//uniform mat4 model;
out vec3 FragPos;
out vec3 Normal;
out vec3 TexCoords;

void main() {
	//lighting calculations
	gl_Position = u_MVP * vec4(position,1.0);
	FragPos = vec4(model * vec4(position,1.0)).xyz;
	//Normal = transpose(inverse(mat3(model))) * normalize(aNormal);
	Normal = normalModel * normalize(aNormal);
	
	//pass texcoords to fragment shader
	TexCoords = position;
};

#shader fragment
#version 460 core

layout (location = 0) out vec4 color;

layout (std140, binding = 1) uniform Light {
	//float stride;
	vec4 u_Color;
};
layout (std140, binding = 2) uniform Camera {
	vec3 lightPosition;
	vec3 u_vPos;
};
//uniform vec3 u_Light;
//uniform vec3 u_vPos;
//uniform vec4 u_Color;
uniform samplerCube TextureSampler;


layout (std140, binding = 3) uniform isTextured {
	uint isTexture;
};
in vec3 Normal;
in vec3 FragPos;

in vec3 TexCoords;

void main() {
	// Light emission properties
	//Ambient Light
	vec3 LightColor = vec3(1.0, 1.0, 1.0);
	vec3 ambient = vec3(0.2, 0.2, 0.16);

	//diffuse lighting
	vec3 norm = normalize(Normal);
	vec3 lightDir = normalize(lightPosition - FragPos);
	float diff = clamp(dot(norm, lightDir), 0.0, 1.0);
	vec3 diffuse = diff * LightColor;
	vec4 tex = texture(TextureSampler, TexCoords);

	//specular lighting
	float specularStrength = 0.5;
	vec3 viewDir = normalize(cameraPosition - FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16);
	vec3 specular = specularStrength * spec * LightColor;

	if (isTexture==1) {
		color = (vec4(ambient, 1.0f) + vec4(diffuse, 1.0) + vec4(specular, 1.0)) * u_Color;
	}
	else {
		color = ((vec4(ambient, 1.0f) + vec4(diffuse, 1.0)) * tex + vec4(specular,1.0)) * u_Color;
	}

	color.a = u_Color[3];
};