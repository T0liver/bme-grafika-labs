#include "framework.h"

const int windowWidth = 600, windowHeight = 600;
const float Epsilon = 0.0001f;

struct Material {
	vec3 ka, kd, ks;
	float shininess;
	Material(vec3 _kd, vec3 _ks, float _shininess) : ka(_kd* (float)M_PI), kd(_kd), ks(_ks), shininess(_shininess) {}
};

struct Hit {
	float t;
	vec3 position, normal;
	Material* material;
	Hit() : t(-1) {}
};

struct Ray {
	vec3 start, dir;
	Ray(vec3 _start, vec3 _dir) : start(_start), dir(normalize(_dir)) {}
};

class Intersectable {
protected:
	Material* material;
public:
	virtual Hit intersect(const Ray& ray) = 0;
};

const char* vertexSource = R"(
	#version 330
	layout(location = 0) in vec2 cVertexPosition;
	out vec2 texcoord;
	void main() {
		texcoord = (cVertexPosition + vec2(1,1)) * 0.5;
		gl_Position = vec4(cVertexPosition, 0, 1);
	}
)";

const char* fragmentSource = R"(
	#version 330
	uniform sampler2D textureUnit;
	in vec2 texcoord;
	out vec4 fragmentColor;
	void main() {
		fragmentColor = texture(textureUnit, texcoord);
	}
)";

class FullScreenTexturedQuad : public Geometry<vec2> {
	Texture* texture = nullptr;
public:
	FullScreenTexturedQuad() {
		vtx = { vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, 1) };
		updateGPU();
	}
	void LoadTexture(int width, int height, std::vector<vec3>& image) {
		delete texture;
		texture = new Texture(width, height, image);
	}
	void Draw(GPUProgram& program) {
		Bind();
		if (texture) texture->Bind(0);
		program.setUniform(0, "textureUnit");
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
	~FullScreenTexturedQuad() { delete texture; }
};

class RaytraceApp : public glApp {
	GPUProgram program;
	FullScreenTexturedQuad* quad;
public:
	RaytraceApp() : glApp("Ray tracing") {}

	void onInitialization() override {
		glViewport(0, 0, windowWidth, windowHeight);
		quad = new FullScreenTexturedQuad();
		program.create(vertexSource, fragmentSource);
	}

	void onDisplay() override {
		std::vector<vec3> image(windowWidth * windowHeight);
		quad->LoadTexture(windowWidth, windowHeight, image);
		quad->Draw(program);
	}

	void onTimeElapsed(float startTime, float endTime) override {
		refreshScreen();
	}

	~RaytraceApp() { delete quad; }
};

RaytraceApp app;
