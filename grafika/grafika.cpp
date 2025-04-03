//=============================================================================================
// Harmadik házi feladat: Mercator térkép
//=============================================================================================
#include "framework.h"

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330
	precision highp float;

	layout(location = 0) in vec2 cP;
	layout(location = 1) in vec2 vertexUV;

	out vec2 texCoord;

	uniform mat4 mvp;
	uniform bool needMvp;

	void main() {
		if (needMvp) {
			gl_Position = mvp * vec4(cP.x, cP.y, 0, 1);
		} else {
			gl_Position = vec4(cP.x, cP.y, 0, 1);
		}
		texCoord = vertexUV;

	}
)";

// axialTilt : 	Próbáld meg beszorozni a tengelyt szögét 2-vel.
//				Ne kérdezd miért, nekem is így nézett ki előtte az árnyék, az valamiért megoldotta

// pixel árnyaló
const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform bool point;
	uniform vec3 color;
	uniform sampler2D textureUnit;

	uniform int time;

	in vec2 texCoord;
	out vec4 fragmentColor;

	const float axialTilt = 46.0f;
	const float PI = 3.14159265359;

	void main() {
		if (point) {
			fragmentColor = vec4(color, 1);
			return;
		}

		vec4 texColor = texture(textureUnit, texCoord);
		float latitude = (-texCoord.y * 170.0) - 85.0;
		float longitude = (texCoord.x * 360.0) - 180.0;

	    float declination = axialTilt * cos(2.0 * PI * (time / 24.0));
	    float localHourAngle = (longitude / 180.0) * PI + (time / 12.0) * PI;
	    float sunAltitude = sin(radians(latitude)) * sin(radians(declination)) +
	                        cos(radians(latitude)) * cos(radians(declination)) * cos(localHourAngle);

		if (sunAltitude < 0.0f) {
			texColor.rgb *= 0.5;
		}

		fragmentColor = texColor;
	}
)";

// 85 fok = 1.4835298642 rad
// 85 fok = 0.4722222222 pi rad
const float Mercator = logf(atanf(M_PI / 4.0f + 1.4835298642f * M_PI / 2.0f));
// const float Mercator = logf(atanf(M_PI / 4.0f + 85.0f / 2.0f));

const float getMercator(const float angle) {
	return logf(atanf(M_PI / 4.0f + angle * M_PI / 180 / 2.0f));
}

const vec2 ncdToMerc(const vec2 cords) {
    float x = cords.x * M_PI;
	float y = cords.y * Mercator;
	return vec2(x, y);
}

const vec2 mercToLongLat(const vec2 cords) {
	float longi = cords.x;
	float lati = 2.0f * atan(exp(cords.y)) - M_PI / 2.0f;
	return vec2(longi, lati);
}

const vec3 longlatToDesc(const vec2 cords, const float R) {
	float x = R * cosf(cords.y) * cosf(cords.x);
	float y = R * cosf(cords.y) * sinf(cords.x);
	float z = R * sinf(cords.y);
	return vec3(x, y, z);
}

const vec3 ncdToDesc(const vec2 cords) {
	float longi = cords.x * M_PI;  // Norm. x -> [-π, π]
	float lati = 2.0f * atan(exp(cords.y * M_PI)) - M_PI / 2.0f;  // Mercator y -> [-π/2, π/2]

	float x = cos(lati) * cos(longi);
	float y = cos(lati) * sin(longi);
	float z = sin(lati);

	return vec3(x, y, z);
}

const vec2 descToNcd(const vec3 cords) {
	float lat = asin(cords.z);  // -1 ≤ z ≤ 1 → -π/2 ≤ lat ≤ π/2
	float lon = atan2(cords.y, cords.x);  // [-π, π]

	float x = lon / M_PI;  // Norm. x -> [-1,1]
	float y = log(tan(M_PI / 4.0f + lat / 2.0f)) / M_PI;  // Mercator norm. y

	return vec2(x, y);
}

const float getAngle(const vec3 start, const vec3 end) {
	return acos(dot(start, end));
}

