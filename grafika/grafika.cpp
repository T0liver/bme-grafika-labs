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

enum GState {
	WAIT,
	START,
	FALLEN
};

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
		return unProject(win, getViewM(), getProjM(), viewport);
	}
};

class Spline {
	std::vector<vec3> ctrlPoints;
	std::vector<vec3> crvPoints;
	unsigned int vaoCurve, vboCurve;
	unsigned int vaoCtrl, vboCtrl;

	vec3 hermite(const glm::vec3& p0, const glm::vec3& v0, const glm::vec3& p1, const glm::vec3& v1, float t) {
		float t2 = t * t;
		float t3 = t2 * t;

		glm::vec3 result = (2.0f * t3 - 3.0f * t2 + 1.0f) * p0 +
			(t3 - 2.0f * t2 + t) * v0 +
			(-2.0f * t3 + 3.0f * t2) * p1 +
			(t3 - t2) * v1;
		return result;
	}

public:
	Spline() {
		// Curve VAO and VBO
		glGenVertexArrays(1, &vaoCurve);
		glBindVertexArray(vaoCurve);
		glGenBuffers(1, &vboCurve);
		glBindBuffer(GL_ARRAY_BUFFER, vboCurve);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		// Control points VAO and VBO
		glGenVertexArrays(1, &vaoCtrl);
		glBindVertexArray(vaoCtrl);
		glGenBuffers(1, &vboCtrl);
		glBindBuffer(GL_ARRAY_BUFFER, vboCtrl);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	~Spline() {
		glDeleteBuffers(1, &vboCurve);
		glDeleteVertexArrays(1, &vaoCurve);
		glDeleteBuffers(1, &vboCtrl);
		glDeleteVertexArrays(1, &vaoCtrl);
	}

	void addCtrlPoint(const vec3& point) {
		ctrlPoints.push_back(point);
		if (ctrlPoints.size() > 1) {
			calcCrvPoints();
		}
		updateGPU();
	}

	void calcCrvPoints() {
		crvPoints.clear();
		const int nrSegs = 100;

		for (size_t i = 0; i < ctrlPoints.size() - 1; i++) {
			vec3 v0, v1;
			if (i == 0) {
				vec3 cr = ctrlPoints[i + 1] - ctrlPoints[i];
				vec3 nx;
				if (ctrlPoints.size() < 3) {
					nx = ctrlPoints[i + 1] - ctrlPoints[i];
				}
				else {
					nx = ctrlPoints[i + 2] - ctrlPoints[i + 1];
				}
				v0 = cr * 0.5f;
				v1 = (cr + nx) * 0.5f;
			}
			else if (i == ctrlPoints.size() - 2) {
				vec3 pr = ctrlPoints[i] - ctrlPoints[i - 1];
				vec3 cr = ctrlPoints[i + 1] - ctrlPoints[i];
				v0 = (pr + cr) * 0.5f;
				v1 = cr * 0.5f;
			}
			else {
				vec3 pr = ctrlPoints[i] - ctrlPoints[i - 1];
				vec3 cr = ctrlPoints[i + 1] - ctrlPoints[i];
				vec3 nx = ctrlPoints[i + 2] - ctrlPoints[i + 1];
				v0 = (pr + cr) * 0.5f;
				v1 = (cr + nx) * 0.5f;
			}

			for (int j = 0; j <= nrSegs; j++) {
				float t = (float)j / nrSegs;
				vec3 p = hermite(ctrlPoints[i], v0, ctrlPoints[i + 1], v1, t);
				crvPoints.push_back(p);
			}
		}
	}

	void updateGPU() {
		glBindBuffer(GL_ARRAY_BUFFER, vboCurve);
		glBufferData(GL_ARRAY_BUFFER, crvPoints.size() * sizeof(vec3), crvPoints.data(), GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, vboCtrl);
		glBufferData(GL_ARRAY_BUFFER, ctrlPoints.size() * sizeof(vec3), ctrlPoints.data(), GL_DYNAMIC_DRAW);
	}

	void Draw(GPUProgram* prog) {
		prog->Use();

		glBindVertexArray(vaoCurve);
		prog->setUniform(vec3(1.0f, 1.0f, 0.0f), "color");
		glDrawArrays(GL_LINE_STRIP, 0, crvPoints.size());

		glBindVertexArray(vaoCtrl);
		prog->setUniform(vec3(1.0f, 0.0f, 0.0f), "color");
		glDrawArrays(GL_POINTS, 0, ctrlPoints.size());
	}
};

class Gondola {
	vec3 position;
	float angle;
	float speed;
	GState state;
	Spline* spline;
	unsigned int vao, vboCirc, vboSpks;
	std::vector<vec3> circVertx, spksVertx;

