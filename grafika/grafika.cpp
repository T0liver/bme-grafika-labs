#include "framework.h"

#define DEBUG true

const int windowWidth = 1200, windowHeight = 600;

struct Camera {
	vec3 wEye, wLookat, wVup;
	float fov, asp, fp, bp;
public:
	Camera() {
		asp = (float)windowWidth / windowHeight;
		fov = 45.0f * (float)M_PI / 180.0f;
		fp = 1; bp = 50;
	}

	mat4 V() {
		return lookAt(wEye, wLookat, wVup);
	}

	mat4 P() {
		return perspective(fov, asp, fp, bp);
	}

	void Spin(const float angle = M_PI_4) {
		vec3 d = wEye - wLookat;

		wEye = vec3(
			d.x * cos(angle) + d.z * sin(angle),
			d.y,
			-d.x * sin(angle) + d.z * cos(angle)
		) + wLookat;
	}
};

class Material {
public:
	vec3 kd, ks, ka;
	float shininess;

	Material() {}

	Material(vec3 _kd, vec3 _ks, vec3 _ka, float _shiniess) {
		kd = _kd;
		ks = _ks;
		ka = _ka;
		shininess = _shiniess;
	}
};

class Light {
public:
	vec3 La, Le;
	vec4 wLightPos;

	Light() {}
	Light(vec3 _La, vec3 _Le, vec4 _wLightPos) {
		La = _La;
		Le = _Le;
		wLightPos = _wLightPos;
	}
};

class CheckerTexture : public Texture {
public:
	CheckerTexture(const int _width, const int _height,
		const vec3 _color1 = vec3(0.0f, 0.1f, 0.3f), const vec3 _color2 = vec3(0.3f, 0.3f, 0.3f))
		: Texture(_width, _height)
	{
		std::vector<vec3> img;
		img.reserve(_width * _height);

		for (int y = 0; y < _height; ++y) {
			for (int x = 0; x < _width; ++x) {
				bool isEven = (x + y) % 2 == 0;
				img.push_back(isEven ? _color1 : _color2);
			}
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _width, _height, 0, GL_RGB, GL_FLOAT, &img[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
};

class AsphaltTexture : public Texture {
public:
	AsphaltTexture(const int _width, const int _height) : Texture(_width, _height)
	{
		std::vector<vec3> img(_width * _height);
		vec3 asphaltColor(0.2f, 0.2f, 0.2f);
		vec3 lineColor(1.0f, 1.0f, 0.0f);

		int lineWidth = _width / 20;
		int centerX = _width / 2;

		for (int y = 0; y < _height; ++y) {
			for (int x = 0; x < _width; ++x) {
				bool isLine = abs(x - centerX) < lineWidth / 2 && (y / 20) % 2 == 0;
				img[y * _width + x] = isLine ? lineColor : asphaltColor;
			}
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _width, _height, 0, GL_RGB, GL_FLOAT, &img[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
};

struct RenderState {
	mat4 MVP, M, Minv, V, P;
	Material* material;
	std::vector<Light> lights;
	Texture* texture;
	vec3 wEye;
};

class Shader : public GPUProgram {
public:
	virtual void Bind(RenderState state) = 0;

	void setUniformMaterial(const Material& material, const std::string& name) {
		setUniform(material.kd, name + ".kd");
		setUniform(material.ks, name + ".ks");
		setUniform(material.ka, name + ".ka");
		setUniform(material.shininess, name + ".shininess");
	}

	void setUniformLight(const Light& light, const std::string& name) {
		setUniform(light.La, name + ".La");
		setUniform(light.Le, name + ".Le");
		setUniform(light.wLightPos, name + ".wLightPos");
	}
};

class PhongShader : public Shader {
	const char* vertexSource = R"(
		#version 330
		precision highp float;

		struct Light {
			vec3 La, Le;
			vec4 wLightPos;
		};

		uniform mat4  MVP, M, Minv;
		uniform Light[8] lights;
		uniform int   nLights;
		uniform vec3  wEye;

		layout(location = 0) in vec3  vtxPos;
		layout(location = 1) in vec3  vtxNorm;
		layout(location = 2) in vec2  vtxUV;

		out vec3 wNormal;
		out vec3 wView;
		out vec3 wLight[8];
		out vec2 texcoord;
		out vec3 worldPos;

		void main() {
			gl_Position = MVP * vec4(vtxPos, 1);
			vec4 wPos = vec4(vtxPos, 1) * M;
			worldPos = wPos.xyz;
			for(int i = 0; i < nLights; i++) {
				wLight[i] = lights[i].wLightPos.xyz * wPos.w - wPos.xyz * lights[i].wLightPos.w;
			}
		    wView  = wEye * wPos.w - wPos.xyz;
		    wNormal = (Minv * vec4(vtxNorm, 0)).xyz;
		    texcoord = vtxUV;
		}
	)";

	const char* fragmentSource = R"(
		#version 330
		precision highp float;
		const int maxTriangles = 256;

		struct Light {
			vec3 La, Le;
			vec4 wLightPos;
		};

		struct Material {
			vec3 kd, ks, ka;
			float shininess;
		};

		uniform Material material;
		uniform Light[8] lights;
		uniform int nLights;
		uniform sampler2D diffuseTexture;

		uniform vec3 triangleP1[maxTriangles];
		uniform vec3 triangleP2[maxTriangles];
		uniform vec3 triangleP3[maxTriangles];
		uniform int numOfTriangles;
		uniform vec3 lightDir;
		
		uniform bool useTexture;

		in vec3 wNormal;
		in vec3 wView;
		in vec3 wLight[8];
		in vec3 worldPos;
		in vec2 texcoord;

        out vec4 fragmentColor;

		void main() {
			vec3 N = normalize(wNormal);
			vec3 V = normalize(wView);
			if (dot(N, V) < 0) N = -N;

			vec3 texColor = useTexture ? texture(diffuseTexture, texcoord).rgb : vec3(1.0);
			vec3 ka = material.ka * texColor;
			vec3 kd = material.kd * texColor;

			vec3 radiance = vec3(0, 0, 0);
			for(int i = 0; i < nLights; i++) {
				vec3 L = normalize(wLight[i]);
				vec3 H = normalize(L + V);

				bool inShadow = false;
				vec3 origin = worldPos + N * 0.01;
				for (int j = 0; j < numOfTriangles; j++) {

					const float eps = 1e-4;
					vec3 edge1 = triangleP2[j] - triangleP1[j];
					vec3 edge2 = triangleP3[j] - triangleP1[j];
					vec3 h = cross(-lightDir, edge2);
					float a = dot(edge1, h);
					if (abs(a) < eps) continue;
					float f = 1.0 / a;
					vec3 s = origin - triangleP1[j];
					float u = f * dot(s, h);
					if (u < 0.0 || u > 1.0) continue;
					vec3 q = cross(s, edge1);
					float v = f * dot(-lightDir, q);
					if (v < 0.0 || u + v > 1.0) continue;
					float t = f * dot(edge2, q);

					if (t > eps) {
						inShadow = true;
						break;
					}
				}

				if (inShadow) {
					float cost = max(dot(N, L), 0), cosd = max(dot(N, H), 0);
					radiance += ka * lights[i].La +
								(kd * cost + material.ks * pow(cosd, material.shininess)) * lights[i].La;
				} else {
					float cost = max(dot(N, L), 0), cosd = max(dot(N, H), 0);
					radiance += ka * lights[i].La +
								(kd * cost + material.ks * pow(cosd, material.shininess)) * lights[i].Le;
				}
			}
			fragmentColor = vec4(radiance, 1);
		}
	)";

public:
	PhongShader() { create(vertexSource, fragmentSource); }

	void Bind(RenderState state) {
		Use();
		setUniform(state.MVP, "MVP");
		setUniform(state.M, "M");
		setUniform(state.Minv, "Minv");
		setUniform(state.wEye, "wEye");

		setUniform(0, "diffuseTexture");
		bool useTexture = state.texture != nullptr;
		setUniform(useTexture, "useTexture");
		if (useTexture) {
			state.texture->Bind(0);
			// kell ez?
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		setUniformMaterial(*state.material, "material");

		setUniform((int)state.lights.size(), "nLights");
		for (unsigned int i = 0; i < state.lights.size(); i++) {
			setUniformLight(state.lights[i], std::string("lights[") + std::to_string(i) + std::string("]"));
		}
	}
};

struct VertexData {
	vec3 position, normal;
	vec2 texcoord;
};

class Object3d {
protected:
	GLuint vao = 0, vbo = 0;
	int vertexCount = 0;
	std::vector<VertexData> vertices;
public:
	Object3d() {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
	}

	const std::vector<VertexData>& getVertices() const { return vertices; }

	virtual ~Object3d() {
		if (vbo) glDeleteBuffers(1, &vbo);
		if (vao) glDeleteVertexArrays(1, &vao);
	}

	void uploadVertexData(const std::vector<VertexData>& vertices) {
		vertexCount = vertices.size();
		this->vertices = vertices;
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(VertexData), vertices.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(0); // position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, position));

		glEnableVertexAttribArray(1); // normal
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));

		glEnableVertexAttribArray(2); // texcoord
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, texcoord));
	}

	virtual void Draw() {
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, vertexCount);
	}
};

