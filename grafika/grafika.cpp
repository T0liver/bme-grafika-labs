#include "framework.h"

const int windowWidth = 600, windowHeight = 600;

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330

	uniform mat4  MVP, M, Minv;
	uniform vec4  wLiPos;
	uniform vec3  wEye;

	layout(location = 0) in vec3 vtxPos;
	layout(location = 1) in vec3 vtxNorm;

	out vec3 wNormal;
	out vec3 wView;
	out vec3 wLight;

	void main() {
	   gl_Position = MVP * vec4(vtxPos, 1);

	   vec4 wPos = M * vec4(vtxPos, 1);
	   wLight  = wLiPos.xyz/wLiPos.w - wPos.xyz/wPos.w;
	   wView   = wEye - wPos.xyz/wPos.w;
	   wNormal = (vec4(vtxNorm, 0) * Minv).xyz;
	}
)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
	uniform vec3 kd, ks, ka;
	uniform float shine;
	uniform vec3 La, Le;

	in  vec3 wNormal;
	in  vec3 wView;
	in  vec3 wLight;
	out vec4 fragmentColor;     
	
	void main() {
	   vec3 N = normalize(wNormal);
	   vec3 V = normalize(wView);  
	   vec3 L = normalize(wLight);
	   vec3 H = normalize(L + V);
	   float cost = max(dot(N,L), 0), cosd = max(dot(N,H), 0);
	   vec3 color = ka * La + (kd * cost + ks * pow(cosd,shine)) * Le;
	   //vec3 color = vec3(0.0f, 1.0f, 0.0f);
	   fragmentColor = vec4(color, 1);
	}
)";

class Material {
public:
	vec3 ka, kd, ks;
	float shininess;
	Material() {}
	Material(vec3 _kd, vec3 _ks, float _shininess)
		: ka(_kd* (float)M_PI), kd(_kd), ks(_ks), shininess(_shininess) {
	}
	~Material() {}

	void uploadGPU(GPUProgram* gpuProg) {
		gpuProg->setUniform(ka, "ka");
		gpuProg->setUniform(kd, "kd");
		gpuProg->setUniform(ks, "ks");
		gpuProg->setUniform(shininess, "shine");
	}
};

class Camera {
	vec3 wEye, wLookat, wVup;
	float fov, asp, fp, bp;
public:
	Camera(vec3 eye, vec3 lookat, vec3 vup) : wEye(eye), wLookat(lookat), wVup(vup) {
		asp = (float)windowWidth / windowHeight;
		fov = 40.0f * (float)M_PI / 180.0f;
		fp = 1;
		bp = 20;
	}

	mat4 V() { return lookAt(wEye, wLookat, wVup); }

	mat4 P() { return perspective(fov, asp, fp, bp); }

	vec3 getEye() const { return wEye; }

	void Spin(const float angle = M_PI_4) {
		vec3 d = wEye - wLookat;

		wEye = vec3(
			d.x * cos(angle) + d.z * sin(angle),
			d.y,
			-d.x * sin(angle) + d.z * cos(angle)
		) + wLookat;
	}
};

struct VtxData {
	vec3 pos, normal;
	vec2 texcoord;
};

class Object3D {
	unsigned int vao, vbo;
	std::vector<VtxData> vtx;
	int nVtxInStrip, nStrips;
	Material* material;
public:
	Object3D() {
		// Geometry
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		// ParamSurface
		glEnableVertexAttribArray(0); // 0. regiszter = pozíció
		glEnableVertexAttribArray(1); // 1. regiszter = normál vektor
		glEnableVertexAttribArray(2); // 2. regiszter = textúra koordináta
		int nb = sizeof(VtxData);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, nb, (void*)offsetof(VtxData, pos));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, nb, (void*)offsetof(VtxData, normal));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, nb, (void*)offsetof(VtxData, texcoord));
	}

	void setMaterial(Material* _material) {
		material = _material;
	}

	virtual VtxData GenVtxData(float u, float v) = 0;

	void updateGPU() {
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(VtxData), &vtx[0], GL_DYNAMIC_DRAW);
	}

	void bind() {
		glBindVertexArray(vao);
	}


	void create(int M, int N) {
		nVtxInStrip = (M + 1) * 2;
		nStrips = N;
		for (int i = 0; i < N; i++) {
			for (int j = 0; j <= M; j++) {
				vtx.push_back(GenVtxData((float)j / M, (float)i / N));
				vtx.push_back(GenVtxData((float)j / M, (float)(i + 1) / N));
			}
		}
		updateGPU();
	}

	void draw(GPUProgram* gpuProg) {
		bind();
		material->uploadGPU(gpuProg);
		glPointSize(10.0f);
		for (unsigned int i = 0; i < nStrips; i++)
			glDrawArrays(GL_POINTS, 0, 1);
			//glDrawArrays(GL_POINTS, i * nVtxInStrip, nVtxInStrip);
	}


	virtual ~Object3D() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}
};

