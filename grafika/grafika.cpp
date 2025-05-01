#include "framework.h"

const int windowWidth = 600, windowHeight = 600;
const float Epsilon = 0.0001f;
const int maxdepth = 5;
const vec3 bgColor(0.4f, 0.4f, 0.4f);

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

template<typename T>
T clampp(const T& value, const T& min, const T& max) {
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

template<typename genType> genType maxx(genType x, genType y)
{
	return (x < y) ? y : x;
}

vec3 reflect(const vec3& I, const vec3& N) {
	return I - 2.0f * dot(I, N) * N;
}

vec3 refract(const vec3& I, const vec3& N, float eta) {
	float cosi = clampp(dot(-I, N), -1.0f, 1.0f);
	float etai = 1.0f, etat = eta;
	vec3 n = N;

	if (cosi < 0) {
		cosi = -cosi;
		std::swap(etai, etat);
		n = -N;
	}

	float etaRatio = etai / etat;
	float k = 1.0f - etaRatio * etaRatio * (1.0f - cosi * cosi);
	if (k < 0.0f)
		return vec3(0.0f);
	else
		return etaRatio * I + (etaRatio * cosi - sqrtf(k)) * n;
}


enum MaterialType
{
	isRough,
	isReflective,
	isRefractive
};

struct Material {
	vec3 ka, kd, ks;
	vec3 n, k;
	float shininess;
	MaterialType type;
	Material() {}
	Material(vec3 _kd, vec3 _ks, float _shininess, vec3 _n = vec3(0.0f), vec3 _k = vec3(0.0f), MaterialType _type = isRough)
		: ka(_kd* (float)M_PI), kd(_kd), ks(_ks), n(_n), k(_k), shininess(_shininess), type(_type) {}
	~Material() {}

	vec3 fresnelReflectance(const float cosTheta, const vec3 F0) const {
		switch (type)
		{
		case isRough:
		{
			float clampedCosTheta = clampp(cosTheta, 0.0f, 1.0f);
			return F0 + (vec3(1.0f) - F0) * pow(1.0f - clampedCosTheta, 5.0f);
			break;
		}
		case isReflective:
		{
			float cos2 = cosTheta * cosTheta;
			vec3 n2k2 = n * n + k * k;

			vec3 twoNCos = 2.0f * n * cosTheta;

			vec3 tmpD = n2k2 * cos2 - twoNCos + 1.0f;
			vec3 tmpU = n2k2 * cos2 + twoNCos + 1.0f;
			vec3 rParallel2 = vec3(tmpD.x / tmpU.x, tmpD.y / tmpU.y, tmpD.z / tmpU.z);

			vec3 tmp2D = n2k2 - twoNCos + cos2;
			vec3 tmp2U = n2k2 + twoNCos + cos2;
			vec3 rPerp2 = vec3(tmp2D.x / tmp2U.x, tmp2D.y / tmp2U.y, tmp2D.z / tmp2U.z);

			return 0.5f * (rParallel2 + rPerp2);
			break;
		}
		case isRefractive:
		{
			vec3 eta = n;
			vec3 kappa = k;

			float clampedCosTheta = clampp(cosTheta, 0.0f, 1.0f);
			float cos2 = clampedCosTheta * clampedCosTheta;

			vec3 eta2 = eta * eta;
			vec3 kappa2 = kappa * kappa;

			vec3 twoEtaCos = 2.0f * eta * clampedCosTheta;

			vec3 tmpD = (eta2 + kappa2) * cos2 - twoEtaCos + 1.0f;
			vec3 tmpU = (eta2 + kappa2) * cos2 + twoEtaCos + 1.0f;
			vec3 rParallel2 = vec3(tmpD.x / tmpU.x, tmpD.y / tmpU.y, tmpD.z / tmpU.z);

			vec3 tmp2D = (eta2 + kappa2) - twoEtaCos + cos2;
			vec3 tmp2U = (eta2 + kappa2) + twoEtaCos + cos2;
			vec3 rPerp2 = vec3(tmp2D.x / tmp2U.x, tmp2D.y / tmp2U.y, tmp2D.z / tmp2U.z);

			return 0.5f * (rParallel2 + rPerp2);
			break;
		}
		default:
			return vec3(0.0f);
			break;
		}
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
	Sphere(vec3 _center, float _radius, Material* _material) : center(_center), radius(_radius) {
		material = _material;
	}
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
		: center(_center), size(_size), normal(vec3(0, 1, 0)), tileSize(_tileSize),
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

		float relX = p.x + halfSize;
		float relZ = p.z + halfSize;

		int xi = int(floor(relX / tileSize));
		int zi = int(floor(relZ / tileSize));

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

		// JPorta: warning: unused variable ‘md’, 'mn', 'dd', 'nn', 'k' [-Wunused-variable]
		float md = dot(m, d);
		float nd = dot(n, d);
		float mn = dot(m, n);
		float dd = dot(d, d);
		float nn = dot(n, n);
		float a = dot(d - nd * n, d - nd * n);
		float k = dot(m, m) - radius * radius;
		vec3 z = m - dot(m, n) * n;
		float c = dot(z, z) - radius * radius;
		if (fabs(a) < 1e-6f) return hit;

		float b = 2.0f * dot(d - nd * n, z);
		float discr = b * b - 4.0f * a * c;
		if (discr < 0) return hit;

		float t = (-b - sqrtf(discr)) / (2.0f * a);
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

class Cone : public Intersectable {
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
		float B = 2.0f * (va * co_a - dot(v, co) * cosTheta2);
		float C = co_a * co_a - dot(co, co) * cosTheta2;

		float discr = B * B - 4.0f * A * C;
		if (discr >= 0.0f) {
			float sqrtDiscr = sqrt(discr);
			float t1 = (-B - sqrtDiscr) / (2.0f * A);
			float t2 = (-B + sqrtDiscr) / (2.0f * A);

			float tCone;
			if (t1 > 0 && t2 <= 0) {
				tCone = t1;
			} else if (t2 > 0.0f && t1 <= 0.0f) {
				tCone = t2;
			} else if (t1 > 0.0f && t2 > 0.0f) {
				tCone = fmin(t1, t2);
			} else {
				tCone = -1.0f;
			}
			if (tCone > 0.0f) {
				vec3 p = ray.start + tCone * ray.dir;
				vec3 apexToP = p - base;
				float heightAlongAxis = dot(apexToP, axis);
				if (heightAlongAxis >= 0.0f && heightAlongAxis <= height) {
					vec3 normalDir = normalize(
						apexToP - axis * (length(apexToP) / cosTheta)
					);

					hit.t = tCone;
					hit.position = p;
					hit.normal = normalDir;
					hit.material = material;
				}
			}
		}

		vec3 baseCenter = base + axis * height;
		float radius = height * tan(angle);
		float denom = dot(ray.dir, axis);

		if (abs(denom) > 0.0f) {
			float tCap = dot(baseCenter - ray.start, axis) / denom;
			if (tCap > 0.0f) {
				vec3 pCap = ray.start + tCap * ray.dir;
				if (length(pCap - baseCenter) <= radius) {
					if (hit.t < 0.0f || tCap < hit.t) {
						hit.t = tCap;
						hit.position = pCap;
						hit.normal = -axis;
						hit.material = material;
					}
				}
			}
		}

		return hit;
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

class Scene {
	std::vector<Intersectable*> objects;
	Camera* camera;
	const vec3 La = vec3(0.2f, 0.2f, 0.2f);
	Light* light;
	std::vector<Light*> lights;
public:
	void add(Intersectable* obj) {
		objects.push_back(obj);
		printf("added obj %p\n", obj);
	}

	void addCam(Camera* cam) {
		camera = cam;
	}

	void addLight(Light* _light) {
		light = _light;
	}

	void addLightSource(Light* _light) {
		lights.push_back(_light);
	}

	vec3 trace(const Ray& ray, int d = 0) {
		if (d > maxdepth) { return vec3(0.0f, 0.0f, 0.0f); }
		Hit bestHit = firstIntersect(ray);
		if (bestHit.t < 0) return bgColor;

		vec3 radiance = bestHit.material->ka * La;
		vec3 r = bestHit.position;
		vec3 N = normalize(bestHit.normal);
		vec3 V = -ray.dir;

		if (bestHit.material->type == isRough) {
			radiance += DirectLight(bestHit, ray);
		}
		if (bestHit.material->type == isReflective) {
			// JPorta: error: ‘reflect’ was not declared in this scope
			vec3 reflectDir = reflect(ray.dir, N);
			Ray reflectRay(r + N * Epsilon, reflectDir, ray.out);
			vec3 F = bestHit.material->fresnelReflectance(dot(V, N), bestHit.material->ks);
			radiance += trace(reflectRay, d + 1) * F;
		}
		if (bestHit.material->type == isRefractive) {
			float ior = ray.out ? bestHit.material->n.x : 1.0f / bestHit.material->n.x;
			// JPorta: error: ‘refract’ was not declared in this scope
			vec3 refractionDir = refract(ray.dir, N, ior);

			vec3 F = bestHit.material->fresnelReflectance(dot(V, N), bestHit.material->ks);

			vec3 reflectDir = reflect(ray.dir, N);
			Ray reflectRay(r + N * Epsilon, reflectDir, ray.out);
			radiance += trace(reflectRay, d + 1) * F;

			if (length(refractionDir) > 0.0f) {
				Ray refractRay(r - N * Epsilon, refractionDir, !ray.out);
				radiance += trace(refractRay, d + 1) * (vec3(1.0f) - F);
			}
		}
		

		return radiance;
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
			vec3 tmpe = vec3(bestHit.normal.x * -1, bestHit.normal.y * -1, bestHit.normal.z * -1);
			bestHit.normal = tmpe;
		}
		return bestHit;
	}

	bool shadowIntersect(Ray ray) {
		for (Intersectable* object : objects) if (object->intersect(ray).t > 0) return true;
		return false;
	}

	vec3 DirectLight(Hit hit) {
		vec3 outRad = hit.material->ka * La;
		for (Light* src : lights) {
			Ray shadowRay(hit.position + hit.normal * Epsilon, src->direction);
			Hit shadowHit = firstIntersect(shadowRay);
			if (shadowHit.t < 0) {
				vec3 L = normalize(src->direction);
				vec3 V = normalize(-hit.position);
				vec3 H = normalize(L + V);

				float cosTheta = maxx(dot(hit.normal, L), 0.0f);
				float cosDelta = maxx(dot(hit.normal, H), 0.0f);

				outRad += hit.material->kd * src->Le * cosTheta;
				outRad += hit.material->ks * src->Le * pow(cosDelta, hit.material->shininess);
			}
		}
		return outRad;
	}

	vec3 DirectLight(Hit hit, Ray ray) {
		vec3 outRadiance = hit.material->ka * La;
		for (Light* light : lights) {
			Ray shadowRay(hit.position + hit.normal * Epsilon, light->direction);
			float cosTheta = dot(hit.normal, light->direction);
			if (cosTheta > 0 && !shadowIntersect(shadowRay)) {	// shadow computation
				outRadiance = outRadiance + light->Le * hit.material->kd * cosTheta;
				vec3 halfway = normalize(-ray.dir + light->direction);
				float cosDelta = dot(hit.normal, halfway);
				if (cosDelta > 0) outRadiance = outRadiance + light->Le * hit.material->ks * powf(cosDelta, hit.material->shininess);
			}
		}
		return outRadiance;
	}
};

class RaytraceApp : public glApp {
	GPUProgram* program;
	Scene* scene;
	Camera* camera;
	Light* light;
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
		light = new Light(vec3(1.0f, 1.0f, 1.0f), vec3(2.0f, 2.0f, 2.0f));

		vec3 eye = vec3(0.0f, 1.0f, 4.0f), vup = vec3(0.0f, 1.0f, 0.0f), lookat = vec3(0.0f, 0.0f, 0.0f);
		float fov = M_PI_4;
		camera->set(eye, lookat, vup, fov);
		scene->addLight(light);
		scene->addLightSource(light);
		scene->addLightSource(new Light(vec3(1.0f, 1.0f, 1.0f), vec3(-0.4f, -0.4f, -0.4f)));

		Material* gold = new Material(vec3(0.0f), vec3(1.0f), 0.0f, vec3(0.17f, 0.35f, 1.5f), vec3(3.1f, 2.7f, 1.9f), isReflective); // arany.v2
		Material* water = new Material(vec3(0.0f), vec3(1.0f), 0.0f, vec3(1.33f, 1.33f, 1.33f), vec3(0.0f), isRefractive); // viz
		Material* white = new Material(vec3(0.3f, 0.3f, 0.3f), vec3(0.0f), 0.0f); // fehér
		Material* blue = new Material(vec3(0.0f, 0.1f, 0.3f), vec3(0.0f), 0.0f); // kék
		Material* yellowPlastic = new Material(vec3(0.3f, 0.2f, 0.1f), vec3(2.0f, 2.0f, 2.0f), 50.0f, vec3(0.0f), vec3(0.0f), isRough); // sárga műanyag
		Material* cyanPlastic = new Material(vec3(0.1f, 0.2f, 0.3f), vec3(2.0f, 2.0f, 2.0f), 100.0f, vec3(0.0f), vec3(0.0f), isRough); // cián műanyag
		Material* magentaPlastic = new Material(vec3(0.3f, 0.0f, 0.2f), vec3(2.0f, 2.0f, 2.0f), 20.0f, vec3(0.0f), vec3(0.0f), isRough); // magenta műanyag

		scene->addCam(camera);
		scene->add(new CheckerPlane(vec3(0.0f, -1.0f, 0.0f), 20.0f, 1.0f, white, blue));
		scene->add(new Cylinder(vec3(0.0f, -1.0f, -0.8f), vec3(-0.2f, 1.0f, -0.1f), 0.3f, 2.0f, water));
		scene->add(new Cylinder(vec3(1.0f, -1.0f, 0.0f), vec3(0.1f, 1.0f, 0.0f), 0.3f, 2.0f, gold));
		scene->add(new Cylinder(vec3(-1.0f, -1.0f, 0.0f), vec3(0.0f, 1.0f, 0.1f), 0.3f, 2.0f, yellowPlastic));
		scene->add(new Cone(vec3(0.0f, 1.0f, 0.0f), vec3(-0.1f, -1.0f, -0.05f), 0.2f, 2.0f, cyanPlastic));
		scene->add(new Cone(vec3(0.0f, 1.0f, 0.8f), vec3(0.2f, -1.0f, -0.0f), 0.2f, 2.0f, magentaPlastic));

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

	void onKeyboard(int key) override {
		if (key == 'a') {
			camera->Spin();
			scene->render(image);
			// printf("You spin my head right round, right round... by [45] degrees\n");
		}
	}

	~RaytraceApp() {
		delete quad;
		delete program;
		delete scene;
		delete camera;
		delete light;
	}
};

RaytraceApp app;
