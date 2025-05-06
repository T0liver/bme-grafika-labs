#include "framework.h"

const int windowWidth = 600, windowHeight = 600;
const vec3 bgColor(0.4f, 0.4f, 0.4f);

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330 core

	uniform mat4 MVP, M, Minv;
	uniform vec4 wLiPos;
	uniform vec3 wEye;

	layout(location = 0) in vec3 vtxPos;
	layout(location = 1) in vec3 vtxNorm;

	out vec3 wNormal;
	out vec3 wView;
	out vec3 wLight;
	out vec3 wPos;

	void main() {
		gl_Position = MVP * vec4(vtxPos, 1.0);
		vec4 worldPos = M * vec4(vtxPos, 1.0);
		wPos = worldPos.xyz / worldPos.w;
		wLight = wLiPos.xyz * worldPos.w - worldPos.xyz * wLiPos.w;
		wView = wEye - wPos;
		wNormal = (vec4(vtxNorm, 0.0) * Minv).xyz;
	}
)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans szín
	out vec4 fragmentColor;		// pixel szín

	void main() {
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

struct Material {
	vec3 ka, kd, ks;
	float shininess;
	Material() {}
	Material(vec3 _kd, vec3 _ks, float _shininess)
		: ka(_kd* (float)M_PI), kd(_kd), ks(_ks),shininess(_shininess) {
	}
	~Material() {}

	vec3 fresnelReflectance(const float cosTheta, const vec3 F0) const {
		float clampedCosTheta = clamp(cosTheta, 0.0f, 1.0f);
		return F0 + (vec3(1.0f) - F0) * pow(1.0f - clampedCosTheta, 5.0f);
	}
};

struct Hit {
	float t;
	vec3 position, normal;
	Material* material;
	Hit() : t(-1) {}
};

struct Ray {
	vec3 start, dir;
	bool out;
	Ray(vec3 _start, vec3 _dir) : start(_start), dir(normalize(_dir)) {}
	Ray(vec3 _start, vec3 _dir, bool _out) : start(_start), dir(normalize(_dir)), out(_out) {}
};

struct Light {
	vec3 direction;
	vec3 Le;
	Light(vec3 _direction, vec3 _Le) {
		direction = normalize(_direction);
		Le = _Le;
	}
};

struct Triangle {
	vec3 p0, p1, p2;
};

class Object3D {
protected:
	unsigned int vao, vbo;
	std::vector<vec3> vtx;
	std::vector<Triangle> triangles;

public:
	Object3D() {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
	}
	virtual ~Object3D() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}

	void bind() const {
		glBindVertexArray(vao);
	}
	void unbind() const {
		glBindVertexArray(0);
	}

	virtual void tessellate() = 0;

	void uploadToGPU() {
		std::vector<glm::vec3> triVtx;
		for (const Triangle& tri : triangles) {
			triVtx.push_back(tri.p0);
			triVtx.push_back(tri.p1);
			triVtx.push_back(tri.p2);
		}
		vtx = triVtx;

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, triVtx.size() * sizeof(vec3), triVtx.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
		glBindVertexArray(0);
	}

	void uploadAsUniform(unsigned int shader, const mat4& mMtx, const std::string& uniformName) {
		std::vector<vec3> worldVtx;
		for (const Triangle& tri : triangles) {
			worldVtx.push_back(mMtx * vec4(tri.p0, 1.0f));
			worldVtx.push_back(mMtx * vec4(tri.p1, 1.0f));
			worldVtx.push_back(mMtx * vec4(tri.p2, 1.0f));
		}

		glUniform3fv(glGetUniformLocation(shader, uniformName.c_str()), worldVtx.size(), &worldVtx[0].x);
	}
};

class Cylinder : public Object3D {
	int slices;
	float height, radius;

public:
	Cylinder(float _height, float _radius, int _slices = 32) : slices(_slices), height(_height), radius(_radius) {
		tessellate();
		uploadToGPU();
	}