class Plane : public Object3d {
public:
	Plane(const vec3 _center, const vec2 _size, const vec3 _normal) {
		std::vector<VertexData> verticles;

		vec3 tangent = normalize(cross(_normal, vec3(0.0f, 0.0f, 1.0f)));
		if (length(tangent) < 1e-6f) tangent = normalize(cross(_normal, vec3(0.0f, 1.0f, 0.0f)));
		vec3 bitangent = normalize(cross(_normal, tangent));
		vec3 halfU = tangent * (_size.x / 2.0f);
		vec3 halfV = bitangent * (_size.y / 2.0f);

		vec3 P1 = _center - halfU - halfV;
		vec3 P2 = _center + halfU - halfV;
		vec3 P3 = _center + halfU + halfV;
		vec3 P4 = _center - halfU + halfV;

		verticles.push_back({ P1, _normal, vec2(0.0f, 0.0f) });
		verticles.push_back({ P2, _normal, vec2(1.0f, 0.0f) });
		verticles.push_back({ P3, _normal, vec2(1.0f, 1.0f) });
		verticles.push_back({ P1, _normal, vec2(0.0f, 0.0f) });
		verticles.push_back({ P3, _normal, vec2(1.0f, 1.0f) });
		verticles.push_back({ P4, _normal, vec2(0.0f, 1.0f) });

		uploadVertexData(verticles);
	}
};

