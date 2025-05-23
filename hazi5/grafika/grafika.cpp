#include "framework.h"

const int tessellationLevel = 20;
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

	void Move(const int dir, float delta) {
		vec3 ward;
		switch (dir)
		{
		// forward
		case 1:
			ward = normalize(wLookat - wEye);
			break;
		// backward
		case 2:
			ward = normalize(wEye - wLookat);
			break;
		// right
		case 3:
			ward = normalize(cross(normalize(wLookat - wEye), wVup));
			break;
		// left
		case 4:
			ward = normalize(cross(wVup, normalize(wLookat - wEye)));
			break;
		default:
			return;
		}
		wEye += vec3(ward.x * delta, 0.0f, ward.z * delta);
		wLookat += vec3(ward.x * delta, 0.0f, ward.z * delta);
	}

	void LookAround(float yaw, float pitch) {
		vec3 direction;
		direction.x = cos(radians(yaw)) * cos(radians(pitch));
		direction.y = sin(radians(pitch));
		direction.z = sin(radians(yaw)) * cos(radians(pitch));
		direction = normalize(direction);
		wLookat = wEye + direction;
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

		vec3 tangent = normalize(cross(axis, vec3(0.0f, 0.0f, 1.0f)));
		if (length(tangent) < 1e-6f) tangent = normalize(cross(axis, vec3(0.0f, 1.0f, 0.0f)));
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
		mat4 M, Minv;
		SetModelingTransform(M, Minv);
		state.M = M;
		state.Minv = Minv;
		state.MVP = state.P * state.V * state.M;
		state.material = material;
		state.texture = texture;
		shader->Bind(state);
		if (geoObj) {
			geoObj->Draw();
		}
	}

	/** deprecated
	virtual void Animate(float tstart, float tend) {
		rotationAngle = 0.8f * tend;
	}
	*/
};

class Scene {
	std::vector<Object*> objects;
	std::vector<vec3> trisP1, trisP2, trisP3;
	std::vector<Light> lights;
	Camera camera;
public:
	void Build() {
		// Shaders
		Shader* phongShader = new PhongShader();

		// Materials
		Material* boardMaterial = new Material(vec3(1.0f, 1.0f, 1.0f), vec3(0.0f, 0.0f, 0.0f), vec3(2.0f), 100.0f);
		Material* goldenThing = new Material(vec3(0.17f, 0.35f, 1.5f), vec3(3.1f, 2.7f, 1.9f), vec3(0.51f, 1.05f, 4.5f), 150.0f);
		Material* waterThing = new Material(vec3(1.3f, 1.3f, 1.3f), vec3(0.1f, 0.1f, 0.1f), vec3(3.9f, 3.9f, 3.9f), 80.0f);
		Material* yellowPlastic = new Material(vec3(0.3f, 0.2f, 0.1f), vec3(2.0f, 2.0f, 2.0f), vec3(0.9f, 0.6f, 0.3f), 50.0f);
		Material* cyanPlastic = new Material(vec3(0.1f, 0.2f, 0.3f), vec3(2.0f, 2.0f, 2.0f), vec3(0.1f, 0.5f, 0.8f), 100.0f);
		Material* magentaPlastic = new Material(vec3(0.3f, 0.0f, 0.2f), vec3(2.0f, 2.0f, 2.0f), vec3(0.8f, 0.1f, 0.5f), 20.0f);

		// Textures
		Texture* boardTexture = new CheckerTexture(20, 20);

		// Create objects by setting up their vertex data on the GPU
		Object3d* checkerPlane = new Plane(vec3(0.0f, -1.0f, 0.0f), vec2(20.0f, 20.0f), vec3(0.0f, 1.0f, 0.0f));
		Object* board = new Object(phongShader, boardMaterial, checkerPlane, boardTexture);
		objects.push_back(board);

		Object3d* goldenCylinder = new Cylinder(vec3(1.0f, -1.0f, 0.0f), vec3(0.1f, 1.0f, 0.0f), 0.3f, 2.0f);
		Object* goldenC = new Object(phongShader, goldenThing, goldenCylinder);
		objects.push_back(goldenC);

		Object3d* waterCylinder = new Cylinder(vec3(0.0f, -1.0f, -0.8f), vec3(-0.2f, 1.0f, -0.1f), 0.3f, 2.0f);
		Object* waterC = new Object(phongShader, waterThing, waterCylinder);
		objects.push_back(waterC);

		Object3d* yellowCylinder = new Cylinder(vec3(-1.0f, -1.0f, 0.0f), vec3(0.0f, 1.0f, 0.1f), 0.3f, 2.0f);
		Object* yellowC = new Object(phongShader, yellowPlastic, yellowCylinder);
		objects.push_back(yellowC);

		Object3d* cyanCone = new Cone(vec3(0.0f, 1.0f, 0.0f), vec3(-0.1f, -1.0f, -0.05f), 2.0f, 0.2f);
		Object* cyanConeObject = new Object(phongShader, cyanPlastic, cyanCone);
		objects.push_back(cyanConeObject);

		Object3d* magentaCone = new Cone(vec3(0.0f, 1.0f, 0.8f), vec3(0.2f, -1.0f, 0.0f), 2.0f, 0.2f);
		Object* magentaConeObject = new Object(phongShader, magentaPlastic, magentaCone);
		objects.push_back(magentaConeObject);

		// Camera
		camera.wEye = vec3(0.0f, 1.0f, 4.0f);
		camera.wLookat = vec3(0.0f, 0.0f, 0.0f);
		camera.wVup = vec3(0.0f, 1.0f, 0.0f);

		// Lights
		lights.resize(1);
		lights[0].wLightPos = vec4(1.0f, 1.0f, 1.0f, 0.0f);
		lights[0].La = vec3(0.4f, 0.4f, 0.4f);
		lights[0].Le = vec3(2.0f, 2.0f, 2.0f);

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

	void Render() {
		RenderState state;
		state.wEye = camera.wEye;
		state.V = camera.V();
		state.P = camera.P();
		state.lights = lights;
		for (Object* obj : objects) obj->Draw(state);
	}

	/** deprecated
	void Animate(float tstart, float tend) {
		for (Object3d* obj : objects) obj->Animate(tstart, tend);
	}
	*/

	void Spin(const float angle = M_PI_4) {
		camera.Spin(angle);
	}

	void Move(const int dir, float delta = 0.2f) {
		camera.Move(dir, delta);
	}

	void LookAround(float yaw, float pitch) {
		camera.LookAround(yaw, pitch);
	}
};

class Kepszintezis : public glApp {
	Scene scene;

