#include <SFML/Graphics.hpp>
#include <cmath>
#pragma once

extern const std::vector<sf::Vector3f> vs;
extern const std::vector<std::vector<int>> fs;

unsigned SCREEN_WIDTH = 800;
unsigned SCREEN_HEIGHT = 800;

void point(sf::RenderWindow& window, sf::Vector2f p) {
	float s = 10.f;
	sf::RectangleShape shape( {s, s} );
	shape.setFillColor( sf::Color::Green );
	shape.setPosition({p.x - s/2, p.y - s/2});
	window.draw(shape);
}

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

sf::Vector3f translate(sf::Vector3f p, sf::Vector3f t) {
	return {p.x + t.x, p.y + t.y, p.z + t.z};
}

sf::Vector3f rotate(sf::Vector3f p, float angle) {
	return {
		p.x * cos(angle) - p.z * sin(angle),
		  p.y,
		p.x * sin(angle) + p.z * cos(angle)
	};
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

int main()
{
	sf::RenderWindow window( sf::VideoMode( { SCREEN_WIDTH, SCREEN_HEIGHT } ), "Cube 3D" );

	int FPS = 60;
	float dz = 3.f;
	float angle = 0;
	sf::Vector3f camera = {0.f, 0.f, 0.f};
	float yaw = 0.f;
	float speed = 1.5f;
	float turnSpeed = 2.f;

	while ( window.isOpen() )
	{
		while ( const std::optional event = window.pollEvent() )
		{
			if ( event->is<sf::Event::Closed>() )
				window.close();
		}
		float dt = 1.f/FPS;
		angle += 0.5 * M_PI * dt;

		sf::Vector3f forward = {
			std::sin(yaw),
			0.f,
			std::cos(yaw)
		};
		sf::Vector3f right = {
			std::cos(yaw),
			0.f,
			-std::sin(yaw)
		};
		sf::Vector3f move = {0.f, 0.f, 0.f};
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
			move = add(move, forward);
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
			move = sub(move, forward);
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
			move = add(move, right);
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
			move = sub(move, right);
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q))
			yaw -= turnSpeed * dt;
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E))
			yaw += turnSpeed * dt;
		if (length(move) > 0.f) {
			move = mul(move, speed * dt);
			sf::Vector3f nextCamera = add(camera, move);
			camera = nextCamera;
		}

		window.clear();

		auto transform = [&](sf::Vector3f v) {
			v = rotate(v, angle);
			v = translate_z(v, dz);
			v.x -= camera.x;
			v.y -= camera.y;
			v.z -= camera.z;
			v = rotate_y(v, -yaw);
			return screen(project(v));
		};
		for (auto f : fs) {
			for (int i = 0; i < f.size(); i++) {
				auto a = vs[f[i]];
				auto b = vs[f[(i+1)%f.size()]];
				line(window, transform(a), transform(b));
			}
		}
		window.display();
		sf::sleep(sf::seconds(1.f/FPS));
	}
}
