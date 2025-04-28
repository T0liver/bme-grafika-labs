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

// TODO: Az objects változót tuti máshonnan fogja megkani, de egyenlőre maradjon így
Hit firstIntersect(Ray ray, std::vector<Intersectable*>& objects) {  
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