class Cylinder : public Object3d {
public:
	Cylinder(const vec3 _base, const vec3 _axis, const float _radius, const float _height) {
		vec3 axis = normalize(_axis);
		vec3 top = _base + axis * _height;

		vec3 up = vec3(0.0f, 0.0f, 1.0f);
		if (abs(dot(axis, up)) > 0.999f)
			up = vec3(0.0f, 1.0f, 0.0f);
		vec3 tangent = normalize(cross(up, axis));
		vec3 bitangent = normalize(cross(axis, tangent));

		const int slices = 6;
		const float angleStep = 2.0f * M_PI / slices;

		for (int i = 0; i < slices; i++) {
			float a0 = i * angleStep;
			float a1 = (i + 1) % slices * angleStep;

			vec3 P1 = _base + _radius * (cos(a0) * tangent + sin(a0) * bitangent);
			vec3 P2 = _base + _radius * (cos(a1) * tangent + sin(a1) * bitangent);
			vec3 P3 = top + _radius * (cos(a0) * tangent + sin(a0) * bitangent);
			vec3 P4 = top + _radius * (cos(a1) * tangent + sin(a1) * bitangent);

			vec3 N1 = normalize(P1 - _base);
			vec3 N2 = normalize(P2 - _base);

			vertices.push_back({ P1, N1, vec2(0.0f, 0.0f) });
			vertices.push_back({ P2, N2, vec2(1.0f, 0.0f) });
			vertices.push_back({ P3, N1, vec2(0.0f, 1.0f) });

			vertices.push_back({ P3, N1, vec2(0.0f, 1.0f) });
			vertices.push_back({ P2, N2, vec2(1.0f, 0.0f) });
			vertices.push_back({ P4, N2, vec2(1.0f, 1.0f) });
		}

		for (int i = 0; i < slices; i++) {
			float a0 = i * angleStep;
			float a1 = (i + 1) % slices * angleStep;

			vec3 P1 = _base + _radius * (cos(a0) * tangent + sin(a0) * bitangent);
			vec3 P2 = _base + _radius * (cos(a1) * tangent + sin(a1) * bitangent);

			vec3 normal = -axis;
			vertices.push_back({ _base, normal, vec2(0.5f, 0.5f) });
			vertices.push_back({ P2, normal, vec2(0.5f + 0.5f * cos(a1), 0.5f + 0.5f * sin(a1)) });
			vertices.push_back({ P1, normal, vec2(0.5f + 0.5f * cos(a0), 0.5f + 0.5f * sin(a0)) });
		}

		for (int i = 0; i < slices; i++) {
			float a0 = i * angleStep;
			float a1 = (i + 1) % slices * angleStep;

			vec3 P1 = top + _radius * (cos(a0) * tangent + sin(a0) * bitangent);
			vec3 P2 = top + _radius * (cos(a1) * tangent + sin(a1) * bitangent);

			vec3 normal = axis;
			vertices.push_back({ top, normal, vec2(0.5f, 0.5f) });
			vertices.push_back({ P1, normal, vec2(0.5f + 0.5f * cos(a0), 0.5f + 0.5f * sin(a0)) });
			vertices.push_back({ P2, normal, vec2(0.5f + 0.5f * cos(a1), 0.5f + 0.5f * sin(a1)) });
		}

		uploadVertexData(vertices);
	}
};

class Cone : public Object3d {
public:
	Cone(const vec3 _apex, const vec3 _axisDir, const float _height, const float _angle) {
		std::vector<VertexData> verticles;

		vec3 axisDir = normalize(_axisDir);
		vec3 baseCenter = _apex + axisDir * _height;
		float radius = tan(_angle) * _height;

		vec3 U = normalize(cross(axisDir, vec3(1.0f, 0.0f, 0.0f)));
		if (length(U) < 1e-6f) U = normalize(cross(axisDir, vec3(0.0f, 1.0f, 0.0f)));
		vec3 V = normalize(cross(axisDir, U));

		const int slices = 12;
		for (int i = 0; i < slices; ++i) {
			float theta1 = (float)(i + 0.5f) / slices * 2 * M_PI;
			float theta2 = (float)(i + 1.5f) / slices * 2 * M_PI;

			vec3 p1 = baseCenter + radius * (cos(theta1) * U + sin(theta1) * V);
			vec3 p2 = baseCenter + radius * (cos(theta2) * U + sin(theta2) * V);

			vec3 normal1 = normalize(cross(cross(p1 - _apex, axisDir), p1 - _apex));
			vec3 normal2 = normalize(cross(cross(p2 - _apex, axisDir), p2 - _apex));

			verticles.push_back({ _apex, normalize(cross(p1 - _apex, p2 - _apex)), vec2(0.5f, 1.0f) });
			verticles.push_back({ p1, normal1, vec2((float)i / slices, 0.0f) });
			verticles.push_back({ p2, normal2, vec2((float)(i + 1) / slices, 0.0f) });

			vec3 baseNormal = -axisDir;
			verticles.push_back({ baseCenter, baseNormal, vec2(0.5f, 0.5f) });
			verticles.push_back({ p2, baseNormal, vec2(0.5f + 0.5f * cos(theta2), 0.5f + 0.5f * sin(theta2)) });
			verticles.push_back({ p1, baseNormal, vec2(0.5f + 0.5f * cos(theta1), 0.5f + 0.5f * sin(theta1)) });
		}

		uploadVertexData(verticles);
	}
};

