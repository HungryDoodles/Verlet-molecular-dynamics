#pragma once
#include <vector>
#include <map>
#include <fstream>
#include <GL/glew.h>
#include <GL/glut.h>


class GLProgram 
{
	GLuint program = 0;
	std::vector<GLuint> shaders;
	std::map<std::string, GLint> uniforms;

	bool bLinked = false;

public:
	GLProgram() 
	{ }

	void Initialize(const std::string& shaderFile, GLuint shaderType, const std::vector<std::string>& uniformNames)
	{
		CreateProgram();
		AddShaderFromFile(shaderFile, shaderType);
		LinkProgram();
		InitializeUniforms(uniformNames);
	}
	void Initialize(const std::string& shaderFile1, GLuint shaderType1,
		const std::string& shaderFile2, GLuint shaderType2,
		const std::vector<std::string>& uniformNames)
	{
		CreateProgram();
		AddShaderFromFile(shaderFile1, shaderType1);
		AddShaderFromFile(shaderFile2, shaderType2);
		LinkProgram();
		InitializeUniforms(uniformNames);
	}

	~GLProgram() 
	{
		if (HasProgram()) DeleteProgram();
	}

	GLuint CreateProgram() 
	{
		if (HasProgram()) DeleteProgram();
		program = glCreateProgram();
		return program;
	}
	GLuint Get() const { return program; }
	void DeleteProgram() 
	{
		if (program != 0)
		{
			glDeleteProgram(program);
			program = 0;
		}
		for (auto& shader : shaders)
			glDeleteShader(shader);
		shaders.clear();
		bLinked = false;
		uniforms.clear();
	}

	GLuint AddShaderFromFile(const std::string& filename, GLuint shaderType) 
	{
		if (!HasProgram())
			return 0;

		std::ifstream t(filename);
		std::string source((std::istreambuf_iterator<char>(t)),
			std::istreambuf_iterator<char>());

		return AddShader(source, shaderType);
	}
	GLuint AddShader(const std::string& source, GLuint shaderType) 
	{
		if (!HasProgram())
			return -1;

		GLuint shader = glCreateShader(shaderType);
		const char* sourcePtr = source.c_str();
		int sourceSize = (int)source.size();
		glShaderSource(shader, 1, &sourcePtr, &sourceSize);
		glCompileShader(shader);

		GLint compilationStatus = -1;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compilationStatus);

		if (compilationStatus != GL_TRUE) 
		{
			GLint maxLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

			if (maxLength == 0)
				maxLength = 10;

			// The maxLength includes the NULL character
			std::vector<GLchar> errorLog(maxLength);
			glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

			std::cout << "Shader error: " << std::string(errorLog.begin(), errorLog.end()) << std::endl;

			glDeleteShader(shader);
			return 0;
		}

		shaders.push_back(shader);
		return shader;
	}

	void LinkProgram() 
	{
		for (auto& shader : shaders) 
			glAttachShader(program, shader);
		glLinkProgram(program);

		GLint linkStatus = -1;

		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		bLinked = linkStatus == GL_TRUE;

		if (!bLinked) 
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

			if (maxLength == 0)
				maxLength = 10;

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

			std::cout << "Link error: " << std::string(infoLog.begin(), infoLog.end()) << std::endl;
		}
	}

	bool HasProgram() const { return program != 0; }
	bool IsLinked() const { return bLinked; }

	int InitializeUniforms(const std::vector<std::string>& names) 
	{
		if (!IsLinked())
			return 0;
		int counter = 0;
		for (auto& name : names) 
		{
			GLint newUniform = glGetUniformLocation(program, name.c_str());
			if (newUniform == -1)
				continue;

			uniforms[name] = newUniform;
			++counter;
		}
		return counter;
	}
	
	GLint operator[](const std::string& name) const
	{
		auto pair = uniforms.find(name);
		if (pair == uniforms.end())
			return -1;
		return (*pair).second;
	}

	explicit operator GLuint() const { return program; }
	explicit operator bool() const { return IsLinked(); }
};

class GLBuffer
{
	GLuint buffer = 0;
	GLuint type = 0;
public:
	GLBuffer()
	{ }
	void Initialize(GLuint bufferType, GLsizeiptr size, void* data, GLuint accessType)
	{
		CreateBuffer(bufferType, size, data, accessType);
	}
	void Initialize(GLuint bufferType, GLsizeiptr size, void* data, GLuint accessType, GLuint index)
	{
		CreateBuffer(bufferType, size, data, accessType);
		SetBase(index);
	}
	~GLBuffer()
	{
		DeleteBuffer();
	}

	GLuint CreateBuffer(GLuint bufferType, GLsizeiptr size, void* data, GLuint accessType) 
	{
		glCreateBuffers(1, &buffer);
		type = bufferType;
		glBindBuffer(type, buffer);
		glBufferData(type, size, data, accessType);
		glBindBuffer(type, 0);
		return buffer;
	}
	void DeleteBuffer()
	{
		if (buffer != 0) 
		{
			glDeleteBuffers(1, &buffer);
			buffer = 0;
		}
	}
	void SetBase(GLuint index) 
	{
		if (!HasBuffer())
			return;
		glBindBuffer(type, buffer);
		glBindBufferBase(type, index, buffer);
		glBindBuffer(type, 0);
	}

	bool HasBuffer() const
	{
		return buffer != 0;
	}
	GLuint Get() const { return buffer; }

	operator GLuint() const
	{
		return buffer;
	}
	operator bool() const { return buffer != 0; }
};