	float yaw = -90.0f;
	float pitch = 0.0f;
	float lastX = windowWidth / 2.0f;
	float lastY = windowHeight / 2.0f;
	bool firstMouse = true;
public:
	Kepszintezis() : glApp(3, 3, windowWidth, windowHeight, "Kepszintezis") {}

	void onInitialization() {
		glViewport(0, 0, windowWidth, windowHeight);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		scene.Build();
	}

	void onDisplay() {
		if (paused) return;
		glClearColor(0.4f, 0.4f, 0.4f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		scene.Render();
	}

	void onKeyboard(int key) override {
		printf("Key pressed: %d\n", key);
		if (key == 'q') {
			if (paused) return;
			scene.Spin();
			refreshScreen();
		}
		else if (key == 'w') {
			if (paused) return;
			scene.Move(1);
			refreshScreen();
		}
		else if (key == 's') {
			if (paused) return;
			scene.Move(2);
			refreshScreen();
		}
		else if (key == 'd') {
			if (paused) return;
			scene.Move(3);
			refreshScreen();
		}
		else if (key == 'a') {
			if (paused) return;
			scene.Move(4);
			refreshScreen();
		}
		else if (key == 'p') {
			paused = !paused;
		}
	}

	void onMouseMotion(int x, int y) override {
		if (paused) return;

		if (firstMouse) {
			lastX = x;
			lastY = y;
			firstMouse = false;
			return;
		}

		float sensitivity = 0.1f;
		float offsetX = (x - lastX) * sensitivity;
		float offsetY = (lastY - y) * sensitivity;

		lastX = x;
		lastY = y;

		yaw += offsetX;
		pitch += offsetY;

		if (pitch > 89.0f) pitch = 89.0f;
		if (pitch < -89.0f) pitch = -89.0f;

		scene.LookAround(yaw, pitch);
		refreshScreen();
	}

};

Kepszintezis app;