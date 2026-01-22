#include "GLSLShader.h"
#include <iostream>
#include <fstream>
#include <glad/glad.h>
#include <spirv_cross/spirv_glsl.hpp>

std::vector<uint8_t> ReadSPIRV(const std::filesystem::path& filename) {
	const auto resolvedPath = ResolveFromExeDir(filename);
	std::ifstream file(resolvedPath); 

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open SPIR-V file: " + resolvedPath.string());
	}

	file.seekg(0, std::ios::end);
	size_t fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<uint8_t> spirvData;
	spirvData.resize(fileSize);
	file.read(reinterpret_cast<char*>(spirvData.data()), fileSize);

	return spirvData;
}
std::string GLSLShader::ConvertSPIRVToGLSL(
	const std::vector<uint8_t>& spirvBytes,
	bool isVertexShader
) {
	// Validate SPIR-V size
	if (spirvBytes.empty() || spirvBytes.size() % 4 != 0) {
		throw std::runtime_error("Invalid SPIR-V: empty or size not multiple of 4");
	}

	// Convert byte array to uint32_t array (SPIR-V uses 32-bit words)
	std::vector<uint32_t> spirv(spirvBytes.size() / 4);
	std::memcpy(spirv.data(), spirvBytes.data(), spirvBytes.size());

	// Create SPIRV-Cross compiler
	spirv_cross::CompilerGLSL glsl(std::move(spirv));

	spirv_cross::ShaderResources resources = glsl.get_shader_resources();
	const std::string stagePrefix = isVertexShader ? "vs_" : "fs_";
	for (auto& resource : resources.storage_buffers) {
		uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
		uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
		std::string uniqueName = stagePrefix + resource.name + "_b" + std::to_string(binding);
		glsl.set_name(resource.id, uniqueName);
		glsl.set_name(resource.base_type_id, uniqueName + "_type");
#ifdef _DEBUG
		std::cout << "SSBO: " << resource.name << " - set=" << set << ", binding = " << binding << std::endl;
#endif
		// unset descriptor set as we're using OpenGL
		glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	}
	for (auto& resource : resources.uniform_buffers) {
		uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
		uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
		std::string uniqueName = stagePrefix + resource.name + "_b" + std::to_string(binding);
		glsl.set_name(resource.id, uniqueName);
		glsl.set_name(resource.base_type_id, uniqueName + "_type");
#ifdef _DEBUG
		std::cout << "UBO: " << resource.name << " - set=" << set << ", binding = " << binding << std::endl;
#endif
		// unset descriptor set as we're using OpenGL
		glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	}
	for (auto& resource : resources.sampled_images) {
		uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
		std::string uniqueName = stagePrefix + resource.name + "_b" + std::to_string(binding);
		glsl.set_name(resource.id, uniqueName);
		glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	}

	// Configure OpenGL-specific compiler options
	spirv_cross::CompilerGLSL::Options options;
	options.version = 460;                      
	options.es = false;                         
	options.enable_420pack_extension = true;    
	options.vertex.fixup_clipspace = false;     
	options.vertex.flip_vert_y = false;         

	options.vulkan_semantics = false;
	options.vertex.support_nonzero_base_instance = true;

	glsl.set_common_options(options);

	std::string glslSource = glsl.compile();

	std::cout << "GLSLShader: " << (isVertexShader ? "Vertex" : "Fragment")
		<< " shader converted successfully (" << glslSource.size()
		<< " bytes)" << std::endl;

	return glslSource;
}
GLSLShader::GLSLShader(const std::filesystem::path& filepath)
	: m_FilePath{filepath}, m_RendererID{0}
{
	ParseShader(filepath);
}

