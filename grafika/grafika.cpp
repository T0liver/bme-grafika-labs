#include "framework.h"

const int windowWidth = 600, windowHeight = 600;
const float Epsilon = 0.0001f;

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
	bool out;
	Ray(vec3 _start, vec3 _dir) : start(_start), dir(normalize(_dir)) {}
	Ray(vec3 _start, vec3 _dir, bool _out) : start(_start), out(_out), dir(normalize(_dir)) {}
};


class Intersectable {
protected:
	Material* material;
public:
	virtual Hit intersect(const Ray& ray) = 0;
};

class Sphere : public Intersectable {
	vec3 center;
	float radius;
public:
	Hit intersect(const Ray& ray) override {
		Hit hit;
		vec3 dist = ray.start - center;
		float a = dot(ray.dir, ray.dir);
		float b = dot(dist, ray.dir) * 2.0f;
		float c = dot(dist,dist) - radius * radius;
		float discr = b * b - 4 * a * c;
		if (discr < 0) return hit; else discr = sqrtf(discr);
		float t1 = (-b + discr) / (2.0f * a);
		float t2 = (-b - discr) / (2.0f * a);
		if (t1 <= 0) return hit;
		hit.t = (t2 > 0) ? t2 : t1;
		hit.position = ray.start + ray.dir * hit.t;
		hit.normal = (hit.position - center) / radius;
		hit.material = material;
		return hit;
	}
};

class Plane : public Intersectable {
	vec3 normal, center;
	float size;
public:
	Plane(vec3 _normal, vec3 _center, float _size, Material* _material)
		: normal(vec3(0.0f, 1.0f, 0.0f)), center(_center), size(_size) {
		material = _material;
	}

	Hit intersect(const Ray& ray) override {
		Hit hit;

		if (fabs(dot(ray.dir, normal)) < 1e-6f)
			return hit;

		float t = dot(center - ray.start, normal) / dot(ray.dir, normal);
		if (t < 0)
			return hit;

		vec3 p = ray.start + ray.dir * t;

		float halfSize = size / 2.0f;
		if (p.x < center.x - halfSize || p.x > center.x + halfSize ||
			p.z < center.z - halfSize || p.z > center.z + halfSize)
			return hit;

		hit.t = t;
		hit.position = p;
		hit.normal = normal;
		hit.material = material;
		return hit;
	}
};

class CheckerPlane : public Intersectable {
	vec3 center;
	float size;
	vec3 normal;
	float tileSize;
	Material* matWhite;
	Material* matBlue;
public:
	CheckerPlane(const vec3& _center, float _size, float _tileSize, Material* _white, Material* _blue)
		: center(_center), size(_size), tileSize(_tileSize), normal(vec3(0, 1, 0)),
		matWhite(_white), matBlue(_blue) {
		material = nullptr;
	}

	Hit intersect(const Ray& ray) override {
		Hit hit;

		if (fabs(dot(ray.dir, normal)) < 1e-6f)
			return hit;

		float t = dot(center - ray.start, normal) / dot(ray.dir, normal);
		if (t < 0)
			return hit;

		vec3 p = ray.start + t * ray.dir;

		float halfSize = size / 2.0f;
		if (p.x < center.x - halfSize || p.x > center.x + halfSize ||
			p.z < center.z - halfSize || p.z > center.z + halfSize)
			return hit;

		// Melyik csempére esett?
		float relX = p.x + halfSize;
		float relZ = p.z + halfSize;

		int xi = int(floor(relX / tileSize));
		int zi = int(floor(relZ / tileSize));

		// A (0.5, 0.5, -1) pont fehér -> xi+zi páros akkor fehér
		Material* chosenMat = ((xi + zi) % 2 == 0) ? matWhite : matBlue;

		hit.t = t;
		hit.position = p;
		hit.normal = normal;
		hit.material = chosenMat;
		return hit;
	}
};


class Cylinder : public Intersectable {
	vec3 base, axis;
	float radius, height;
public:
	Cylinder(vec3 _base, vec3 _axis, float _radius, float _height, Material* _material) {
		base = _base;
		axis = normalize(_axis);
		radius = _radius;
		height = _height;
		material = _material;
	}

	Hit intersect(const Ray& ray) override {
		Hit hit;

		vec3 d = ray.dir;
		vec3 m = ray.start - base;
		vec3 n = axis;

		float md = dot(m, d);
		float nd = dot(n, d);
		float mn = dot(m, n);
		float dd = dot(d, d);
		float nn = dot(n, n);
		float a = dd - nd * nd;
		float k = dot(m, m) - radius * radius;
		float c = k - mn * mn;
		if (abs(a) < 1e-6f) return hit;

		float b = dd * mn - nd * md;
		float discr = b * b - a * c;
		if (discr < 0) return hit;

		float t = (-b - sqrt(discr)) / a;
		if (t < 0) return hit;

		vec3 p = ray.start + t * d;
		float h = dot(p - base, n);
		if (h < 0 || h > height) return hit;
		
		hit.t = t;
		hit.position = p;
		vec3 outward_normal = normalize((p - base) - n * h);
		hit.normal = outward_normal;
		hit.material = material;
		return hit;
	}
};

class Cone : Intersectable {
	vec3 base, axis;
	float angle, height;
public:
	Cone(vec3 _base, vec3 _axis, float _angle, float _height, Material* _material) {
		base = _base;
		axis = normalize(_axis);
		angle = _angle;
		height = _height;
		material = _material;
	}

