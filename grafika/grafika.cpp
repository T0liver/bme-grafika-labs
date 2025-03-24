//=============================================================================================
// Hullámvasút
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
const int pointSize = 10, lineSize = 3;

class Camera {
	vec3 center;
	float width, height;
public:
	Camera(vec3 center, float width, float height) : center(center), width(width), height(height) {}

	mat4 getViewM() const {
		return lookAt(center + vec3(0, 0, -1), center, vec3(0, 1, 0));
	}

	mat4 getProjM() const {
		return ortho(
			center.x - width / 2, center.x + width / 2,
			center.y - height / 2, center.y + height / 2,
			0.1f, 100.0f);
	}

	mat4 getViewIM() const {
		return inverse(getViewM());
	}

	mat4 getProjIM() const {
		return inverse(getProjM());
	}

	vec3 scrToW(const vec2& cords, const vec2& size) const {
		vec4 viewport = vec4(0, 0, size.x, size.y);
		vec3 win = vec3(cords, 1.0f);
		// mat4 viewProjI = getProjIM() * getViewIM();
		return unProject(win, getViewM(), getViewM(), viewport);
	}
};

class Spline {
	std::vector<vec3> ctrlPoints;
	std::vector<vec3> crvPoints;
	unsigned int vao, vbo;

public:
	Spline() {
		glGenVertexArrays(1, &vao);	
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}
	
	~Spline() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}

	void addCtrlPoint(const vec3& point) {
		ctrlPoints.push_back(point);
		calcCrvPoints();
		updateGPU();
	}

	void calcCrvPoints() {
		crvPoints.clear();
		const int nrSegs = 100;
		for (size_t i = 1; i + 2 < ctrlPoints.size(); i++) {
			for (int j = 0; j <= nrSegs; j++) {
				float t = (float)j / nrSegs;
				vec3 p = catmullRom(ctrlPoints[i - 1], ctrlPoints[i], ctrlPoints[i + 1], ctrlPoints[i + 2], t);
				crvPoints.push_back(p);
			}
		}
	}

	vec3 catmullRom(const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float t) {
		float t2 = t * t;
		float t3 = t2 * t;

		vec3 ret = 0.5f * ((2.0f * p1) +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);

		return ret;
	}

	void updateGPU() {
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, crvPoints.size() * sizeof(vec3), &crvPoints[0], GL_DYNAMIC_DRAW);
	}

	void Draw(GPUProgram* prog) {
		prog->Use();

		glBindVertexArray(vao);
		prog->setUniform(vec3(1.0f, 1.0f, 0.0f), "color");
		glDrawArrays(GL_LINE_STRIP, 0, crvPoints.size());

		glPointSize(10);
		prog->setUniform(vec3(1.0f, 0.0f, 0.0f), "color");
		glDrawArrays(GL_POINTS, 0, crvPoints.size());
	}
};


class Hullamvasut : public glApp {
	Geometry<vec2>* triangle;  // geometria
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
public:
	Hullamvasut() : glApp("Hullamvasut") {}

	// Inicializáció
	void onInitialization() {
		glViewport(0, 0, winWidth, winHeight);

		glPointSize(pointSize);
		glLineWidth(lineSize);

		gpuProgram = new GPUProgram(vertSource, fragSource);
	}

	void onMousePressed(MouseButton btn, int pX, int pY) {
		if (btn == MOUSE_LEFT) {

		}
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
	}

	~Hullamvasut() {
		delete gpuProgram;
	}
};

Hullamvasut app;

