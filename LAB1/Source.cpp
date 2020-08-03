#include <GL/glew.h>
#include "VerletSimulator.h"
#include "StepperSimulator.h"
#include <memory>
#include <GL/glut.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <fstream>

using namespace std;

typedef float real;
typedef Vector2<real> Vector2r;
typedef VerletSimulator<real> SimType;

const float triangleLenBase = 1024.0 * 0.005;
const float viewScale = 0.8;
unsigned int sfmlwndX = 1024, sfmlwndY = 512;
unsigned int wndX = 1024, wndY = 512;
sf::VertexArray triangles;
sf::Font font;
sf::Text infoText;
float compExecTimeMS = 0;
float rendExecTimeMS = 0;

unique_ptr<ISimulator<real>> sim;
sf::RenderWindow sfmlWnd;
ofstream outf("output.txt");

void SetupSFMLWindow() 
{
	sf::ContextSettings settings;
	settings.depthBits = 24;
	settings.stencilBits = 8;
	settings.antialiasingLevel = 2;
	settings.majorVersion = 2;
	settings.minorVersion = 0;

	sfmlWnd.create(sf::VideoMode(sfmlwndX, sfmlwndY), "Data Display", 7U, settings);
	sf::ContextSettings newSettings = sfmlWnd.getSettings();
	if (newSettings.majorVersion != 4)
		cout << "SFML window context set failed" << endl;

	sfmlWnd.setFramerateLimit(60);
	if (!font.loadFromFile("FONT.TTF"))
	{
		cout << "Font failed to load!" << endl;
	}
	else
	{
		infoText.setFont(font);
		infoText.setScale({0.3, 0.3});
	}
}

void Results(sf::RenderWindow& wnd)
{
	int N = sim->GetN();
	real dt = sim->GetDt();
	Vector2r dims = sim->GetDims();

	if (triangles.getVertexCount() != N * 3)
		triangles = sf::VertexArray(sf::PrimitiveType::Triangles, 3 * N);

	wnd.clear(sf::Color::Black);

	sf::Transform rot1 = sf::Transform::Identity;
	rot1.rotate(360 / 3);
	sf::Transform rot2 = sf::Transform::Identity;
	rot2.rotate(2 * 360 / 3);

	const vector<Component<real>>& comps = sim->GetComponents();

	sf::Vector2f screenSizeHalf(sfmlWnd.getSize().x / 2.0f, sfmlWnd.getSize().y / 2.0f);
	for (int i = 0; i < N; ++i)
	{
		Vector2r vn = comps[i].v.Normalized();
		sf::Vector2f sfv(float(vn.x), float(vn.y));
		sf::Vector2f offset(float(comps[i].p.x / dims.x * sfmlwndX), float(comps[i].p.y / dims.y * sfmlwndY));

		sf::Vector2f p1, p2, p3;
		p1 = offset + (sfv * float(comps[i].v.Size() * dt + 1)) * triangleLenBase;
		p2 = offset + (rot1 * sfv) * triangleLenBase;
		p3 = offset + (rot2 * sfv) * triangleLenBase;

		triangles[i * 3 + 0].position = (p1 - screenSizeHalf) * viewScale + screenSizeHalf;
		triangles[i * 3 + 1].position = (p2 - screenSizeHalf) * viewScale + screenSizeHalf;
		triangles[i * 3 + 2].position = (p3 - screenSizeHalf) * viewScale + screenSizeHalf;

		real alpha = 1 - atan(comps[i].v.Size() * dt * 100);
		sf::Color theColor = sf::Color(sf::Uint8(255 * (1 - alpha)), 127, sf::Uint8(255 * alpha), 255);
		triangles[i * 3 + 0].color = theColor;
		triangles[i * 3 + 1].color = theColor;
		triangles[i * 3 + 2].color = theColor;
	}

	string infoString = "";
	const map<string, real>& stats = sim->GetStats();
	for(auto& pair : stats)
		infoString += pair.first + ": " + to_string(pair.second) + "\r\n";


	infoText.setString(infoString +
		"dT: " + to_string(dt) + "\r\n" +
		"comp time: " + to_string(compExecTimeMS) + "\r\n" +
		"rend time: " + to_string(rendExecTimeMS));

	wnd.draw(triangles);
	wnd.draw(infoText);
	wnd.display();

	//if (sim->GetSimulate()) outf << stats.at("Time") << "\t" << int(stats.at("In box")) << endl;
}

void TakeScreenshot() 
{
	sf::Texture screenshot;

	auto windowSize = sfmlWnd.getSize();
	screenshot.create(windowSize.x, windowSize.y);
	screenshot.update(sfmlWnd);

	if (screenshot.copyToImage().saveToFile("Screenshot.png"))
	{
		std::cout << "Screenshot saved to " << "Screenshot.png" << std::endl;
	}
	else
	{
		std::cout << "I fucked up saving a screenshot" << std::endl;
	}
}

int main(int argc, char ** argv) 
{
	SetupSFMLWindow();

	sim = unique_ptr<ISimulator<real>>(new SimType);

	sim->Initialize("Config.ini");

	while(sfmlWnd.isOpen())
	{
		sf::Event event;
		while (sfmlWnd.pollEvent(event))
		{
			switch (event.type)
			{
			case sf::Event::Closed:
				sfmlWnd.close();
				break;
			case sf::Event::Resized:
				wndX = sfmlWnd.getSize().x;
				wndY = sfmlWnd.getSize().y;
				break;
			case sf::Event::KeyPressed:
				if (event.key.code == sf::Keyboard::Key::S)
					sim->SetSimulate(!sim->GetSimulate());
				if (event.key.code == sf::Keyboard::Key::R)
					sim->Initialize("Config.ini");
				if (event.key.code == sf::Keyboard::Key::P)
					TakeScreenshot();
				break;
			}
		}

		auto start = chrono::steady_clock::now();

		sim->Update();

		float compExecTimeMS = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - start).count() / 1000.0f;

		start = chrono::steady_clock::now();

		Results(sfmlWnd);

		rendExecTimeMS = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - start).count() / 1000.0f;
		::compExecTimeMS = compExecTimeMS;
	}

	sim.release();
	return 0;
}