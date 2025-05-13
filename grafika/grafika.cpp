#include "framework.h"

template<class T> struct Dnum { // Dual numbers for automatic derivation
	float f; // function value
	T d;  // derivatives
	Dnum(float f0 = 0, T d0 = T(0)) { f = f0, d = d0; }
	Dnum operator+(Dnum r) { return Dnum(f + r.f, d + r.d); }
	Dnum operator-(Dnum r) { return Dnum(f - r.f, d - r.d); }
	Dnum operator*(Dnum r) {
		return Dnum(f * r.f, f * r.d + d * r.f);
	}
	Dnum operator/(Dnum r) {
		return Dnum(f / r.f, (r.f * d - r.d * f) / r.f / r.f);
	}
};

// Elementary functions prepared for the chain rule as well
template<class T> Dnum<T> Exp(Dnum<T> g) { return Dnum<T>(expf(g.f), expf(g.f) * g.d); }
template<class T> Dnum<T> Sin(Dnum<T> g) { return  Dnum<T>(sinf(g.f), cosf(g.f) * g.d); }
template<class T> Dnum<T> Cos(Dnum<T>  g) { return  Dnum<T>(cosf(g.f), -sinf(g.f) * g.d); }
template<class T> Dnum<T> Tan(Dnum<T>  g) { return Sin(g) / Cos(g); }
template<class T> Dnum<T> Sinh(Dnum<T> g) { return  Dnum<T>(sinh(g.f), cosh(g.f) * g.d); }
template<class T> Dnum<T> Cosh(Dnum<T> g) { return  Dnum<T>(cosh(g.f), sinh(g.f) * g.d); }
template<class T> Dnum<T> Tanh(Dnum<T> g) { return Sinh(g) / Cosh(g); }
template<class T> Dnum<T> Log(Dnum<T> g) { return  Dnum<T>(logf(g.f), g.d / g.f); }
template<class T> Dnum<T> Pow(Dnum<T> g, float n) {
	return  Dnum<T>(powf(g.f, n), n * powf(g.f, n - 1) * g.d);
}

typedef Dnum<vec2> Dnum2;

