//=============================================================================================
// Harmadik házi feladat: Mercator térkép
//=============================================================================================
#include "framework.h"

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330
	precision highp float;

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter
	uniform mat4 mvp;		// modelnézettér projekciós transzformáció
	void main() {
		gl_Position = mvp * vec4(cP.x, cP.y, 0, 1); 	// bemenet már normalizált eszközkoordinátákban
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

const int winWidth = 600, winHeight = 600;

// Camera class, mert gondolom kene ilyen is...
class Camera {
	vec2 center;
	float width, height;
public:
	Camera(vec2 center, float width, float height) : center(center), width(width), height(height) {}

	mat4 getViewM() const {
		return translate(vec3(-center.x, -center.y, 0));
	}

	mat4 getProjM() const {
		return scale(vec3(2.0f / width, 2.0f / height, 1.0f));
	}

	mat4 getViewIM() const {
		return translate(vec3(center.x, center.y, 0.0f));
	}

	mat4 getProjIM() const {
		return scale(vec3(width / 2.0f, height / 2.0f, 1.0f));
	}

	vec2 scrToW(const vec2& cords, const vec2& size) const {
		float nX = 2.0f * cords.x / size.x - 1;
		float nY = 1.0f - 2.0f * cords.y / size.y;
		vec4 vertx = getViewIM() * getProjIM() * vec4(nX, nY, 0, 1);
		return vec3(vertx.x, -vertx.y, 0.0f);
	}
};

class Object : public Geometry<vec2> {
protected:
	vec2 position;
public:
	virtual std::vector<vec2> GenVertexData() = 0;

    void update() {
		Vtx() = GenVertexData();
		this->updateGPU();
    }

	void Draw(GPUProgram* gpuProgram, int type, vec3 color, Camera& camera) {
		// mat4 M = translate(vec3(position.x, position.y, 0.0f)) * rotate(phi, vec3(0.0f, 0.0f, 1.0f)) * scale(scaling);
		mat4 M = translate(vec3(position.x, position.y, 0.0f)); // kellenek ezek? ^
		mat4 MVP = camera.getProjM() * camera.getViewM() * M;
		gpuProgram->setUniform(MVP, "MVP");
		__super::Draw(gpuProgram, type, color);
	}
};

class Merkator : public glApp {
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
public:
	Merkator() : glApp("Merkator térkép") {}

	// Inicializáció, 
	void onInitialization() {
		gpuProgram = new GPUProgram(vertSource, fragSource);
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);
	}
};

Merkator app;