class Frustum : public Object3d {
public:
	Frustum(const vec3 _base, const vec3 _axisDir, const float _height, const float _radiusBottom, const float _radiusTop) {
		vec3 axis = normalize(_axisDir);
		vec3 top = _base + axis * _height;

		vec3 up = vec3(0.0f, 0.0f, 1.0f);
		if (abs(dot(axis, up)) > 0.999f)
			up = vec3(0.0f, 1.0f, 0.0f);
		vec3 tangent = normalize(cross(up, axis));
		vec3 bitangent = normalize(cross(axis, tangent));

		const int slices = 6;

		const float angleStep = 2.0f * M_PI / slices;

		for (int i = 0; i < slices; ++i) {
			float a0 = i * angleStep;
			float a1 = (i + 1) % slices * angleStep;

			vec3 dir0 = cos(a0) * tangent + sin(a0) * bitangent;
			vec3 dir1 = cos(a1) * tangent + sin(a1) * bitangent;

			vec3 p0b = _base + _radiusBottom * dir0;
			vec3 p1b = _base + _radiusBottom * dir1;
			vec3 p0t = top + _radiusTop * dir0;
			vec3 p1t = top + _radiusTop * dir1;

			vec3 n0 = normalize(dir0 * (_radiusBottom - _radiusTop) + axis * _height);
			vec3 n1 = normalize(dir1 * (_radiusBottom - _radiusTop) + axis * _height);

			vertices.push_back({ p0b, n0, vec2((float)i / slices, 0.0f) });
			vertices.push_back({ p1b, n1, vec2((float)(i + 1) / slices, 0.0f) });
			vertices.push_back({ p0t, n0, vec2((float)i / slices, 1.0f) });

			vertices.push_back({ p0t, n0, vec2((float)i / slices, 1.0f) });
			vertices.push_back({ p1b, n1, vec2((float)(i + 1) / slices, 0.0f) });
			vertices.push_back({ p1t, n1, vec2((float)(i + 1) / slices, 1.0f) });
		}

		for (int i = 0; i < slices; ++i) {
			float a0 = i * angleStep;
			float a1 = (i + 1) % slices * angleStep;

			vec3 p0 = _base + _radiusBottom * (cos(a0) * tangent + sin(a0) * bitangent);
			vec3 p1 = _base + _radiusBottom * (cos(a1) * tangent + sin(a1) * bitangent);

			vec3 normal = -axis;
			vertices.push_back({ _base, normal, vec2(0.5f, 0.5f) });
			vertices.push_back({ p1, normal, vec2(0.5f + 0.5f * cos(a1), 0.5f + 0.5f * sin(a1)) });
			vertices.push_back({ p0, normal, vec2(0.5f + 0.5f * cos(a0), 0.5f + 0.5f * sin(a0)) });
		}

		for (int i = 0; i < slices; ++i) {
			float a0 = i * angleStep;
			float a1 = (i + 1) % slices * angleStep;

			vec3 p0 = top + _radiusTop * (cos(a0) * tangent + sin(a0) * bitangent);
			vec3 p1 = top + _radiusTop * (cos(a1) * tangent + sin(a1) * bitangent);

			vec3 normal = axis;
			vertices.push_back({ top, normal, vec2(0.5f, 0.5f) });
			vertices.push_back({ p0, normal, vec2(0.5f + 0.5f * cos(a0), 0.5f + 0.5f * sin(a0)) });
			vertices.push_back({ p1, normal, vec2(0.5f + 0.5f * cos(a1), 0.5f + 0.5f * sin(a1)) });
		}

		uploadVertexData(vertices);
	}
};

class Sphere : public Object3d {
public:
	Sphere(const vec3 _center, const float _radius, const int latitudeSegments = 6, const int longitudeSegments = 6) {
		std::vector<std::vector<VertexData>> vertexGrid;
for (int lat = 0; lat <= latitudeSegments; ++lat) {
			float theta = (float)lat / latitudeSegments * M_PI;
			float sinTheta = sin(theta);
			float cosTheta = cos(theta);

			std::vector<VertexData> row;
			for (int lon = 0; lon <= longitudeSegments; ++lon) {
				float phi = (float)lon / longitudeSegments * 2.0f * M_PI;
				float sinPhi = sin(phi);
				float cosPhi = cos(phi);

				vec3 normal = vec3(cosPhi * sinTheta, sinPhi * sinTheta, cosTheta);
				vec3 position = _center + _radius * normal;
				vec2 texcoord = vec2((float)lon / longitudeSegments, 1.0f - (float)lat / latitudeSegments);

				row.push_back({ position, normal, texcoord });
			}
			vertexGrid.push_back(row);
		}

		for (int lat = 0; lat < latitudeSegments; ++lat) {
			for (int lon = 0; lon < longitudeSegments; ++lon) {
				const VertexData& v00 = vertexGrid[lat][lon];
				const VertexData& v01 = vertexGrid[lat][lon + 1];
				const VertexData& v10 = vertexGrid[lat + 1][lon];
				const VertexData& v11 = vertexGrid[lat + 1][lon + 1];

				vertices.push_back(v00);
				vertices.push_back(v10);
				vertices.push_back(v01);

				vertices.push_back(v01);
				vertices.push_back(v10);
				vertices.push_back(v11);
			}
		}

		uploadVertexData(vertices);
	}
};


struct Object {
	Shader* shader;
	Material* material;
	Texture* texture;
	Object3d* geoObj;
	vec3 scaleing, translation, rotationAxis;
	float rotationAngle;
public:
	Object(Shader* _shader, Material* _material, Object3d* _obj, Texture* _texture = nullptr) :
		scaleing(vec3(1.0f, 1.0f, 1.0f)), translation(vec3(0.0f, 0.0f, 0.0f)), rotationAxis(0.0f, 0.0f, 1.0f), rotationAngle(0.0f) {
		shader = _shader;
		texture = _texture;
		material = _material;
		geoObj = _obj;
	}

