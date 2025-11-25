#pragma once
#include <string>
#include <unordered_map>
#include <sstream>
#include <glm/glm.hpp>
#include "ShaderCompiler.h"

struct ShaderProgramSource {
	std::string VertexSource;
	std::string FragmentSource;
};

class GLSLShader {
private:
	SlangCompiler slangCompiler;
	std::string m_FilePath;
	uint32_t m_RendererID;
public:
	GLSLShader(const std::string& filepath);
	GLSLShader(const std::string& vertFilepath, const std::string& fragFilepath);
	GLSLShader();
	~GLSLShader();

	void Bind() const;
	void Unbind() const;

private:
	std::string ConvertSPIRVToGLSL(
		const std::vector<uint8_t>& spirvBytes,
		bool isVertexShader
	);
	void ParseShader(const std::string& filepath);
	uint32_t CompileShader(uint32_t type, const std::string& source);
	uint32_t CreateShader(const std::string& vertexGLSLShader, const std::string& fragmentGLSLShader);
	uint32_t CompileSpirVShader(uint32_t type, const std::vector<uint8_t>& SPV);
	uint32_t CreateSpirVShader(const std::vector<uint8_t>& VertexSPV, const std::vector<uint8_t>& FragmentSPV);
};