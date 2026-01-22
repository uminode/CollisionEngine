#pragma once
#ifdef _WIN32

#include <string>
#include <wrl.h>
//#include <d3dcompiler.h>
#include <dxcapi.h>
#include <filesystem>
#include "ShaderCompiler.h"
#include <vector>
enum class ShaderType
{
	NONE = -1, VERTEX = 0, FRAGMENT = 1
};
class HLSLShader {
private:
	SlangCompiler slangCompiler;
	std::filesystem::path m_FilePath;
	std::vector<std::string> m_entryPoints = { "vertexMain", "fragmentMain" };
	Microsoft::WRL::ComPtr<IDxcBlob> m_vertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> m_fragmentShaderBlob;
	std::vector<ShaderResourceBinding> m_bindings;
public:
	HLSLShader(const std::filesystem::path& filepath);
	HLSLShader(const std::filesystem::path& vertFilepath, const std::filesystem::path& fragFilepath);
	inline Microsoft::WRL::ComPtr<IDxcBlob> getVertexShaderBlob() const { return m_vertexShaderBlob.Get(); }
	inline Microsoft::WRL::ComPtr<IDxcBlob> getFragmentShaderBlob() const { return m_fragmentShaderBlob.Get(); }
	inline std::vector<ShaderResourceBinding> getBindings() const { return m_bindings; }
	void CompileShader(ShaderType type, const std::string& source);
	HLSLShader() = default;
	~HLSLShader() = default;

//	void Bind() const;
//	void Unbind() const;

};
#endif