GLSLShader::GLSLShader(const std::filesystem::path& vertFilepath, const std::filesystem::path& fragFilepath)
	: m_FilePath{ "" }, m_RendererID{ 0 }
{
	const auto vext = vertFilepath.extension();
	const auto fext = fragFilepath.extension();
	//if (vertFilepath.find(".vert") != std::string::npos && fragFilepath.find(".frag") != std::string::npos) {
	if (vext == ".vert" && fext == ".frag") {
		ShaderProgramSource source;
		source.VertexSource = "";
		source.FragmentSource = "";
		std::ifstream vStream(vertFilepath);
		if (vStream.is_open()) {
			std::string line;
			std::stringstream ss;
			while (getline(vStream, line)) {
				ss << line << '\n';
			}
			source.VertexSource = ss.str();
		}
		else {
			std::cout << "Failed to open vertex shader file: " << vertFilepath << std::endl;
		}
		std::ifstream fStream(fragFilepath);
		if (fStream.is_open()) {
			std::string line;
			std::stringstream ss;
			while (getline(fStream, line)) {
				ss << line << '\n';
			}
			source.FragmentSource = ss.str();
		}
		else {
			std::cout << "Failed to open fragment shader file: " << fragFilepath << std::endl;
		}
		m_RendererID = CreateShader(source.VertexSource, source.FragmentSource);
	}
	//else if (vertFilepath.find(".spv") != std::string::npos && fragFilepath.find(".spv") != std::string::npos) {
	else if (vext == ".spv" && fext == ".spv") {
		std::vector<uint8_t> vertexSPVbytes = ReadSPIRV(vertFilepath);
		std::vector<uint8_t> fragmentSPVbytes = ReadSPIRV(fragFilepath);

		m_RendererID = CreateSpirVShader(vertexSPVbytes, fragmentSPVbytes);
	}
	else {
		std::cout << "Invalid vertex and/or fragment shader filename.\n";
	}
}

GLSLShader::GLSLShader()
	: m_RendererID{ 0 } {
}

GLSLShader::~GLSLShader()
{
	glDeleteProgram(m_RendererID);
}

uint32_t GLSLShader::Program() const {
	return m_RendererID;
}
// Deprecated. Moved to OpenGLRenderer
//void GLSLShader::Bind() const
//{
//	glUseProgram(m_RendererID);
//}

// Deprecated. Moved to OpenGLRenderer
//void GLSLShader::Unbind() const
//{
//	glUseProgram(0);
//}

void GLSLShader::ParseShader(const std::filesystem::path& filepath) {
	const auto resolvedPath = ResolveFromExeDir(filepath);
	std::ifstream stream(resolvedPath); 
	if (!stream.is_open()) {
		throw std::runtime_error("Failed to open shader file: " + resolvedPath.string());
	}

	enum class ShaderType
	{
		NONE = -1, VERTEX = 0, FRAGMENT = 1
	};

	std::string line;
	std::stringstream ss[2];
	ShaderType type = ShaderType::NONE;
	const auto fext = filepath.extension();
	//if (filepath.find(".shader") != std::string::npos) {
	if (fext == ".shader") {
		while (getline(stream, line)) {
			if (line.find("#shader") != std::string::npos) {
				if (line.find("vertex") != std::string::npos) {
					type = ShaderType::VERTEX;
				}
				else if (line.find("fragment") != std::string::npos) {
					type = ShaderType::FRAGMENT;
				}
			}
			else {
				ss[(int)type] << line << '\n';
			}
		}
		m_RendererID = CreateShader(ss[0].str(), ss[1].str());
	}
	else {
		//if (filepath.find(".slang") != std::string::npos) {
		if (fext == ".slang") {
			std::string source = std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
			std::vector<ShaderOutput> slangSpirVOutput = slangCompiler.compileToSPIRV(
				source,
				{ "vertexMain", "fragmentMain" });
			if (slangSpirVOutput.size() == 2) {
				try {
					m_RendererID = CreateShader(ConvertSPIRVToGLSL(slangSpirVOutput[0].binaryData, true),
						ConvertSPIRVToGLSL(slangSpirVOutput[1].binaryData, false));
					// Don't try to do this because slang doesn't support OpenGL SPIR-V so it will be broken
					//m_RendererID = CreateSpirVShader(slangSpirVOutput[0].binaryData, slangSpirVOutput[1].binaryData);
					// Also don't try to do this because slang doesn't support OpenGL GLSL so it will also be broken
					//m_RendererID = CreateShader(slangGLSLOutput[0].asText(), slangGLSLOutput[1].asText());
				} catch (const std::runtime_error& e) {
					// logging to spv files
					std::fstream vFile("vertex_log.spv", std::ios::out | std::ios::binary);
					vFile.write(reinterpret_cast<const char*>(slangSpirVOutput[0].binaryData.data()), slangSpirVOutput[0].binaryData.size());
					vFile.close();
					std::fstream fFile("fragment_log.spv", std::ios::out | std::ios::binary);
					fFile.write(reinterpret_cast<const char*>(slangSpirVOutput[1].binaryData.data()), slangSpirVOutput[1].binaryData.size());
					fFile.close();
					std::cout << "Error creating SPIR-V shader program: " << e.what() << "\n Dumped logs: vertex.spv, fragment.spv" << std::endl;

				}
			}
		}
		else {
			std::cout << "Unsupported shader file format for parsing: " << filepath << std::endl;
		}
	}
}