	void createCircle() {
		const int nrSegs = 100;
		const float radius = 1.0f;
		for (int i = 0; i <= nrSegs; ++i) {
			float theta = 2.0f * M_PI * float(i) / float(nrSegs);
			float x = radius * cosf(theta);
			float y = radius * sinf(theta);
			circVertx.push_back(vec3(x, y, 0.0f));
		}
	}

	void createSpokes() {
		const int nrSpks = 8;
		const float radius = 1.0f;
		for (int i = 0; i < nrSpks; ++i) {
			float theta = 2.0f * M_PI * float(i) / float(nrSpks);
			float x = radius * cosf(theta);
			float y = radius * sinf(theta);
			spksVertx.push_back(vec3(0.0f, 0.0f, 0.0f));
			spksVertx.push_back(vec3(x, y, 0.0f));
		}
	}
public:
	Gondola(Spline* spline) : spline(spline), state(WAIT), speed(0.0f), angle(0.0f) {
		createCircle();
		createSpokes();

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vboCirc);
		glBindBuffer(GL_ARRAY_BUFFER, vboCirc);
		glBufferData(GL_ARRAY_BUFFER, circVertx.size() * sizeof(vec3), circVertx.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		glGenBuffers(1, &vboSpks);
		glBindBuffer(GL_ARRAY_BUFFER, vboSpks);
		glBufferData(GL_ARRAY_BUFFER, spksVertx.size() * sizeof(vec3), spksVertx.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	~Gondola() {
		glDeleteBuffers(1, &vboCirc);
		glDeleteBuffers(1, &vboSpks);
		glDeleteVertexArrays(1, &vao);
	}

	void Start() {
		state = START;
		speed = 0.0f;
	}

	void Animate(float dt) {
		if (state == START) {
			// Simple physics animation
			speed += 9.81f * dt; // Gravity
			position += vec3(speed * dt, 0.0f, 0.0f); // Update position
			angle += speed * dt; // Update angle

			// Check if fallen
			if (position.y < -10.0f) {
				state = FALLEN;
			}
		}
	}

	void Draw(GPUProgram* prog, const mat4& mvp) {
		prog->Use();
		mat4 model = translate(mat4(1.0f), position) * rotate(mat4(1.0f), angle, vec3(0.0f, 0.0f, 1.0f));
		mat4 mvpMatrix = mvp * model;
		prog->setUniform(mvpMatrix, "mvp");

		// Draw circle
		prog->setUniform(vec3(0.0f, 0.0f, 1.0f), "color"); // Blue fill
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vboCirc);
		glDrawArrays(GL_TRIANGLE_FAN, 0, circVertx.size());

		// Draw spokes
		prog->setUniform(vec3(1.0f, 1.0f, 1.0f), "color"); // White spokes
		glBindBuffer(GL_ARRAY_BUFFER, vboSpks);
		glDrawArrays(GL_LINES, 0, spksVertx.size());
	}

};

class Hullamvasut : public glApp {
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
	Camera* camera;
	Spline* spline;
public:
	Hullamvasut() : glApp("Hullamvasut") {}

	// Inicializáció
	void onInitialization() {
		gpuProgram = new GPUProgram(vertSource, fragSource);
		camera = new Camera(vec3(0.0f, 0.0f, 0.0f), 20.0f, 20.0f);
		spline = new Spline();
		glLineWidth(lineSize);
		glPointSize(pointSize);
	}

	void onMousePressed(MouseButton btn, int pX, int pY) {
		if (btn == MOUSE_LEFT) {
			vec2 scrCords = vec2(pX, winHeight - pY);
			vec2 scrSize = vec2(winWidth, winHeight);
			vec3 worldCords = camera->scrToW(scrCords, scrSize);
			spline->addCtrlPoint(worldCords);
			refreshScreen();
		}
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);
		mat4 viewProj = camera->getProjM() * camera->getViewM();
		gpuProgram->setUniform(viewProj, "mvp");

		spline->Draw(gpuProgram);
	}

	~Hullamvasut() {
		delete gpuProgram;
	}
};

Hullamvasut app;