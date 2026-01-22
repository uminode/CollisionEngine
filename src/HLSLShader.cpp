#pragma once
#ifdef _WIN32
#include "HLSLShader.h"
#include "PathUtils.h"
#include "DXHelper.h"
#include <fstream>
HLSLShader::HLSLShader(const std::filesystem::path& filepath) {
	std::filesystem::path resolvedPath = ResolveFromExeDir(filepath);
	m_FilePath = resolvedPath;

	std::ifstream stream(resolvedPath); 
	if (!stream.is_open()) {
		throw std::runtime_error("Failed to open shader file: " + resolvedPath.string());
	}

	std::string line;
	std::stringstream ss[2];
	ShaderType type = ShaderType::NONE;
	const auto fext = filepath.extension();
	if (fext == ".slang") {
		std::string source = std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		std::vector<ShaderOutput> hlslSource = slangCompiler.compileToHLSL(source, m_entryPoints);
		m_bindings = hlslSource[0].resourceBindings; // assuming both shaders have same bindings
		try {

			CompileShader(ShaderType::VERTEX, hlslSource[0].asText());
			CompileShader(ShaderType::FRAGMENT, hlslSource[1].asText());
//			ThrowIfFailed(D3DCompile(
//				hlslSource[0].asText().c_str(),
//				hlslSource[0].asText().size(),
//				nullptr,
//				nullptr,
//				nullptr,
//				"vertexMain",
//				"vs_5_0",
//				D3DCOMPILE_ENABLE_STRICTNESS,
//				0,
//				m_vertexShaderBlob.GetAddressOf(),
//				nullptr
//			));
//			ThrowIfFailed(D3DCompile(
//				hlslSource[1].asText().c_str(),
//				hlslSource[1].asText().size(),
//				nullptr,
//				nullptr,
//				nullptr,
//				"fragmentMain",
//				"ps_5_0",
//				D3DCOMPILE_ENABLE_STRICTNESS,
//				0,
//				m_fragmentShaderBlob.GetAddressOf(),
//				nullptr
//			));

		}
		catch (const std::runtime_error& e) {
			// logging to spv files
			std::fstream vFile("vertex_log.hlsl", std::ios::out);
			std::string hlslSourceText = hlslSource[0].asText();
			vFile.write(hlslSourceText.c_str(), hlslSourceText.size());
			vFile.close();
			std::fstream fFile("fragment_log.hlsl", std::ios::out);
			hlslSourceText = hlslSource[1].asText();
			fFile.write(hlslSourceText.c_str(), hlslSourceText.size());
			fFile.close();
			std::cout << "Error creating HLSL shaders: " << e.what() << "\n Dumped logs: vertex_log.hlsl, fragment_log.hlsl" << std::endl;
			std::cerr << "Error creating HLSL shaders: " << e.what() << "\n Dumped logs: vertex_log.hlsl, fragment_log.hlsl" << std::endl;
		}
	}
	else {
		throw std::runtime_error("Unsupported shader file format for HLSLShader (only .slang files are accepted): " + resolvedPath.string());
	}

}
HLSLShader::HLSLShader(const std::filesystem::path& vertFilepath, const std::filesystem::path& fragFilepath) : m_FilePath{ "" } {
	throw std::runtime_error("HLSLShader dual file constructor not implemented yet. Sorry");
}

void HLSLShader::CompileShader(ShaderType type, const std::string& source) {
	Microsoft::WRL::ComPtr<IDxcUtils> utils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

	DxcBuffer sourceBuffer;
	sourceBuffer.Ptr = source.c_str();
	sourceBuffer.Size = source.size();
	sourceBuffer.Encoding = DXC_CP_UTF8;

	std::vector<LPCWSTR> arguments = {
		L"-E", (ShaderType::VERTEX == type) ? L"vertexMain" : L"fragmentMain",
		L"-T", (ShaderType::VERTEX == type) ? L"vs_6_8" : L"ps_6_8"
#ifdef _DEBUG
				,L"-Zi",
		L"-Qembed_debug"
#endif
	};

	Microsoft::WRL::ComPtr<IDxcResult> result;
	ThrowIfFailed(compiler->Compile(
		&sourceBuffer,
		arguments.data(),
		arguments.size(),
		nullptr,
		IID_PPV_ARGS(&result)
	));
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0) {
		OutputDebugStringA(errors->GetStringPointer());
		throw std::runtime_error("Shader compilation failed");
	}
#endif
	if (ShaderType::VERTEX == type) {
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&m_vertexShaderBlob), nullptr);
	}
	else if (ShaderType::FRAGMENT == type) {
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&m_fragmentShaderBlob), nullptr);

	}
	else {
		throw std::runtime_error("Unsupported shader type for HLSLShader::CompileShader");
	}

}

#endif