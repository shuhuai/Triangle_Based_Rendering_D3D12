//--------------------------------------------------------------------------------------
// File: ShaderManager.h
//
// A class for loading compiled shader files to memory.
//--------------------------------------------------------------------------------------
#pragma once
#include "DirectxHelper.h"
#include <vector>
#include <map>

struct ShaderObject {
	void* binaryPtr;
	size_t  size;
	std::string name;
};

class ShaderManager
{
public:
	// Load a shader file.
	BOOL LoadCompiledShader(const std::string filename);
	// Use multiple threads to load all shader files in a folder.
	BOOL LoadMtFolder(const std::string foldername);
	// Load all shader files in a folder.
	BOOL LoadFolder(const std::string foldername);

	const ShaderObject* GetShaderObj(std::string filename);

	// Clear all shader data in memory.
	void Clear();

private:
	std::map<std::string, ShaderObject> m_shaderMap;
	// The extension name for shader files.
	const std::string m_extensionName = ".cso";
};

// Global shader manager.
extern ShaderManager g_ShaderManager;