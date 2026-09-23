#include <SFML/Graphics.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "penger.h"

// Stuff3D: a from-scratch wireframe renderer. Every vertex goes through the
// same small pipeline by hand (no OpenGL matrices, no depth buffer):
//
//   model space --spin(angle)--> --push +dz--> world space
//   world space --minus camera--> --rotate by yaw--> view space
//   view space  --clip at NEAR--> --x/z, y/z--> NDC --> screen pixels
//
// Run with `--trace out.json` (or `--trace web/trace.js`) to simulate a
// scripted session headlessly and dump every frame (projected points plus
// one vertex's journey through each stage) for the web visualizer in web/.

const unsigned SCREEN_WIDTH = 800;
const unsigned SCREEN_HEIGHT = 800;
const float PI = 3.14159265358979f;
const float NEAR = 0.1f;
const int TRACKED_VERTEX = 21;   // tip of the beak

sf::Vector2f screen(sf::Vector2f p) {
	return {
		(p.x + 1.f)/2.f*SCREEN_WIDTH,
		(1.f - p.y)/2*SCREEN_HEIGHT
	};
}

sf::Vector2f project(sf::Vector3f p) {
	return {p.x / p.z, p.y / p.z};
}

sf::Vector3f translate_z(sf::Vector3f p, float dz) {
	return {p.x, p.y, p.z+dz};
}

sf::Vector3f rotate_y(sf::Vector3f p, float angle) {
	return {
		p.x * std::cos(angle) - p.z * std::sin(angle),
		p.y,
		p.x * std::sin(angle) + p.z * std::cos(angle)
	};
}

void line(sf::RenderWindow& window, sf::Vector2f start, sf::Vector2f end) {
	std::vector<sf::Vertex> l = {
		sf::Vertex({start, sf::Color::Green}),
		sf::Vertex({end, sf::Color::Green}),
	};
	window.draw(l.data(), l.size(), sf::PrimitiveType::Lines);
}