	void tessellate() override {
		vtx.clear();
		float halfHeight = height / 2.0f;
		for (int i = 0; i <= slices; ++i) {
			float theta = 2.0f * M_PI * (float(i) / float(slices));
			float x = radius * cos(theta);
			float z = radius * sin(theta);
			// vtx.push_back(vec3(x, -halfHeight, z));
			// vtx.push_back(vec3(x, halfHeight, z));
			vtx.push_back(vec3(x, z, -halfHeight));
			vtx.push_back(vec3(x, z, halfHeight));
		}
	}
};

class Cone : public Object3D {
	int slices;
	float height, radius;

public:
	Cone(float _height, float _radius, int _slices = 32) : slices(_slices), height(_height), radius(_radius) {
		tessellate();
		uploadToGPU();
	}

	void tessellate() override {
		vtx.clear();
		float halfHeight = height / 2.0f;
		vec3 top(0, 0, halfHeight);
		vec3 center(0, 0, -halfHeight);

		for (int i = 0; i <= slices; ++i) {
			float theta = 2.0f * M_PI * (float(i) / float(slices));
			float x = radius * cos(theta);
			float y = radius * sin(theta);
			vec3 base(x, y, -halfHeight);

			vtx.push_back(top);
			vtx.push_back(base);

			float nextTheta = 2.0f * M_PI * (float(i + 1) / float(slices));
			float nextX = radius * cos(nextTheta);
			float nextY = radius * sin(nextTheta);
			vec3 nextBase(nextX, nextY, -halfHeight);

			vtx.push_back(nextBase);
		}
	}
};

class Quad : public Object3D {
	float size;
public:
	Quad(float _size) : size(_size) {
		tessellate();
		uploadToGPU();
	}

	void tessellate() override {
		vtx.clear();
		float halfSize = size / 2.0f;
		vtx.push_back(vec3(-halfSize, -halfSize, 0));
		vtx.push_back(vec3(halfSize, -halfSize, 0));
		vtx.push_back(vec3(halfSize, halfSize, 0));
		vtx.push_back(vec3(-halfSize, halfSize, 0));
	}
};

class Camera {
	vec3 eye, lookat, right, up, vupWorld;
	float fov;
public:
	void set(vec3 _eye, vec3 _lookat, vec3 _vup, float _fov) {
		eye = _eye; lookat = _lookat; fov = _fov;
		vupWorld = _vup;
		vec3 w = eye - lookat;
		float windowSize = length(w) * tanf(fov / 2);
		right = normalize(cross(_vup, w)) * (float)windowSize * (float)windowWidth / (float)windowHeight;
		up = normalize(cross(w, right)) * windowSize;
	}

	Ray getRay(int X, int Y) {
		vec3 dir = lookat + right * (2.0f * (X + 0.5f) / windowWidth - 1.0f) + up * (2.0f * (Y + 0.5f) / windowHeight - 1.0f) - eye;
		return Ray(eye, dir);
	}

	void Animate(float dt) {
		vec3 d = eye - lookat;
		eye = vec3(d.x * cos(dt) + d.z * sin(dt), d.y, -d.x * sin(dt) + d.z * cos(dt)) + lookat;
		set(eye, lookat, up, fov);
	}

	void Spin(const float angle = M_PI_4) {
		vec3 d = eye - lookat;

		eye = vec3(
			d.x * cos(angle) + d.z * sin(angle),
			d.y,
			-d.x * sin(angle) + d.z * cos(angle)
		) + lookat;

		set(eye, lookat, vupWorld, fov);
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
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
	Camera* camera;	   // kamera
public:
	Kepszintezis() : glApp("Inkrementalas") {}

	// Inicializáció, 
	void onInitialization() {
		gpuProgram = new GPUProgram(vertSource, fragSource);
	}

	void onKeyboard(int key) override {
		if (key == 'a') {
			camera->Spin();
			// printf("You spin my head right round, right round... by [45] degrees\n");
		}
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, windowWidth, windowHeight);
	}
};

Kepszintezis app;