	virtual void SetModelingTransform(mat4& M, mat4& Minv) {
		M = translate(translation) * rotate(rotationAngle, rotationAxis) * scale(scaleing);
		Minv = scale(vec3(1.0f / scaleing.x, 1.0f / scaleing.y, 1.0f / scaleing.z)) * rotate(-rotationAngle, rotationAxis) * translate(-translation);
	}

	vec3 transformPoint(vec3 _point) const {
		mat4 M, Minv;
		const_cast<Object*>(this)->SetModelingTransform(M, Minv);
		vec4 wCords = M * vec4(_point.x, _point.y, _point.z, 1.0f);
		return vec3(wCords.x, wCords.y, wCords.z);
	}

	void Draw(RenderState state) {
		if (geoObj && shader) {
			mat4 M, Minv;
			SetModelingTransform(M, Minv);
			state.M = M;
			state.Minv = Minv;
			state.MVP = state.P * state.V * state.M;
			state.material = material;
			state.texture = texture;
			shader->Bind(state);
			geoObj->Draw();
		}
	}
};

float sign(const vec3 p1, const vec3 p2, const vec3 p3) {
	return (p1.x - p3.x) * (p2.z - p3.z) - (p2.x - p3.x) * (p1.z - p3.z);
}

bool isPointInTriangle(const vec3& p, const vec3& t1, const vec3& t2, const vec3& t3) {
	float d1, d2, d3;
	bool has_neg, has_pos;

	d1 = sign(p, t1, t2);
	d2 = sign(p, t2, t3);
	d3 = sign(p, t3, t1);

	has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

	if (!(has_neg && has_pos)) return true;

	float distToEdge = min(min(
		fabs(d1) / length(vec2(t2.z - t1.z, t1.x - t2.x)),
		fabs(d2) / length(vec2(t3.z - t2.z, t2.x - t3.x))),
		fabs(d3) / length(vec2(t1.z - t3.z, t3.x - t1.x)));
	return distToEdge <= 2.0f;
}

const vec3 defCamBase = vec3(-10.0f, 10.0f, 0.0f);

class Scene {
	std::vector<Object*> objects;
	std::vector<vec3> trisP1, trisP2, trisP3;
	std::vector<Light> lights;
	Camera camera;

	vec3 carBase = vec3(0.0f, 0.0f, 0.0f);
	vec3 camBase = defCamBase;
	vec3 carTarget = carBase;
	vec3 carAxis = vec3(0.0f, 0.0f, -1.0f);
	Object* carObj;
	std::vector<Object*> roadObjects;
	std::vector<Object*> carObjects;
	float speed = 0.0f;
public:
	bool Start = false;
	bool Out = false;

