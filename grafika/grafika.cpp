#include "framework.h"

const int windowWidth = 600, windowHeight = 600;

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330

	uniform mat4 M, Minv, MVP;

	layout(location = 0) in vec3 vtxPos;
	layout(location = 1) in vec3 vtxNorm;

	out vec3 color;

	void main() {
		gl_Position = MVP * vec4(vtxPos, 1);
		vec4 wPos = M * vec4(vtxPos, 1);
		vec4 wNormal = vec4(vtxNorm, 0) * Minv;
		color = Illumination(wPos, wNormal);
	}
)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
	uniform vec3 kd, ks, ka;
	uniform float shine;
	uniform vec3 La, Le;

	in vec3 wNormal;
	in vec3 wView;
	in vec3 wLight;
	// in vec3 wPos;
	out vec4 fragmentColor;

	bool intersectTriangle(vec3 orig, vec3 dir, vec3 v0, vec3 v1, vec3 v2, out float t) {
		vec3 edge1 = v1 - v0;
		vec3 edge2 = v2 - v0;
		vec3 h = cross(dir, edge2);
		float a = dot(edge1, h);
		if (abs(a) < 1e-5) return false;

		float f = 1.0 / a;
		vec3 s = orig - v0;
		float u = f * dot(s, h);
		if (u < 0.0 || u > 1.0) return false;

		vec3 q = cross(s, edge1);
		float v = f * dot(dir, q);
		if (v < 0.0 || u + v > 1.0) return false;

		t = f * dot(edge2, q);
		return (t > 1e-3);
	}

	bool inShadow(vec3 origin, vec3 lightDir) {
		float t;
		for (int i = 0; i < triangleCount; ++i) {
			vec3 v0 = triangles[i * 3 + 0];
			vec3 v1 = triangles[i * 3 + 1];
			vec3 v2 = triangles[i * 3 + 2];
			if (intersectTriangle(origin + lightDir * 1e-4, lightDir, v0, v1, v2, t)) {
				return true;
			}
		}
		return false;
	}

	void main() {
		vec3 N = normalize(wNormal);
		vec3 V = normalize(wView);
		vec3 L = normalize(wLight);
		vec3 H = normalize(L + V);

		float cost = max(dot(N,L), 0);
		float cosd = max(dot(N,H), 0);

		bool shadowed = inShadow(wPos, normalize(wLight));

		vec3 color = ka * La;
		if (!shadowed) {
			color += kd * cost * Le + ks * pow(cosd, shine) * Le;
		}

		fragmentColor = vec4(color, 1);
	}
)";

struct Material {
	vec3 ka, kd, ks;
	float shininess;
	Material() {}
	Material(vec3 _kd, vec3 _ks, float _shininess)
		: ka(_kd* (float)M_PI), kd(_kd), ks(_ks), shininess(_shininess) {
	}
	~Material() {}

	vec3 fresnelReflectance(const float cosTheta, const vec3 F0) const {
		float clampedCosTheta = clamp(cosTheta, 0.0f, 1.0f);
		return F0 + (vec3(1.0f) - F0) * pow(1.0f - clampedCosTheta, 5.0f);
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
public:
	Object3D() {
		glEnableVertexAttribArray(0); // 0. regiszter = pozíció
		glEnableVertexAttribArray(1); // 1. regiszter = normál vektor
		glEnableVertexAttribArray(2); // 2. regiszter = textúra koordináta
		int nb = sizeof(VtxData);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, nb, (void*)offsetof(VtxData, pos));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, nb, (void*)offsetof(VtxData, normal));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, nb, (void*)offsetof(VtxData, texcoord));
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

	void draw() {
		bind();
		for (unsigned int i = 0; i < nStrips; i++)
			glDrawArrays(GL_TRIANGLE_STRIP, i * nVtxInStrip, nVtxInStrip);
	}


	virtual ~Object3D() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
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

class Kepszintezis : public glApp {
	GPUProgram* gpuProg;	   // csúcspont és pixel árnyalók
	Camera* camera;	   // kamera
	FullScreenTexturedQuad* quad;
public:
	Kepszintezis() : glApp("Inkrementalas") {}

	// Inicializáció, 
	void onInitialization() {
		glViewport(0, 0, windowWidth, windowHeight);
		glEnable(GL_DEPTH_TEST);
		gpuProg = new GPUProgram(vertSource, fragSource);
		quad = new FullScreenTexturedQuad();
		camera = new Camera(vec3(0.0f, 1.0f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

		Material* gold = new Material(vec3(0.17f, 0.35f, 1.5f), vec3(3.1f, 2.7f, 1.9f), 0.0f);
	}

	void onKeyboard(int key) override {
		if (key == 'a') {
			camera->Spin();
		}
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0.4f, 0.4f, 0.4f, 0.0f);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, windowWidth, windowHeight);
	}
};

Kepszintezis app;