const float getDistance(const float angle) {
	return angle * 6371.0f;
}

std::vector<vec2> interpolate(vec2 startNCD, vec2 endNCD) {
	std::vector<vec2> path;

	// 1. Kezdő- és végpont átalakítása Descartes-koordinátákba
	vec3 start = normalize(ncdToDesc(startNCD));
	vec3 end = normalize(ncdToDesc(endNCD));

	// 2. Szög kiszámítása
	float angle = getAngle(start, end);
	printf("Distance: %.0f km\n", getDistance(angle));

	// 3. Interpoláció és visszaalakítás
	for (int i = 0; i < 100; i++) {
		float t = float(i) / 99.0f;
		float A = sinf((1 - t) * angle) / sinf(angle);
		float B = sinf(t * angle) / sinf(angle);

		vec3 vertex = A * start + B * end;

		// 4. Cartesian -> Longitude/Latitude
		float lat = asinf(vertex.z / 1.02f);
		float lon = atan2f(vertex.y, vertex.x);

		// 5. Longitude/Latitude -> Mercator
		float X = lon;
		float Y = logf(tanf(M_PI / 4.0f + lat / 2.0f));

		// 6. Mercator -> Screen koordináták
		float screen_x = X / M_PI;
		float screen_y = Y / Mercator;

		path.push_back(vec2(screen_x, screen_y));
	}

	return path;
}


const int winWidth = 600, winHeight = 600;

const vec2 scrN = vec2(float(winWidth) / 2.0f, float(winHeight + 200 ) / 2.0f);

const int lineWidth = 3, pointSize = 10;

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
		vec4 vertx = getProjIM() * getViewIM() * vec4(nX, nY, 0, 1);
		return vec2(vertx.x, vertx.y);
	}
};

class Object : public Geometry<vec2> {
protected:
	unsigned int vao, vbo;
	std::vector<vec2> vtx;
public:
	Object() : vao(0), vbo(0) {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(0);
	}

	void update() {
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vec2), &vtx[0], GL_DYNAMIC_DRAW);
	}

	void Draw(GPUProgram* gpuProgram, const int type, const vec3 color, const mat4& mvp) {
		if (vtx.size() > 0)
		{
			glBindVertexArray(vao);
			gpuProgram->setUniform(true, "point");
			gpuProgram->setUniform(true, "needMvp");
			gpuProgram->setUniform(color, "color");
			gpuProgram->setUniform(mvp, "mvp");
			glDrawArrays(type, 0, vtx.size());
			gpuProgram->setUniform(false, "point");
			gpuProgram->setUniform(false, "needMvp");
		}
	}

	~Object() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}
};