	void Build() {
		if (DEBUG) printf("Creating scene...\n");
		// Shaders
		Shader* phongShader = new PhongShader();

		// Materials
		Material* boardMaterial = new Material(vec3(1.0f, 1.0f, 1.0f), vec3(0.0f, 0.0f, 0.0f), vec3(2.0f), 100.0f);
		Material* yellowPlastic = new Material(vec3(0.3f, 0.2f, 0.1f), vec3(2.0f, 2.0f, 2.0f), vec3(0.9f, 0.6f, 0.3f), 50.0f);
		Material* roadMaterial = new Material(vec3(0.2f, 0.2f, 0.2f), vec3(0.0f, 0.0f, 0.0f), vec3(1.0f), 5.0f);

		// Textures
		Texture* boardTexture = new CheckerTexture(3, 3, vec3(0.0f, 0.0f, 0.0f), vec3(0.9f, 0.9f, 0.9f));
		Texture* asphaltTexture = new AsphaltTexture(3, 3);

		// Create objects by setting up their vertex data on the GPU
		Object3d* checkerPlane = new Plane(vec3(0.0f, -0.75f, 0.0f), vec2(7.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* board = new Object(phongShader, boardMaterial, checkerPlane, boardTexture);
		objects.push_back(board);
		roadObjects.push_back(board);

		Object3d* roadPlane1 = new Plane(vec3(50.0f, -1.0f, 0.0f), vec2(100.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road1 = new Object(phongShader, roadMaterial, roadPlane1, asphaltTexture);
		objects.push_back(road1);
		roadObjects.push_back(road1);

		Object3d* roadPlane2 = new Plane(vec3(100.0f, -1.0f, 5.0f), vec2(20.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road2 = new Object(phongShader, roadMaterial, roadPlane2, asphaltTexture);
		objects.push_back(road2);
		roadObjects.push_back(road2);

		Object3d* roadPlane3 = new Plane(vec3(110.0f, -1.0f, 21.5f), vec2(7.0f, 40.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road3 = new Object(phongShader, roadMaterial, roadPlane3, asphaltTexture);
		objects.push_back(road3);
		roadObjects.push_back(road3);

		Object3d* roadPlane4 = new Plane(vec3(105.0f, -1.0f, 50.5f), vec2(7.0f, 40.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road4 = new Object(phongShader, roadMaterial, roadPlane4, asphaltTexture);
		objects.push_back(road4);
		roadObjects.push_back(road4);

		Object3d* roadPlane5 = new Plane(vec3(88.5f, -1.0f, 70.0f), vec2(40.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road5 = new Object(phongShader, roadMaterial, roadPlane5, asphaltTexture);
		objects.push_back(road5);
		roadObjects.push_back(road5);

		Object3d* roadPlane6 = new Plane(vec3(65.0f, -1.0f, 65.0f), vec2(20.0f, 10.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road6 = new Object(phongShader, roadMaterial, roadPlane6, asphaltTexture);
		objects.push_back(road6);
		roadObjects.push_back(road6);

		Object3d* roadPlane7 = new Plane(vec3(55.0f, -1.0f, 60.0f), vec2(20.0f, 10.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road7 = new Object(phongShader, roadMaterial, roadPlane7, asphaltTexture);
		objects.push_back(road7);
		roadObjects.push_back(road7);

		Object3d* roadPlane8 = new Plane(vec3(35.0f, -1.0f, 58.5f), vec2(40.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road8 = new Object(phongShader, roadMaterial, roadPlane8, asphaltTexture);
		objects.push_back(road8);
		roadObjects.push_back(road8);

		Object3d* roadPlane9 = new Plane(vec3(5.0f, -1.0f, 65.5f), vec2(40.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road9 = new Object(phongShader, roadMaterial, roadPlane9, asphaltTexture);
		objects.push_back(road9);
		roadObjects.push_back(road9);

		Object3d* roadPlane10 = new Plane(vec3(-20.0f, -1.0f, 72.5f), vec2(30.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road10 = new Object(phongShader, roadMaterial, roadPlane10, asphaltTexture);
		objects.push_back(road10);
		roadObjects.push_back(road10);

		Object3d* roadPlane11 = new Plane(vec3(-20.0f, -1.0f, 72.5f), vec2(30.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road11 = new Object(phongShader, roadMaterial, roadPlane11, asphaltTexture);
		objects.push_back(road11);
		roadObjects.push_back(road11);

		Object3d* roadPlane12 = new Plane(vec3(-33.0f, -1.0f, 65.0f), vec2(7.0f, 20.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road12 = new Object(phongShader, roadMaterial, roadPlane12, asphaltTexture);
		objects.push_back(road12);
		roadObjects.push_back(road12);

		Object3d* roadPlane13 = new Plane(vec3(-60.0f, -1.0f, 55.0f), vec2(60.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road13 = new Object(phongShader, roadMaterial, roadPlane13, asphaltTexture);
		objects.push_back(road13);
		roadObjects.push_back(road13);

		Object3d* roadPlane14 = new Plane(vec3(-97.0f, -1.0f, 50.0f), vec2(30.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road14 = new Object(phongShader, roadMaterial, roadPlane14, asphaltTexture);
		objects.push_back(road14);
		roadObjects.push_back(road14);

		Object3d* roadPlane15 = new Plane(vec3(-110.0f, -1.0f, 55.0f), vec2(20.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road15 = new Object(phongShader, roadMaterial, roadPlane15, asphaltTexture);
		objects.push_back(road15);
		roadObjects.push_back(road15);

		Object3d* roadPlane16 = new Plane(vec3(-120.0f, -1.0f, 60.0f), vec2(20.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road16 = new Object(phongShader, roadMaterial, roadPlane16, asphaltTexture);
		objects.push_back(road16);
		roadObjects.push_back(road16);

		Object3d* roadPlane17 = new Plane(vec3(-130.0f, -1.0f, 40.0f), vec2(7.0f, 40.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road17 = new Object(phongShader, roadMaterial, roadPlane17, asphaltTexture);
		objects.push_back(road17);
		roadObjects.push_back(road17);

		Object3d* roadPlane18 = new Plane(vec3(-135.0f, -1.0f, 10.0f), vec2(7.0f, 40.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road18 = new Object(phongShader, roadMaterial, roadPlane18, asphaltTexture);
		objects.push_back(road18);
		roadObjects.push_back(road18);

		Object3d* roadPlane19 = new Plane(vec3(-125.0f, -1.0f, -7.0f), vec2(20.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road19 = new Object(phongShader, roadMaterial, roadPlane19, asphaltTexture);
		objects.push_back(road19);
		roadObjects.push_back(road19);

		Object3d* roadPlane20 = new Plane(vec3(-62.0f, -1.0f, 0.0f), vec2(125.0f, 7.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* road20 = new Object(phongShader, roadMaterial, roadPlane20, asphaltTexture);
		objects.push_back(road20);
		roadObjects.push_back(road20);

		Material* blackRubber = new Material(vec3(0.02f), vec3(0.5f), vec3(0.1f), 10.0f);
		Material* carBodyMat = new Material(vec3(0.1f, 0.0f, 0.0f), vec3(1.0f), vec3(0.8f, 0.0f, 0.0f), 100.0f);
		Material* carPitMat = new Material(vec3(0.4f, 0.4f, 1.2f), vec3(0.8f), vec3(0.5f, 0.5f, 0.9f), 120.0f);

		Object3d* carBodyObj = new Cylinder(carBase, carAxis, 0.5f, 2.0f);
		carObj = new Object(phongShader, carBodyMat, carBodyObj);
		objects.push_back(carObj);
		carObjects.push_back(carObj);

		Object3d* carBodyObj2 = new Frustum(carBase + vec3(0.0f, 0.0f, 1.0f), carAxis, 1.0f, 0.2f, 0.5f);
		Object* carBody2 = new Object(phongShader, carBodyMat, carBodyObj2);
		objects.push_back(carBody2);
		carObjects.push_back(carBody2);

		Object3d* carBodyObj3 = new Cylinder(carBase + vec3(0.0f, 0.4f, -0.3f), carAxis, 0.4f, 1.5f);
		Object* carBody3 = new Object(phongShader, carPitMat, carBodyObj3);
		objects.push_back(carBody3);
		carObjects.push_back(carBody3);

		Object3d* wheel1Obj = new Cylinder(carBase + vec3(0.65f, -0.4f, -1.5f), carAxis + vec3(-1.0f, 0.0f, 1.0f), 0.4f, 0.3f);
		Object* wheel1 = new Object(phongShader, blackRubber, wheel1Obj);
		objects.push_back(wheel1);
		carObjects.push_back(wheel1);

		Object3d* wheel2Obj = new Cylinder(carBase + vec3(0.65f, -0.4f, -0.5f), carAxis + vec3(-1.0f, 0.0f, 1.0f), 0.4f, 0.3f);
		Object* wheel2 = new Object(phongShader, blackRubber, wheel2Obj);
		objects.push_back(wheel2);
		carObjects.push_back(wheel2);

		Object3d* wheel3Obj = new Cylinder(carBase + vec3(-0.35f, -0.4f, -1.5f), carAxis + vec3(-1.0f, 0.0f, 1.0f), 0.4f, 0.3f);
		Object* wheel3 = new Object(phongShader, blackRubber, wheel3Obj);
		objects.push_back(wheel3);
		carObjects.push_back(wheel3);

		Object3d* wheel4Obj = new Cylinder(carBase + vec3(-0.35f, -0.4f, -0.5f), carAxis + vec3(-1.0f, 0.0f, 1.0f), 0.4f, 0.3f);
		Object* wheel4 = new Object(phongShader, blackRubber, wheel4Obj);
		objects.push_back(wheel4);
		carObjects.push_back(wheel4);

		for (Object* obj : carObjects) {
			obj->translation = carBase;
			obj->rotationAxis = vec3(0.0f, 1.0f, 0.0f);
			obj->rotationAngle = M_PI_2;
		}

		// Camera
		camera.wEye = camBase;
		camera.wLookat = carBase;
		camera.wVup = vec3(0.0f, 1.0f, 0.0f);

		// Lights
		lights.resize(2);
		lights[0].wLightPos = vec4(1.0f, 1.0f, 1.0f, 0.0f);
		lights[0].La = vec3(0.2f, 0.2f, 0.2f);
		lights[0].Le = vec3(1.0f, 1.0f, 1.0f);
		lights[1].wLightPos = vec4(camBase.x, camBase.y, camBase.z, 1.0f);
		lights[1].La = vec3(0.2f, 0.2f, 0.2f);
		lights[1].Le = vec3(1.0f, 1.0f, 1.0f);

		if (DEBUG) printf("All set up!\n");

		// Upload the objects (and triangles) to the GPU
		UploadToGPU();
	}

	void Render() {
		RenderState state;
		state.wEye = camera.wEye;
		state.V = camera.V();
		state.P = camera.P();
		state.lights = lights;
		for (Object* obj : objects) obj->Draw(state);

		if (Out) return;

		vec3 carPos = getCarPosition();
		vec3 dir = carTarget - carPos;

		if (length(dir) > 1e-4) {
			if (length(dir) > speed)
				dir = normalize(dir) * speed;
			carTarget += dir;
			MoveCar(dir);
		}

	}

	void UploadToGPU() {
		// Upload the objects (and triangles) to the GPU
		trisP1.clear();
		trisP2.clear();
		trisP3.clear();

		for (Object* obj : objects) {
			Object3d* mesh = obj->geoObj;
			if (!mesh)
				continue;
			const std::vector<VertexData>& verts = mesh->getVertices();
			for (size_t i = 0; i + 2 < verts.size(); i += 3) {
				trisP1.push_back(obj->transformPoint(verts[i + 0].position));
				trisP2.push_back(obj->transformPoint(verts[i + 1].position));
				trisP3.push_back(obj->transformPoint(verts[i + 2].position));
			}
		}

		std::vector<float> trisP1flat, trisP2flat, trisP3flat;
		for (size_t i = 0; i < trisP1.size(); ++i) {
			trisP1flat.push_back(trisP1[i].x);
			trisP1flat.push_back(trisP1[i].y);
			trisP1flat.push_back(trisP1[i].z);

			trisP2flat.push_back(trisP2[i].x);
			trisP2flat.push_back(trisP2[i].y);
			trisP2flat.push_back(trisP2[i].z);

			trisP3flat.push_back(trisP3[i].x);
			trisP3flat.push_back(trisP3[i].y);
			trisP3flat.push_back(trisP3[i].z);
		}

		// Upload the triangles to the GPU
		int currentProgram;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
		int loc0 = glGetUniformLocation(currentProgram, "triangleP1");
		int loc1 = glGetUniformLocation(currentProgram, "triangleP2");
		int loc2 = glGetUniformLocation(currentProgram, "triangleP3");
		int locN = glGetUniformLocation(currentProgram, "numOfTriangles");
		vec3 dir = normalize(vec3(-lights[0].wLightPos.x, -lights[0].wLightPos.y, -lights[0].wLightPos.z));
		glUniform3fv(glGetUniformLocation(currentProgram, "lightDir"), 1, &dir.x);
		glUniform1i(locN, (GLint)trisP1.size());
		glUniform3fv(loc0, trisP1.size(), trisP1flat.data());
		glUniform3fv(loc1, trisP2.size(), trisP2flat.data());
		glUniform3fv(loc2, trisP3.size(), trisP3flat.data());
	}

	void Spin(const float angle = M_PI_4) {
		camera.Spin(angle);
	}

	void MoveCar(const vec3& dir) {
		if (!carObj) return;

		if (!Start || speed == 0.0f) return;

		if (!isCarOnRoad()) {
			Out = true;
			if (DEBUG) printf("Car is out of the road:\t%f, %f, %f\n", carBase.x, carBase.y, carBase.z);
			return;
		}

		if (length(dir) > 1e-4) {
			vec3 newDir = normalize(dir);
			carAxis = normalize(mix(carAxis, newDir, 0.2f));
		}

		vec3 forward = vec3(0.0f, 0.0f, 1.0f);
		float angle = acos(clamp(dot(forward, carAxis), -1.0f, 1.0f));
		vec3 axis = cross(forward, carAxis);
		if (length(axis) < 1e-4) axis = vec3(0.0f, 1.0f, 0.0f);

		carBase = carBase + dir;
		carBase *= vec3(1.0f, 0.0f, 1.0f);

		for (Object* obj : carObjects) {
			obj->translation = carBase;
			obj->rotationAxis = axis;
			obj->rotationAngle = angle;
		}

		float camAngle = atan2(carAxis.x, carAxis.z);
		vec3 rotatedCamBase = vec3(
			camBase.x * sin(camAngle) + camBase.z * cos(camAngle),
			camBase.y,
			camBase.x * cos(camAngle) - camBase.z * sin(camAngle)
		);

		vec3 desiredCamEye = carBase + rotatedCamBase;
		camera.wEye = mix(camera.wEye, desiredCamEye, 0.1f);
		camera.wLookat = mix(camera.wLookat, carBase, 0.1f);

		lights[1].wLightPos = vec4(camBase.x, camBase.y, camBase.z, 1.0f);

		UploadToGPU();
	}

	mat4 getCameraViewMatrix() { return camera.V(); }
	mat4 getCameraProjMatrix() { return camera.P(); }
	vec3 getCarPosition() { return carBase; }

	void setTarget(const vec3& _target) {
		carTarget = _target;
	}

	vec3 getCarAxis() { return carAxis; }
	vec3 getCarBase() { return carBase; }

	bool isCarOnRoad() {
		for (Object* roadObj : roadObjects) {
			const std::vector<VertexData>& verts = roadObj->geoObj->getVertices();
			for (int i = 0; i + 2 < verts.size(); i += 3) {
				vec3 t1 = roadObj->transformPoint(verts[i].position);
				vec3 t2 = roadObj->transformPoint(verts[i + 1].position);
				vec3 t3 = roadObj->transformPoint(verts[i + 2].position);
				if (isPointInTriangle(carBase, t1, t2, t3)) {
					return true;
				}
			}
		}
		return false;
	}

	void ResetCar() {

		carBase = vec3(0.0f, 0.0f, 0.0f);
		carTarget = carBase;
		carAxis = vec3(0.0f, 0.0f, -1.0f);

		for (Object* obj : carObjects) {
			obj->translation = carBase;
			obj->rotationAxis = vec3(0.0f, 1.0f, 0.0f);
			obj->rotationAngle = M_PI_2;
		}
		speed = 0.0f;

		camBase = defCamBase;
		camera.wEye = camBase;
		camera.wLookat = carBase;
		lights[1].wLightPos = vec4(camBase.x, camBase.y, camBase.z, 1.0f);

		Out = false;

		if (DEBUG) printf("Car is resetted!\n");
	}

	void speedUp() {
		if (speed < 0.5f) speed += 0.01f;
		if (DEBUG) printf("Speed up! Actual speed: %f\n", speed);
	}

	void slowDown() {
		if (speed > 0.0f) speed -= 0.01f;
		if (DEBUG) printf("Slow donw! Actual speed: %f\n", speed);
	}
};

class AutodromoDeMaputo : public glApp {
	Scene scene;
public:
	AutodromoDeMaputo() : glApp(3, 3, windowWidth, windowHeight, "Mazambique, Autodromo Internacional de Maputo") {}

	void onInitialization() {
		glViewport(0, 0, windowWidth, windowHeight);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		scene.Build();
	}

	void onDisplay() {
		glClearColor(0.0f, 0.65f, 0.098f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		scene.Render();
	}

	void onKeyboard(int key) override {
		if (DEBUG) printf("Key pressed: %c\n", key);
		if (key == 'p') {
			scene.Start = !scene.Start;
			refreshScreen();
		}
		else if (key == 'r') {
			scene.ResetCar();
			refreshScreen();
		}
		else if (key == 'w')
		{
			scene.speedUp();
		}
		else if (key == 's') {
			scene.slowDown();
		}
	}

	void onMouseMotion(int pX, int pY) override {
		
		float ndcX = 2.0f * pX / windowWidth - 1.0f;
		float ndcY = 1.0f - 2.0f * pY / windowHeight;

		mat4 view = scene.getCameraViewMatrix();
		mat4 proj = scene.getCameraProjMatrix();
		mat4 invVP = inverse(proj * view);

		vec4 rayStartNDC(ndcX, ndcY, -1.0f, 1.0f);
		vec4 rayEndNDC(ndcX, ndcY, 1.0f, 1.0f);
		vec4 rayStartWorld = invVP * rayStartNDC;
		vec4 rayEndWorld = invVP * rayEndNDC;
		rayStartWorld /= rayStartWorld.w;
		rayEndWorld /= rayEndWorld.w;

		vec3 rayDir = normalize(vec3(rayEndWorld - rayStartWorld));
		vec3 rayOrigin = vec3(rayStartWorld);

		float t = (-0.75f - rayOrigin.y) / rayDir.y;
		if (t <= 0) return;

		vec3 hitPoint = rayOrigin + t * rayDir;

		scene.setTarget(hitPoint * vec3(1.0f, 0.0f, 1.0f));
		
	}

	void onTimeElapsed(float startTime, float endTime) override {
		scene.Render();
		refreshScreen();
	}

};

AutodromoDeMaputo app;