uint32_t GLSLShader::CompileShader(uint32_t type, const std::string& source) {
	uint32_t id = glCreateShader(type);
	const char* src = source.c_str();
	glShaderSource(id, 1, &src, nullptr);
	glCompileShader(id);

	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);

		char* message = (char*)malloc(length * sizeof(char));
		glGetShaderInfoLog(id, length, &length, message);
		std::cout << "Failed to compile" << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " shader!" << std::endl;
		std::cout << message << std::endl;
		glDeleteShader(id);
		free(message);
		return uint32_t{ 0 };
	}

	return id;
}
uint32_t GLSLShader::CompileSpirVShader(uint32_t type, const std::vector<uint8_t>& SPV) {
	uint32_t id = glCreateShader(type);
	glShaderBinary(1, &id, GL_SHADER_BINARY_FORMAT_SPIR_V, SPV.data(), SPV.size());
	glSpecializeShader(id, "main", 0, 0, 0);
	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
		char* message = (char*)malloc(length * sizeof(char));
		glGetShaderInfoLog(id, length, &length, message);
		std::cout << "Failed to compile" << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " SPIR-V shader!" << std::endl;
		std::cout << message << std::endl;
		glDeleteShader(id);
		free(message);
		return 0;
	}
	return id;
}
uint32_t GLSLShader::CreateSpirVShader(const std::vector<uint8_t>& VertexSPV, const std::vector<uint8_t>& FragmentSPV) {
	uint32_t program = glCreateProgram();
	uint32_t vs = CompileSpirVShader(GL_VERTEX_SHADER, VertexSPV);
	uint32_t fs = CompileSpirVShader(GL_FRAGMENT_SHADER, FragmentSPV);
	if ( 0 == vs || 0 == fs) {
		std::cout << "SPIR-V shader compilation failed." << std::endl;
		glDeleteProgram(program);
		return 0;
	}
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	int32_t isLinked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE)
	{
		int32_t maxLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
		// The maxLength includes the NULL character
		char* infoLog = (char*)malloc(maxLength * sizeof(char));
		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);
		std::cout << "Shader linking failed: " << infoLog << std::endl;
		free(infoLog);
		glDeleteProgram(program);
		throw std::runtime_error("SPIR-V Shader linking failed.");
	}
	glValidateProgram(program);
	glGetProgramiv(program, GL_VALIDATE_STATUS, &isLinked);
	if (isLinked == GL_FALSE)
	{
		int32_t maxLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
		// The maxLength includes the NULL character
		char* infoLog = (char*)malloc(maxLength * sizeof(char));
		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);
		std::cout << "Shader validation failed: " << infoLog << std::endl;
		free(infoLog);
		glDeleteProgram(program);
		return 0;
	}

	glDeleteShader(vs);
	glDeleteShader(fs);

	return program;

}

uint32_t GLSLShader::CreateShader(const std::string& vertexGLSLShader, const std::string& fragmentGLSLShader) {
	uint32_t program = glCreateProgram();
	uint32_t vs = CompileShader(GL_VERTEX_SHADER, vertexGLSLShader);
	uint32_t fs = CompileShader(GL_FRAGMENT_SHADER, fragmentGLSLShader);

	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	int32_t isLinked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE)
	{
		int32_t maxLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
		// The maxLength includes the NULL character
		char* infoLog = (char*)malloc(maxLength * sizeof(char));
		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);
		std::cout << "Shader linking failed: " << infoLog << std::endl;
		free(infoLog);
		glDeleteProgram(program);
		return 0;
	}
	glValidateProgram(program);
	glGetProgramiv(program, GL_VALIDATE_STATUS, &isLinked);
	if (isLinked == GL_FALSE)
	{
		int32_t maxLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
		// The maxLength includes the NULL character
		char* infoLog = (char*)malloc(maxLength * sizeof(char));
		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);
		std::cout << "Shader validation failed: " << infoLog << std::endl;
		free(infoLog);
		glDeleteProgram(program);
		return 0;
	}

	glDeleteShader(vs);
	glDeleteShader(fs);

	return program;
}