const int tessellationLevel = 20;
const int windowWidth = 600, windowHeight = 600;

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
		printf("You spin my head right round, right round... by [45] degrees\n");
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
		const vec3 _color1 = vec3(0.0f, 0.2f, 0.6f), const vec3 _color2 = vec3(0.6f, 0.6f, 0.6f))
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
		const int maxTriangles = 265;

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
		uniform vec3 numOfTriangles;
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
			if (dot(N, V) < 0) N = -N;	// prepare for one-sided surfaces like Mobius or Klein
			vec3 texColor = useTexture ? texture(diffuseTexture, texcoord).rgb : vec3(1.0f);
			vec3 ka = material.ka * texColor;
			vec3 kd = material.kd * texColor;

			vec3 radiance = vec3(0, 0, 0);
			for(int i = 0; i < nLights; i++) {
				vec3 L = normalize(wLight[i]);
				vec3 H = normalize(L + V);
				float cost = max(dot(N,L), 0);
				float cosd = max(dot(N,H), 0);

				radiance += ka * lights[i].La + 
                           (kd * cost + material.ks * pow(cosd, material.shininess)) * lights[i].Le;
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
	Plane(vec3 _center, vec2 _size, vec3 _normal) {
		std::vector<VertexData> verticles;
		
		_normal = normalize(_normal);
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
	Cylinder(vec3 base, vec3 axis, float radius, float height) {
		axis = normalize(axis);
		vec3 top = base + axis * height;

		vec3 tangent = normalize(cross(axis, vec3(0, 0, 1)));
		if (length(tangent) < 1e-6f) tangent = normalize(cross(axis, vec3(0, 1, 0)));
		vec3 bitangent = normalize(cross(axis, tangent));

		const int slices = 6;
		const float angleStep = 2.0f * M_PI / slices;

		for (int i = 0; i < slices; i++) {
			float a0 = i * angleStep;
			float a1 = (i + 1) % slices * angleStep;

			vec3 p0 = base + radius * (cos(a0) * tangent + sin(a0) * bitangent);
			vec3 p1 = base + radius * (cos(a1) * tangent + sin(a1) * bitangent);
			vec3 p2 = top + radius * (cos(a0) * tangent + sin(a0) * bitangent);
			vec3 p3 = top + radius * (cos(a1) * tangent + sin(a1) * bitangent);

			vec3 n0 = normalize(p0 - base);
			vec3 n1 = normalize(p1 - base);
			vec3 n2 = normalize(p2 - top);
			vec3 n3 = normalize(p3 - top);

			vertices.push_back({ p0, n0, vec2(0, 0) });
			vertices.push_back({ p1, n1, vec2(1, 0) });
			vertices.push_back({ p2, n0, vec2(0, 1) });

			vertices.push_back({ p2, n0, vec2(0, 1) });
			vertices.push_back({ p1, n1, vec2(1, 0) });
			vertices.push_back({ p3, n1, vec2(1, 1) });
		}

		for (int i = 0; i < slices; i++) {
			float a0 = i * angleStep;
			float a1 = (i + 1) % slices * angleStep;

			vec3 p0 = base + radius * (cos(a0) * tangent + sin(a0) * bitangent);
			vec3 p1 = base + radius * (cos(a1) * tangent + sin(a1) * bitangent);

			vec3 normal = -axis;
			vertices.push_back({ base, normal, vec2(0.5f, 0.5f) });
			vertices.push_back({ p1, normal, vec2(0.5f + 0.5f * cos(a1), 0.5f + 0.5f * sin(a1)) });
			vertices.push_back({ p0, normal, vec2(0.5f + 0.5f * cos(a0), 0.5f + 0.5f * sin(a0)) });
		}

		for (int i = 0; i < slices; i++) {
			float a0 = i * angleStep;
			float a1 = (i + 1) % slices * angleStep;

			vec3 p0 = top + radius * (cos(a0) * tangent + sin(a0) * bitangent);
			vec3 p1 = top + radius * (cos(a1) * tangent + sin(a1) * bitangent);

			vec3 normal = axis;
			vertices.push_back({ top, normal, vec2(0.5f, 0.5f) });
			vertices.push_back({ p0, normal, vec2(0.5f + 0.5f * cos(a0), 0.5f + 0.5f * sin(a0)) });
			vertices.push_back({ p1, normal, vec2(0.5f + 0.5f * cos(a1), 0.5f + 0.5f * sin(a1)) });
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
		Minv = scale(vec3(1 / scaleing.x, 1 / scaleing.y, 1 / scaleing.z)) * rotate(-rotationAngle, rotationAxis) * translate(-translation);
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

		Material* boardMaterial = new Material(vec3(1.0f, 1.0f, 1.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), 100.0f);

		Material* yellowPlastic = new Material(vec3(0.3f, 0.2f, 0.1f), vec3(2.0f, 2.0f, 2.0f), vec3(0.1f), 50.0f);

		Material* cyanPlastic = new Material(vec3(0.1f, 0.2f, 0.3f), vec3(2.0f, 2.0f, 2.0f), vec3(0.1f, 0.1f, 0.1f), 100.0f);

		Material* magentaPlastic = new Material(vec3(0.3f, 0.0f, 0.2f), vec3(2.0f, 2.0f, 2.0f), vec3(0.1f, 0.1f, 0.1f), 20.0f);

		Material* waterThing = new Material(vec3(1.3f, 1.3f, 1.3f), vec3(0.0f), vec3(0.1f, 0.1f, 0.1f), 1.0f);

		Texture* boardTexture = new CheckerTexture(20, 20);

		// Create objects by setting up their vertex data on the GPU
		Object3d* checkerPlane = new Plane(vec3(0.0f, -1.0f, 0.0f), vec2(20.0f, 20.0f), vec3(0.0f, 1.0f, 0.0f));

		Object* board = new Object(phongShader, boardMaterial, checkerPlane, boardTexture);
		objects.push_back(board);

		Object3d* yellowCylinder = new Cylinder(vec3(-1.0f, -1.0f, 0.0f), vec3(0.0f, 1.0f, 0.1f), 0.3f, 2.0f);
		Object* yellowC = new Object(phongShader, yellowPlastic, yellowCylinder);
		objects.push_back(yellowC);

		// Camera
		camera.wEye = vec3(0.0f, 1.0f, 4.0f);
		camera.wLookat = vec3(0.0f, 0.0f, 0.0f);
		camera.wVup = vec3(0.0f, 1.0f, 0.0f);

		// Lights
		lights.resize(1);
		lights[0].wLightPos = vec4(1.0f, 1.0f, 1.0f, 0.0f);
		lights[0].La = vec3(0.4f, 0.4f, 0.4f);
		lights[0].Le = vec3(3.0f, 3.0f, 3.0f);

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
};

class Kepszintezis : public glApp {
	Scene scene;
public:
	Kepszintezis() : glApp("Kepszintezis") {}

	void onInitialization() {
		glViewport(0, 0, windowWidth, windowHeight);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		scene.Build();
	}

	void onDisplay() {
		glClearColor(0.5f, 0.5f, 0.8f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		scene.Render();
	}

	void onKeyboard(int key) override {
		printf("Key pressed: %d\t", key);
		if (key == 'a') {
			scene.Spin();
			refreshScreen();
		}
	}
};

Kepszintezis app;