class Triangle : public Object3D {
	vec3 p1, p2, p3;
public:
	Triangle(vec3 _p1, vec3 _p2, vec3 _p3, Material* _material)
		: p1(_p1), p2(_p2), p3(_p3) {
		setMaterial(_material);
	}
	VtxData GenVtxData(float u, float v) override {
		VtxData vtx;
		vtx.pos = (1 - u - v) * p1 + u * p2 + v * p3;
		vtx.normal = normalize(cross(p2 - p1, p3 - p1));
		vtx.texcoord = vec2(u, v);
		return vtx;
	}
};

class Point : public Object3D {
	vec3 p;
public:
	Point(vec3 _p, Material* _material)
		: p(_p) {
		setMaterial(_material);
	}
	VtxData GenVtxData(float u, float v) override {
		VtxData vtx;
		vtx.pos = p;
		vtx.normal = vec3(1.0f, 1.0f, 1.0f);
		vtx.texcoord = vec2(u, v);
		return vtx;
	}
};

class Cylinder : public Object3D {
	vec3 base, axis;
	float radius, height;
public:
	Cylinder(vec3 _base, vec3 _axis, float _radius, float _height, Material* _material)
		: base(_base), axis(normalize(_axis)), radius(_radius), height(_height) {
		setMaterial(_material);
	}

	VtxData GenVtxData(float u, float v) override {
		VtxData vtx;
		float theta = u * 2.0f * M_PI;
		vec3 tangent = normalize(cross(axis, vec3(0, 1, 0)));
		if (length(tangent) < 1e-3f)
			tangent = normalize(cross(axis, vec3(1, 0, 0)));
		vec3 bitangent = normalize(cross(axis, tangent));

		vec3 circlePos = radius * (cosf(theta) * tangent + sinf(theta) * bitangent);
		vec3 heightOffset = v * height * axis;
		vtx.pos = base + circlePos + heightOffset;
		vtx.normal = normalize(circlePos);
		vtx.texcoord = vec2(u, v);

		return vtx;
	}
};


class FullScreenTexturedQuad : public Geometry<vec2> {
	Texture* texture = nullptr;
public:
	FullScreenTexturedQuad() {
		vtx = { vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, 1) };
		updateGPU();
	}
	void LoadTexture(int width, int height, std::vector<vec3>& image) {
		delete texture;
		texture = new Texture(width, height);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_FLOAT, &image[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	void Draw(GPUProgram* program) {
		Bind();
		if (texture) texture->Bind(0);
		program->setUniform(0, "textureUnit");
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
	~FullScreenTexturedQuad() { delete texture; }
};

class Scene {
	std::vector<Object3D*> objects;
	Camera* camera;
public:
	Scene(Camera* _camera) : camera(_camera) {}

	void Build() {
		// Scene objects
		Cylinder* cyl;
		Point* point;
		Triangle* triangle;

		// Materials
		Material* gold = new Material(vec3(0.17f, 0.35f, 1.5f), vec3(3.1f, 2.7f, 1.9f), 0.0f);
		Material* blue = new Material(vec3(0.0f, 0.0f, 1.0f), vec3(1.0f, 1.0f, 1.0f), 1.0f);
		
		// Creating objects
		cyl = new Cylinder(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), 1.0f, 2.0f, gold);
		cyl->create(12, 8);
		objects.push_back(cyl);

		point = new Point(vec3(0.0f, 0.0f, 1.0f), blue);
		point->create(1, 1);
		objects.push_back(point);

		triangle = new Triangle(vec3(0.0f, 0.0f, 1.0f), vec3(1.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 1.0f), blue);
		triangle->create(1, 1);
		objects.push_back(triangle);
	}
	void addObject(Object3D* obj) {
		objects.push_back(obj);
	}
	void render(GPUProgram* gpuProg) {
		for (auto obj : objects) {
			obj->draw(gpuProg);
		}
	}
};

class Kepszintezis : public glApp {
	GPUProgram* gpuProg;	   // csúcspont és pixel árnyalók
	Camera* camera;	   // kamera
	FullScreenTexturedQuad* quad;
	Scene* scene;
public:
	Kepszintezis() : glApp("Inkrementalas") {}

	// Inicializáció, 
	void onInitialization() {
		glViewport(0, 0, windowWidth, windowHeight);
		glEnable(GL_DEPTH_TEST);
		gpuProg = new GPUProgram(vertSource, fragSource);
		quad = new FullScreenTexturedQuad();
		camera = new Camera(vec3(0.0f, 0.0f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

		scene = new Scene(camera);
		scene->Build();
	}

	void onKeyboard(int key) override {
		if (key == 'a') {
			camera->Spin();
			refreshScreen();
		}
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0.4f, 0.4f, 0.4f, 1.0f);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, windowWidth, windowHeight);
		gpuProg->setUniform(camera->P() * camera->V(), "MVP");

		scene->render(gpuProg);
	}
};

Kepszintezis app;

