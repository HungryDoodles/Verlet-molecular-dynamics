#include <GL/glew.h>
#include "VerletSimulator.h"
#include <memory>
#include <GL/glut.h>

using namespace std;

typedef float real;
typedef Vector2<real> Vector2r;
typedef VerletSimulator<real> SimType;

unsigned int wndX = 1024, wndY = 1024;
float compExecTimeMS = 0;
float rendExecTimeMS = 0;
bool bRender = true;
bool flagReinit = false;
bool flagStat = false;

unique_ptr<ISimulator<real>> sim;

void GlutIdleFunc()
{
	auto start = chrono::steady_clock::now();

	if (flagReinit)
	{
		cout << endl << endl;
		cout << "REINITIALIZATION" << endl;
		sim->Initialize("Config.ini");
		flagReinit = false;
	}

	sim->Update();

	compExecTimeMS = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - start).count() / 1000.0f;

	glutPostRedisplay();

	if (flagStat)
	{
		int N = sim->GetN();
		real dt = sim->GetDt();
		Vector2r dims = sim->GetDims();

		string infoString = "";
		const map<string, real>& stats = sim->GetStats();
		for (auto& pair : stats)
			infoString += pair.first + ": " + to_string(pair.second) + "\r\n";

		infoString +=
			"dT: " + to_string(dt) + "\r\n" +
			"comp time: " + to_string(compExecTimeMS) + "\r\n" +
			"rend time: " + to_string(rendExecTimeMS);

		cout << "Simulation Stats:" << endl;
		cout << infoString << endl;

		flagStat = 0;
	}
}

void GlutDisplayFunc() 
{
	auto start = chrono::steady_clock::now();

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	if (bRender) sim->Draw();

	glutSwapBuffers();

	rendExecTimeMS = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - start).count() / 1000.0f;
}

void GlutKeyboardFunc(unsigned char keycode, int x, int y) 
{
	if (keycode == 's')
	{
		bool bNewSim = !sim->GetSimulate();
		cout << (bNewSim ? "Simulation enabled" : "Simulation disabled") << endl;
		sim->SetSimulate(bNewSim);
	}
	if (keycode == 'd')
	{
		bool bNewRender = !bRender;
		cout << (bNewRender ? "Rendering enabled" : "Rendering disabled") << endl;
		bRender = bNewRender;
	}
	if (keycode == 'r')
		flagReinit = true;
	if (keycode == 'x')
		flagStat = true;
}

void GLAPIENTRY MessageCallback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	auto callbackMessage = ((type == GL_DEBUG_TYPE_ERROR) ? "** GL ERROR **" : "");
	cout << "GL CALLBACK: " << callbackMessage
		<< " type = " << type
		<< ", severity = " << severity
		<< ", message = " << message << endl;
}

void SetupGlutGlew(int argc, char** argv)
{
	glutInit(&argc, argv);

	glutSetOption(GLUT_MULTISAMPLE, 8);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_MULTISAMPLE);
	glutInitContextVersion(4, 3);
	glutInitContextFlags(GLUT_CORE_PROFILE);
	glutInitWindowSize(wndX, wndY);
	glutCreateWindow("Compute context");
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glewExperimental = GL_TRUE;

	if (glewInit() != GLEW_OK)
		cout << "glew failed" << endl;

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(MessageCallback, 0);

	glutDisplayFunc(GlutDisplayFunc);
	glutIdleFunc(GlutIdleFunc);
	glutKeyboardFunc(GlutKeyboardFunc);
}

int main(int argc, char** argv)
{
	SetupGlutGlew(argc, argv);

	sim = unique_ptr<ISimulator<real>>(new SimType);

	flagReinit = true;
	sim->SetSimulate(true);
	glutMainLoop();

	sim.release();
	return 0;
}