class Map {
	std::vector<vec2> vtx;
	unsigned int vao, vbo[2];
	Texture* texture;

public:
	Map() {
		std::vector<unsigned int> mapImg = {
			252, 252, 252, 252, 252, 252, 252, 252, 252, 0, 9, 80, 1, 148, 13, 72, 13, 140, 25, 60, 21, 132, 41, 12, 1, 28, 25, 128, 61, 0, 17, 4, 29, 124, 81, 8, 37, 116, 89, 0, 69, 16, 5, 48, 97, 0, 77, 0, 25, 8, 1, 8, 253, 253, 253, 253, 101, 10, 237, 14, 237, 14, 241, 10, 141, 2, 93, 14, 121, 2, 5, 6, 93, 14, 49, 6, 57, 26, 89, 18, 41, 10, 57, 26, 89, 18, 41, 14, 1, 2, 45, 26, 89, 26, 33, 18, 57, 14, 93, 26, 33, 18, 57, 10, 93, 18, 5, 2, 33, 18, 41, 2, 5, 2, 5, 6, 89, 22, 29, 2, 1, 22, 37, 2, 1, 6, 1, 2, 97, 22, 29, 38, 45, 2, 97, 10, 1, 2, 37, 42, 17, 2, 13, 2, 5, 2, 89, 10, 49, 46, 25, 10, 101, 2, 5, 6, 37, 50, 9, 30, 89, 10, 9, 2, 37, 50, 5, 38, 81, 26, 45, 22, 17, 54, 77, 30, 41, 22, 17, 58, 1, 2, 61, 38, 65, 2, 9, 58, 69, 46, 37, 6, 1, 10, 9, 62, 65, 38, 5, 2, 33, 102, 57, 54, 33, 102, 57, 30, 1, 14, 33, 2, 9, 86, 9, 2, 21, 6, 13, 26, 5, 6, 53, 94, 29, 26, 1, 22, 29, 0, 29, 98, 5, 14, 9, 46, 1, 2, 5, 6, 5, 2, 0, 13, 0, 13, 118, 1, 2, 1, 42, 1, 4, 5, 6, 5, 2, 4, 33, 78, 1, 6, 1, 6, 1, 10, 5, 34, 1, 20, 2, 9, 2, 12, 25, 14, 5, 30, 1, 54, 13, 6, 9, 2, 1, 32, 13, 8, 37, 2, 13, 2, 1, 70, 49, 28, 13, 16, 53, 2, 1, 46, 1, 2, 1, 2, 53, 28, 17, 16, 57, 14, 1, 18, 1, 14, 1, 2, 57, 24, 13, 20, 57, 0, 2, 1, 2, 17, 0, 17, 2, 61, 0, 5, 16, 1, 28, 25, 0, 41, 2, 117, 56, 25, 0, 33, 2, 1, 2, 117, 52, 201, 48, 77, 0, 121, 40, 1, 0, 205, 8, 1, 0, 1, 12, 213, 4, 13, 12, 253, 253, 253, 141
		};

		std::vector<vec3> image = decodeTexture(mapImg, 64, 64);
		
		texture = new Texture(64, 64);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_FLOAT, &image[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(2, &vbo[0]);  // egyszerre két vbo-t kérünk
		// a négyszög csúcsai kezdetben normalizált eszközkoordinátákban
		vtx = { vec2(-1.0f, -1.0f), vec2(1.0f, -1.0f), vec2(1.0f, 1.0f), vec2(-1.0f, 1.0f) };
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]); // GPU-ra másoljuk
		glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vec2), &vtx[0], GL_STATIC_DRAW);
		// updateGPU();               // GPU-ra másoljuk
		glEnableVertexAttribArray(0); // a 0. vbo a 0. bemeneti regisztert táplálja
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL); // csúcsonként 2 float-tal
		// a négyszög csúcsai textúratérben
		std::vector<vec2> uvs = { vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(1.0f, 1.0f), vec2(0.0f, 1.0f) };
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]); // GPU-ra másoljuk
		glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(vec2), &uvs[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(1); // a 1. vbo a 1. bemeneti regisztert táplálja
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL); // csúcsonként 2 float-tal
	}

	std::vector<vec3> decodeTexture(const std::vector<uint32_t>& raw, const int width, const int height) {
		std::vector<vec3> image;

		vec3 colors[4] = {
			vec3(1.0f, 1.0f, 1.0f),  // Fehér
			vec3(0.0f, 0.0f, 1.0f),  // Kék
			vec3(0.0f, 1.0f, 0.0f),  // Zöld
			vec3(0.0f, 0.0f, 0.0f)   // Fekete
		};

		size_t i = 0, pi = 0;
		size_t size = raw.size();
		size_t iSize = width * height;
		while (i < size && pi < iSize) {
			unsigned int byte = raw.at(i++);
			size_t rep = (byte >> 2) + 1;
			unsigned int data = byte & 0x03;
			for (size_t j = 0; j < rep; j++) {
				image.push_back(colors[data]);
			}
		}
		
		return image;
	}

	void Draw(GPUProgram* gpuProgram, Camera* camera) {
		gpuProgram->Use();
		gpuProgram->setUniform(0, "textureUnit");
		texture->Bind(0);
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, vtx.size());
	}

	~Map() {
		delete texture;
	}
};

class Stations : Object {
public:
	Stations() : Object() {}

