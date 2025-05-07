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
	layout(location = 2) in vec2 texcoord;

	out vec3 wNormal;
	out vec3 wView;
	out vec3 wLight;
	out vec2 textcoord;

	void main() {
	   gl_Position = MVP * vec4(vtxPos, 1);

	   vec4 wPos = M * vec4(vtxPos, 1);
	   wLight  = wLiPos.xyz/wLiPos.w - wPos.xyz/wPos.w;
	   wView   = wEye - wPos.xyz/wPos.w;
	   wNormal = (vec4(vtxNorm, 0) * Minv).xyz;
	   textcoord = texcoord;
	}
)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
	uniform sampler2D textureUnit;
	uniform vec3 kd, ks, ka;
	uniform float shine;
	uniform vec3 La, Le;

	in vec3 wNormal;
	in vec3 wView;
	in vec3 wLight;
	in vec2 textcoord;

	out vec4 fragmentColor;     
	
	void main() {
		vec3 N = normalize(wNormal);
		vec3 V = normalize(wView);  
		vec3 L = normalize(wLight);
		vec3 H = normalize(L + V);
		float cost = max(dot(N,L), 0), cosd = max(dot(N,H), 0);
		vec3 textColor = texture(textureUnit, textcoord).xyz;
		vec3 color = ka * La + (kd * textColor * cost + ks * pow(cosd,shine)) * Le;
	   
		fragmentColor = vec4(color, 1);
	}
)";

struct Light {
	vec3 direction;
	vec3 Le;
	Light(vec3 _direction, vec3 _Le) {
		direction = normalize(_direction);
		Le = _Le;
	}
};

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
	vec3 scaleVec, translation, rotationAxis;
	float rotationAngle;
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

	virtual void SetModelingTransform(mat4& M, mat4& Minv) {
		M = scale(scaleVec) * rotate(rotationAngle, rotationAxis) * translate(translation);
		Minv = translate(-translation) * rotate(-rotationAngle, rotationAxis) * scale(vec3(1 / scaleVec.x, 1 / scaleVec.y, 1 / scaleVec.z));
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
			glDrawArrays(GL_TRIANGLE_STRIP, i * nVtxInStrip, nVtxInStrip);
	}


	virtual ~Object3D() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
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
		vec3 w = axis;
		vec3 up = vec3(0.0f, 1.0f, 0.0f);
		if (abs(dot(w, up)) > 0.999f) up = vec3(1.0f, 0.0f, 0.0f);
		vec3 uDir = normalize(cross(up, w));
		vec3 vDir = normalize(cross(w, uDir));

		float theta = u * 2.0f * M_PI;
		float cosTheta = cosf(theta);
		float sinTheta = sinf(theta);
		vec3 circlePoint = radius * (cosTheta * uDir + sinTheta * vDir);

		vtx.pos = base + circlePoint + v * height * w;
		vtx.normal = normalize(circlePoint);
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
	GPUProgram* gpuProg;
	Camera* camera;
	Light* light;
public:
	Scene(Camera* _camera, GPUProgram* _gpuProg) : gpuProg(_gpuProg), camera(_camera) {}

	void Build() {
		// Light 
		light = new Light(vec3(1.0f, 1.0f, 1.0f), vec3(2.5f, 2.5f, 2.5f));

		gpuProg->setUniform(light->direction, "wLiPos");
		gpuProg->setUniform(camera->getEye(), "wEye");

		gpuProg->setUniform(light->Le, "Le");
		gpuProg->setUniform(vec3(0.4f, 0.4f, 0.4f), "La");

		// Materials
		Material* gold = new Material(vec3(0.17f, 0.35f, 1.5f), vec3(3.1f, 2.7f, 1.9f), 0.0f);
		Material* blue = new Material(vec3(0.0f, 0.0f, 1.0f), vec3(1.0f, 1.0f, 1.0f), 5.0f);
		
		// Scene objects
		Cylinder* cyl = new Cylinder(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), 1.0f, 2.0f, blue);
		cyl->create(12, 8);
		objects.push_back(cyl);
	}
	void addObject(Object3D* obj) {
		objects.push_back(obj);
	}
	void render() {
		for (auto obj : objects) {
			obj->draw(gpuProg);
		}
	}
};

class Kepszintezis : public glApp {
	GPUProgram* gpuProg;	   // csúcspont és pixel árnyalók
	Camera* camera;	   // kamera
	FullScreenTexturedQuad* quad;
	std::vector<vec3> image;
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

		scene = new Scene(camera, gpuProg);
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
		mat4 M = mat4(1.0f);
		mat4 Minv = inverse(M);
		gpuProg->setUniform(M, "M");
		gpuProg->setUniform(Minv, "Minv");
		gpuProg->setUniform(camera->P() * camera->V(), "MVP");

		scene->render();
	}
};

Kepszintezis app;