float length(sf::Vector3f v) {
	return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

sf::Vector3f add(sf::Vector3f a, sf::Vector3f b) {
	return {a.x + b.x, a.y + b.y, a.z + b.z};
}

sf::Vector3f sub(sf::Vector3f a, sf::Vector3f b) {
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

sf::Vector3f mul(sf::Vector3f v, float s) {
	return {v.x * s, v.y * s, v.z * s};
}

sf::Vector3f lerp(sf::Vector3f a, sf::Vector3f b, float t) {
	return add(a, mul(sub(b, a), t));
}

// ---------------------------------------------------------------------------
// Simulation state shared by the window loop and the trace mode.

struct Input {
	bool w = false, a = false, s = false, d = false, q = false, e = false;
};

struct State {
	float dz = 3.f;
	float angle = 0.f;
	sf::Vector3f camera = {0.f, 0.f, 0.f};
	float yaw = 0.f;
	float speed = 1.5f;
	float turnSpeed = 2.f;
};

void update(State& st, const Input& in, float dt) {
	st.angle += 0.5f * PI * dt;

	sf::Vector3f forward = {
		std::sin(st.yaw),
		0.f,
		std::cos(st.yaw)
	};
	sf::Vector3f right = {
		std::cos(st.yaw),
		0.f,
		-std::sin(st.yaw)
	};
	sf::Vector3f move = {0.f, 0.f, 0.f};
	if (in.w) move = add(move, forward);
	if (in.s) move = sub(move, forward);
	if (in.d) move = add(move, right);
	if (in.a) move = sub(move, right);
	if (in.q) st.yaw -= st.turnSpeed * dt;
	if (in.e) st.yaw += st.turnSpeed * dt;
	if (length(move) > 0.f) {
		move = mul(move, st.speed * dt);
		st.camera = add(st.camera, move);
	}
}

// Every intermediate value of the pipeline for one vertex.
struct Stages {
	sf::Vector3f model, spun, world, relative, view;
	sf::Vector2f ndc, pixel;
};

Stages run_pipeline(const State& st, sf::Vector3f v) {
	Stages s;
	s.model = v;
	s.spun = rotate_y(v, st.angle);                // spin the model about its own Y axis
	s.world = translate_z(s.spun, st.dz);          // push it dz units in front of the origin
	s.relative = sub(s.world, st.camera);          // move the world so the camera sits at 0,0,0
	s.view = rotate_y(s.relative, st.yaw);         // turn the world so the camera looks down +Z
	s.ndc = s.view.z > 0.f ? project(s.view) : sf::Vector2f{0.f, 0.f};
	s.pixel = screen(s.ndc);
	return s;
}

sf::Vector3f to_view(const State& st, sf::Vector3f v) {
	return run_pipeline(st, v).view;
}

// Clip a view-space segment against the plane z = NEAR. Without this, points
// behind the camera divide by a negative z and flip across the screen.
bool clip_near(sf::Vector3f& a, sf::Vector3f& b) {
	if (a.z < NEAR && b.z < NEAR) return false;
	if (a.z < NEAR) a = lerp(a, b, (NEAR - a.z) / (b.z - a.z));
	else if (b.z < NEAR) b = lerp(b, a, (NEAR - b.z) / (a.z - b.z));
	return true;
}

// ---------------------------------------------------------------------------
// Trace mode: the same update() and pipeline, driven by a scripted key
// timeline instead of the keyboard, written out as JSON.

struct Segment { float seconds; const char* keys; };

// A ~13 second tour: watch the spin, walk in, strafe around the model,
// walk straight through it (near-plane clipping), turn around, back out.
const Segment SCRIPT[] = {
	{2.0f, ""},
	{1.0f, "w"},
	{1.0f, "d"},
	{0.4f, "q"},
	{1.2f, "d"},
	{0.35f, "q"},
	{2.2f, "w"},
	{1.57f, "e"},
	{1.5f, "s"},
	{0.8f, "a"},
	{1.4f, ""},
};

Input keys_to_input(const char* k) {
	Input in;
	in.w = std::strchr(k, 'w'); in.a = std::strchr(k, 'a');
	in.s = std::strchr(k, 's'); in.d = std::strchr(k, 'd');
	in.q = std::strchr(k, 'q'); in.e = std::strchr(k, 'e');
	return in;
}

void put3(FILE* f, sf::Vector3f v) { std::fprintf(f, "[%.4f,%.4f,%.4f]", v.x, v.y, v.z); }
void put2(FILE* f, sf::Vector2f v) { std::fprintf(f, "[%.4f,%.4f]", v.x, v.y); }

int write_trace(const char* path, int tracked) {
	FILE* f = std::fopen(path, "w");
	if (!f) { std::perror(path); return 1; }

	const int FPS = 60;
	const int EVERY = 2;   // keep every 2nd simulated frame -> 30 fps in the trace
	const float dt = 1.f / FPS;
	State st;

	// A .js path gets wrapped as `window.TRACE = {...};` so the web page can
	// load it with a <script> tag straight from disk (no server, no fetch).
	size_t n = std::strlen(path);
	bool as_js = n > 3 && std::strcmp(path + n - 3, ".js") == 0;
	if (as_js) std::fprintf(f, "window.TRACE = ");

	std::fprintf(f, "{\n\"screen\":[%u,%u],\"fps\":%d,\"near\":%.2f,\"dz\":%.2f,\"tracked\":%d,\n",
		SCREEN_WIDTH, SCREEN_HEIGHT, FPS / EVERY, NEAR, st.dz, tracked);
	std::fprintf(f, "\"vertices\":[");
	for (size_t i = 0; i < vs.size(); i++) { if (i) std::fputc(',', f); put3(f, vs[i]); }
	std::fprintf(f, "],\n\"faces\":[");
	for (size_t i = 0; i < fs.size(); i++) {
		std::fprintf(f, "%s[%d,%d,%d]", i ? "," : "", fs[i][0], fs[i][1], fs[i][2]);
	}
	std::fprintf(f, "],\n\"frames\":[\n");

	int frame = 0, written = 0;
	for (const Segment& seg : SCRIPT) {
		Input in = keys_to_input(seg.keys);
		int steps = (int)std::lround(seg.seconds * FPS);
		for (int i = 0; i < steps; i++, frame++) {
			update(st, in, dt);
			if (frame % EVERY) continue;

			// Projected pixel + view depth per vertex. Vertices behind the near
			// plane are written as null.
			std::fprintf(f, "%s{\"t\":%.3f,\"keys\":\"%s\",\"angle\":%.4f,\"yaw\":%.4f,\"camera\":",
				written ? ",\n" : "", frame * dt, seg.keys, st.angle, st.yaw);
			put3(f, st.camera);
			std::fprintf(f, ",\"px\":[");
			for (size_t v = 0; v < vs.size(); v++) {
				Stages s = run_pipeline(st, vs[v]);
				if (v) std::fputc(',', f);
				if (s.view.z < NEAR) std::fprintf(f, "null");
				else std::fprintf(f, "[%.1f,%.1f,%.3f]", s.pixel.x, s.pixel.y, s.view.z);
			}
			// Edges crossing the near plane, cut exactly the way the window
			// renderer cuts them: [x0,y0,x1,y1] in pixels.
			std::fprintf(f, "],\"clipped\":[");
			int nclip = 0;
			for (const auto& face : fs) {
				for (size_t k = 0; k < face.size(); k++) {
					sf::Vector3f a = to_view(st, vs[face[k]]);
					sf::Vector3f b = to_view(st, vs[face[(k+1)%face.size()]]);
					if ((a.z < NEAR) == (b.z < NEAR)) continue;
					if (!clip_near(a, b)) continue;
					sf::Vector2f pa = screen(project(a)), pb = screen(project(b));
					std::fprintf(f, "%s[%.1f,%.1f,%.1f,%.1f]", nclip++ ? "," : "", pa.x, pa.y, pb.x, pb.y);
				}
			}
			Stages s = run_pipeline(st, vs[tracked]);
			std::fprintf(f, "],\"stages\":{\"model\":"); put3(f, s.model);
			std::fprintf(f, ",\"spun\":"); put3(f, s.spun);
			std::fprintf(f, ",\"world\":"); put3(f, s.world);
			std::fprintf(f, ",\"relative\":"); put3(f, s.relative);
			std::fprintf(f, ",\"view\":"); put3(f, s.view);
			std::fprintf(f, ",\"ndc\":"); put2(f, s.ndc);
			std::fprintf(f, ",\"pixel\":"); put2(f, s.pixel);
			std::fprintf(f, "}}");
			written++;
		}
	}
	std::fprintf(f, "\n]}%s\n", as_js ? ";" : "");
	std::fclose(f);
	std::fprintf(stderr, "wrote %d frames (%d vertices, %zu faces) to %s\n",
		written, (int)vs.size(), fs.size(), path);
	return 0;
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "--trace") == 0) {
			const char* out = i + 1 < argc ? argv[i+1] : "trace.json";
			return write_trace(out, TRACKED_VERTEX);
		}
	}

	sf::RenderWindow window( sf::VideoMode( { SCREEN_WIDTH, SCREEN_HEIGHT } ), "Stuff3D" );
	window.setFramerateLimit(60);

	int FPS = 60;
	State st;

	while ( window.isOpen() )
	{
		while ( const std::optional event = window.pollEvent() )
		{
			if ( event->is<sf::Event::Closed>() )
				window.close();
			if ( const auto* key = event->getIf<sf::Event::KeyPressed>() )
				if ( key->code == sf::Keyboard::Key::Escape )
					window.close();
		}
		float dt = 1.f/FPS;

		Input in;
		if (window.hasFocus()) {
			in.w = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
			in.s = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S);
			in.d = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
			in.a = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A);
			in.q = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q);
			in.e = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E);
		}
		update(st, in, dt);

		window.clear();
		for (const auto& f : fs) {
			for (size_t i = 0; i < f.size(); i++) {
				sf::Vector3f a = to_view(st, vs[f[i]]);
				sf::Vector3f b = to_view(st, vs[f[(i+1)%f.size()]]);
				if (!clip_near(a, b)) continue;
				line(window, screen(project(a)), screen(project(b)));
			}
		}
		window.display();
	}
}