	void add(const vec2 p) {
		vtx.push_back(p);
		update();
	}

	const size_t size() {
		return vtx.size();
	}

	const std::vector<vec2>& Vtx() {
		return vtx;
	}

	void Draw(GPUProgram* gpuProgram, Camera* camera) {
		mat4 mvp = camera->getProjM() * camera->getViewM();
		Object::Draw(gpuProgram, GL_POINTS, vec3(1.0f, 0.0f, 0.0f), mvp);
	}
};

class Road : Object {
public:
	Road() : Object() {}

	void add(const vec2 p) {
		vtx.push_back(p);
		update();
	}

	void addPath(const std::vector<vec2>& path) {
		for (size_t i = 0; i < path.size(); i++) {
			vtx.push_back(path.at(i) * vec2(300.0f, 30.0f));
		}
		update();
	}

	void Draw(GPUProgram* gpuProgram, Camera* camera) {
		mat4 mvp = camera->getProjM() * camera->getViewM();
		Object::Draw(gpuProgram, GL_LINE_STRIP, vec3(1.0f, 1.0f, 0.0f), mvp);
	}
};

class Merkator : public glApp {
	GPUProgram* gpuProgram;
	Map* map;
	Stations* stations;
	Road* road;
	Camera* camera;
	int currenttime = 0;
public:
	Merkator() : glApp("Merkator térkép") {}

	// Inicializáció, 
	void onInitialization() {
		gpuProgram = new GPUProgram(vertSource, fragSource);
		
		glPointSize(pointSize);
		glLineWidth(lineWidth);

		map = new Map();
		stations = new Stations();
		road = new Road();
		camera = new Camera(vec2(0.0f, 0.0f), winWidth, winHeight);

		// adding the cordinates and the roads based on jporta input
		
		/*stations->add(vec2(52.9999f, 126.0f));
		stations->add(vec2(-195.9999f, 99.9999f));
		road->addPath(interpolate(vec2(0.1767f, 0.315f), vec2(-0.6533, 0.25f)));
		stations->add(vec2(-89.0f, -20.0f));
		road->addPath(interpolate(vec2(-0.6533, 0.25f), vec2(-0.2967f, -0.05f)));
		stations->add(vec2(204.9998f, 94.0f));
		road->addPath(interpolate(vec2(-0.2967f, -0.05f), vec2(0.6833f, 0.235f)));
		stations->add(vec2(53.9998f, 125.0f));
		road->addPath(interpolate(vec2(0.6833f, 0.235f), vec2(0.18f, 0.3125f)));
		*/
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		map->Draw(gpuProgram, camera);
		road->Draw(gpuProgram, camera);
		stations->Draw(gpuProgram, camera);
	}

	void onMousePressed(MouseButton but, int pX, int pY) {
		if (but == MOUSE_LEFT) {
			float nX = 2.0f * pX / winWidth - 1;
			float nY = 1.0f - 2.0f * pY / winHeight;

			vec2 wPos = camera->scrToW(vec2(pX, pY), vec2(winWidth, winHeight));

			stations->add(wPos);
			printf("Ncd: %f, %f\n", wPos.x, wPos.y);

			if (stations->size() >= 2) {
				vec2 llast = stations->Vtx().at(stations->size() - 2);
				vec2 last = stations->Vtx().at(stations->size() - 1);
				road->addPath(interpolate(vec2(llast.x / scrN.x, llast.y / scrN.y), vec2(last.x / scrN.x, last.y / scrN.y)));
				printf("Path: (%f, %f) -> (%f, %f)\n", llast.x / scrN.x, llast.y / scrN.y, last.x / scrN.x, last.y / scrN.y);
			}

			refreshScreen();
		}
	}

	void onKeyboard(int key) {
		if (key == 'n') {
			currenttime = 0 ? currenttime >= 24 : currenttime + 1;
			gpuProgram->setUniform(currenttime, "time");
			refreshScreen();
		}
	}

	~Merkator() {
		delete gpuProgram;
		delete map;
		delete stations;
		delete camera;
	}
};

Merkator app;

