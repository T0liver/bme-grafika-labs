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

	vec3 scrToW(const vec2& cords, const vec2& size) const {
		float nX = 2.0f * cords.x / size.x - 1;
		float nY = 1.0f - 2.0f * cords.y / size.y;
		vec4 vertx = getViewIM() * getProjIM() * vec4(nX, nY, 0, 1);
		return vec3(vertx.x, -vertx.y, 0.0f);
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

	vec3 hermiteDer1(const glm::vec3& p0, const glm::vec3& v0, const glm::vec3& p1, const glm::vec3& v1, float t) {
		float t2 = t * t;

		glm::vec3 result = (6.0f * t2 - 6.0f * t) * p0 +
			(3.0f * t2 - 4.0f * t + 1.0f) * v0 +
			(-6.0f * t2 + 6.0f * t) * p1 +
			(3.0f * t2 - 2.0f * t) * v1;
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

	vec3 getPoint(float t) const {
		if (crvPoints.empty()) { return vec3(0.0f); }
		int i = static_cast<int>(t * (crvPoints.size() - 1));
		return crvPoints[i];
	}

	vec3 getTangent(float t) const {
		if (crvPoints.empty()) { return vec3(0.0f); }
		if (fabs(t) >= 1.0f) {
			return vec3(0.0f);
		}
		int i = static_cast<int>(t * (crvPoints.size() - 1));
		if (i == 0) {
			return normalize(crvPoints[i + 1] - crvPoints[i]);
		}
		else if (i == crvPoints.size() - 1) {
			return normalize(crvPoints[i] - crvPoints[i - 1]);
		}
		else {
			return normalize(crvPoints[i + 1] - crvPoints[i - 1]);
		}
	}

	bool getalive() {
		if (ctrlPoints.size() < 2) {
			return false;
		}
		else {
			return true;
		}
	}

	float getCurvature(float t) const {
		/*if (crvPoints.empty()) { return 0.0f; }
		int i = static_cast<int>(t * (crvPoints.size() - 1));
		vec3 tangent = getTangent(t);
		vec3 nextTangent;
		if (i == 0) {
			nextTangent = getTangent((i + 1) / float(crvPoints.size() - 1));
		}
		else if (i == crvPoints.size() - 1) {
			nextTangent = getTangent((i - 1) / float(crvPoints.size() - 1));
		}
		else {
			nextTangent = getTangent((i + 1) / float(crvPoints.size() - 1));
		}
		vec3 dTangent = nextTangent - tangent;
		if (i == crvPoints.size() - 1) {
			return length(dTangent) / length(crvPoints[i] - crvPoints[i - 1]);
		} else if (i == 0) {
			return length(dTangent) / length(crvPoints[i + 1] - crvPoints[i]);
		}
		return length(dTangent) / length(crvPoints[i + 1] - crvPoints[i - 1]);*/
		if (crvPoints.empty()) return 0.0f;
		int i = static_cast<int>(t * (crvPoints.size() - 1));
		if (i < 1 || i >= crvPoints.size() - 1) return 0.0f;

		vec3 p0 = crvPoints[i - 1];
		vec3 p1 = crvPoints[i];
		vec3 p2 = crvPoints[i + 1];

		vec3 t0 = p1 - p0;
		vec3 t1 = p2 - p1;

		vec3 ddt = t1 - t0;
		float len = length(t1) + length(t0);
		if (len == 0.0f) return 0.0f;

		return length(ddt) / len;
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
	float angle,
		speed,
		t;
	float mass = 1.0f,
		radius = 1.0f,
		gravity = 40.0f;
	float time;
	Spline* spline;
	GState state;
	unsigned int vaoCirc, vaoSpks, vboCirc, vboSpks;
	std::vector<vec3> circVertx, spksVertx;

	void createCircle() {
		const int nrSegs = 100;
		for (int i = 0; i <= nrSegs; ++i) {
			float theta = 2.0f * M_PI * float(i) / float(nrSegs);
			float x = radius * cosf(theta);
			float y = radius * sinf(theta);
			circVertx.push_back(vec3(x, y, 1.0f));
		}
	}

	void createSpokes() {
		const int nrSpks = 4;
		for (int i = 0; i < nrSpks; ++i) {
			float theta = 2.0f * M_PI * float(i) / float(nrSpks);
			float x = radius * cosf(theta);
			float y = radius * sinf(theta);
			spksVertx.push_back(vec3(0.0f, 0.0f, 0.0f));
			spksVertx.push_back(vec3(x, y, 0.0f));
		}
	}

	float getVecLength(vec3 v) const {
		return sqrtf(v.x * v.x + v.y * v.y);
	}
public:

	Gondola(Spline* spline) : spline(spline), state(WAIT), speed(0.0f), angle(0.0f), t(0.01f) {
		createCircle();
		createSpokes();

		glGenVertexArrays(1, &vaoCirc);
		glBindVertexArray(vaoCirc);


		glGenBuffers(1, &vboCirc);
		glBindBuffer(GL_ARRAY_BUFFER, vboCirc);
		glBufferData(GL_ARRAY_BUFFER, circVertx.size() * sizeof(vec3), circVertx.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		glGenVertexArrays(1, &vaoSpks);
		glBindVertexArray(vaoSpks);

		glGenBuffers(1, &vboSpks);
		glBindBuffer(GL_ARRAY_BUFFER, vboSpks);
		glBufferData(GL_ARRAY_BUFFER, spksVertx.size() * sizeof(vec3), spksVertx.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	~Gondola() {
		glDeleteBuffers(1, &vboCirc);
		glDeleteBuffers(1, &vboSpks);
		glDeleteVertexArrays(1, &vaoCirc);
		glDeleteVertexArrays(1, &vaoSpks);
	}

	void Start() {
		state = START;
		speed = 0.0f;
		t = 0.01f;
		position = spline->getPoint(t);
	}

	void Animate(float dt) {
		if (state == START) {
			if (isnan(speed)) {
				state = FALLEN;
				time = 0.0f;
				return;
			}

			position = spline->getPoint(t);
			printf("dt: %f; t: %f; position: %f, %f", dt, t, position.x, position.y);

			float y0 = spline->getPoint(0.0f).y;
			float y = position.y;
			if (y0 < y) {
				t = 0.01f;
				speed = 0.0f;
				dt = 0.0f;
				return;
			}
			speed += sqrtf(2 * gravity * (y0 - y)) * 0.0006f;

			printf(", v: %f", speed);

			t += speed * dt;

			if (fabs(t) >= 1.0f) {
				state = FALLEN;
				printf("oh\n");
				time = dt;
				return;
			}

			vec3 tangent = spline->getTangent(t);
			tangent = normalize(tangent);

			vec3 normal = vec3(-tangent.y, tangent.x, 0.0f);
			normal = normalize(normal);

			float curvature = spline->getCurvature(t);

			// float centripetalAccel = speed * speed * curvature;
			float centripetalAccel = speed * speed * curvature;

			// float normalForce = mass * (gravity * normal.y + centripetalAccel);
			// float normalForce = gravity * cosf(centripetalAccel) -  speed / radius;
			float normalForce = mass * (gravity * normal.y + centripetalAccel);

			printf(", nf: %f", normalForce);

			if (normalForce < 0.0f) {
				printf("\n");
				state = FALLEN;
				time = 0.0f;
				return;
			}

			vec3 acceleration = vec3(0.0f, -gravity, 0.0f) + normal * (normalForce / mass);

			// SZÖG
			// angle += - (speed / radius) * (1.0f / acceleration.length()) * 0.001f;
			angle += -(speed / radius) * dt;
			printf(", angle: %f\n", angle);

			speed += acceleration.y * dt * dt;

		}
		else if (state == FALLEN) {
			position.y -= 9.81f * time * time;
			time += 0.005f;
			printf("Leestem!, pos: %f, %f, t: %f\n", position.x, position.y, t);
		}
		else if (state == WAIT) {
			printf("So am I still waiting\n");
		}
	}

	void Draw(GPUProgram* prog, const mat4& mvp) {
		if (state == WAIT) { return; }
		prog->Use();
		vec3 tangent = spline->getTangent(t);
		vec3 T = tangent / getVecLength(tangent);
		vec3 normal = vec3(-T.y, T.x, 0.0f);
		mat4 model = translate(position + normal + 0.08f) * rotate(angle, vec3(0.0f, 0.0f, 1.0f));
		mat4 mvpMatrix = mvp * model;
		prog->setUniform(mvpMatrix, "mvp");

		// Draw circle
		prog->setUniform(vec3(0.0f, 0.0f, 1.0f), "color"); // Blue fill
		glBindVertexArray(vaoCirc);
		glDrawArrays(GL_TRIANGLE_FAN, 0, circVertx.size());

		prog->setUniform(vec3(1.0f, 1.0f, 1.0f), "color"); // White fill
		glBindVertexArray(vaoCirc);
		glDrawArrays(GL_LINE_STRIP, 0, circVertx.size());

		// Draw spokes
		prog->setUniform(vec3(1.0f, 1.0f, 1.0f), "color"); // White spokes
		glBindVertexArray(vaoSpks);
		glDrawArrays(GL_LINES, 0, spksVertx.size());
	}

};

class Hullamvasut : public glApp {
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
	Camera* camera;
	Spline* spline;
	Gondola* gondola;
public:
	Hullamvasut() : glApp("Hullamvasut") {}

	// Inicializáció
	void onInitialization() {
		gpuProgram = new GPUProgram(vertSource, fragSource);
		camera = new Camera(vec3(0.0f, 0.0f, 0.0f), 20.0f, 20.0f);
		spline = new Spline();
		gondola = new Gondola(spline);
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

	void onKeyboard(int key) {
		printf("Key: %d\n", key);
		if (key == 32) {
			if (spline->getalive()) {
				gondola->Start();
			}
		}
	}

	/*void onTimeElapsed(float startTime, float endTime) {
		printf("c\n");
		static float tend = 0;
		const float dt = 0.01f; // dt is "infinitesimal"
		float tstart = tend;
		tend = (endTime - startTime) / 1000.0f;
		for (float t = tstart; t < tend; t += dt) {
			float Dt = fmin(dt, tend - t);
			gondola->Animate(Dt);
		}
		refreshScreen();
	}*/

	void onTimeElapsed(float tstart, float tend) {
		const float dt = 0.01; // dt is ”infinitesimal”
		for (float t = tstart; t < tend; t += dt) {
			float Dt = fmin(dt, tend - t);
			gondola->Animate(Dt);
		}
		refreshScreen();
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);
		mat4 viewProj = camera->getProjM() * camera->getViewM();
		gpuProgram->setUniform(viewProj, "mvp");

		spline->Draw(gpuProgram);
		gondola->Draw(gpuProgram, viewProj);
	}

	~Hullamvasut() {
		delete gpuProgram;
		delete camera;
		delete spline;
		delete gondola;
	}
};

Hullamvasut app;