	Hit intersect(const Ray& ray) override {
		Hit hit;

		vec3 v = ray.dir;
		vec3 co = ray.start - base;
		float cosTheta = cos(angle);
		float cosTheta2 = cosTheta * cosTheta;

		float va = dot(v, axis);
		float co_a = dot(co, axis);
		float A = va * va - cosTheta2;
		float B = 2 * (va * co_a - dot(v, co) * cosTheta2);
		float C = co_a * co_a - dot(co, co) * cosTheta2;
		float discr = B * B - 4 * A * C;
		if (discr < 0) return hit;

		float sqrtDiscr = sqrt(discr);
		float t1 = (-B - sqrtDiscr) / (2 * A);
		float t2 = (-B + sqrtDiscr) / (2 * A);
		float t = (t1 > 0) ? t1 : (t2 > 0) ? t2 : -1;
		if (t < 0) return hit;

		vec3 p = ray.start + t * ray.dir; // Metszéspont
		vec3 baseToP = p - base;
		float heightAlongAxis = dot(baseToP, axis);
		if (heightAlongAxis < 0 || heightAlongAxis > height)
			return hit;

		vec3 n = normalize(
			baseToP - axis * (length(baseToP) / cosTheta)
		);

		hit.t = t;
		hit.position = p;
		hit.normal = n;
		hit.material = material;
		return hit;
	}
};

class Camera {
	vec3 eye, lookat, right, up;
	float fov;
public:
	void set(vec3 _eye, vec3 _lookat, vec3 vup, float _fov) {
		eye = _eye; lookat = _lookat; fov = _fov;
		vec3 w = eye - lookat;
		float windowSize = length(w) * tanf(fov / 2);
		right = normalize(cross(vup, w)) * (float)windowSize * (float)windowWidth / (float)windowHeight;
		up = normalize(cross(w, right)) * windowSize;
	}

	Ray getRay(int X, int Y) {
		vec3 dir = lookat + right * (2 * (X + 0.5f) / windowWidth - 1) + up * (2 * (Y + 0.5f) / windowHeight - 1) - eye;
		return Ray(eye, dir);
	}

	void Animate(float dt) {
		vec3 d = eye - lookat;
		eye = vec3(d.x * cos(dt) + d.z * sin(dt), d.y, -d.x * sin(dt) + d.z * cos(dt)) + lookat;
		set(eye, lookat, up, fov);
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
		texture = new Texture(width, height, image);
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
	std::vector<Intersectable*> objects;
	Camera* camera;
	const vec3 La = vec3(0.4f, 0.4f, 0.4f);
public:
	void add(Intersectable* obj) {
		objects.push_back(obj);
		printf("added obj %p\n", obj);
	}

	void addCam(Camera* cam) {
		camera = cam;
	}

	vec3 trace(const Ray& ray) {
		Hit bestHit = firstIntersect(ray);
		if (bestHit.t < 0) return vec3(0.0f, 0.0f, 0.0f);

		return bestHit.material->ka * La;
	}

	void render(std::vector<vec3>& image) {
		image.resize(windowWidth * windowHeight);

		for (int Y = 0; Y < windowHeight; Y++) {
			for (int X = 0; X < windowWidth; X++) {
				vec3 color = trace(camera->getRay(X, Y));
				image[Y * windowWidth + X] = vec3(color.x, color.y, color.z);
			}
		}
	}

	Hit firstIntersect(Ray ray) {
		Hit bestHit;
		for (Intersectable* obj : objects) {
			Hit hit = obj->intersect(ray);
			if (hit.t > 0 && (bestHit.t < 0 || hit.t < bestHit.t)) {
				bestHit = hit;
			}
		}
		if (dot(ray.dir, bestHit.normal) > 0) {
			bestHit.normal *= -1;
		}
		return bestHit;
	}
};

class RaytraceApp : public glApp {
	GPUProgram* program;
	Scene* scene;
	Camera* camera;
	FullScreenTexturedQuad* quad;
	std::vector<vec3> image;
public:
	RaytraceApp() : glApp("Ray tracing") {}

	void onInitialization() override {
		glViewport(0, 0, windowWidth, windowHeight);
		quad = new FullScreenTexturedQuad();
		program = new GPUProgram(vertexSource, fragmentSource);
		scene = new Scene();
		camera = new Camera();

		vec3 eye = vec3(0, 0, 2), vup = vec3(0, 1, 0), lookat = vec3(0, 0, 0);
		float fov = 45 * (float)M_PI / 180;
		camera->set(eye, lookat, vup, fov);

		Material* white = new Material(vec3(0.3f, 0.3f, 0.3f), vec3(0.0f), 0.0f); // fehér
		Material* blue = new Material(vec3(0.0f, 0.1f, 0.3f), vec3(0.0f), 0.0f); // kék

		scene->addCam(camera);
		scene->add(new CheckerPlane(vec3(0, -1, 0), 20.0f, 1.0f, white, blue));

		scene->render(image);
	}

	void onDisplay() override {
		scene->render(image);
		quad->LoadTexture(windowWidth, windowHeight, image);
		quad->Draw(program);
	}

	void onTimeElapsed(float startTime, float endTime) override {
		refreshScreen();
	}

	~RaytraceApp() {
		delete quad;
		delete program;
		delete scene;
		delete camera;
	}
};

RaytraceApp app;
