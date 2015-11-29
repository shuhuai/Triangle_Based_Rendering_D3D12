//--------------------------------------------------------------------------------------
// File: ShaderManager.cpp
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "ShaderManager.h"
#include <Direct.h>
#include <future>

ShaderManager g_ShaderManager;

BOOL ShaderManager::LoadCompiledShader(const std::string filename)
{
	ShaderObject shaderObj;
	FILE* fpVS = nullptr;

	fopen_s(&fpVS, (filename).c_str(), "rb");

	if (!fpVS) { return FALSE; }

	fseek(fpVS, 0, SEEK_END);
	shaderObj.size = ftell(fpVS);
	rewind(fpVS);
	shaderObj.binaryPtr = malloc(shaderObj.size);
	fread(shaderObj.binaryPtr, 1, shaderObj.size, fpVS);

	fclose(fpVS);

	fpVS = nullptr;

	std::size_t strIdx = filename.find_last_of("/\\");
	std::size_t endIdx = filename.find_last_of(".");
	shaderObj.name = filename.substr(strIdx + 1, (endIdx - strIdx) - 1);

	m_shaderMap[shaderObj.name] = shaderObj;

	return TRUE;
}


// Run this function in multiple threads to load different files at the same time.
ShaderObject loadCompiledShaderWorkThread(const std::string filename)
{
	ShaderObject obj;
	FILE* fpVS = nullptr;

	fopen_s(&fpVS, (filename).c_str(), "rb");

	if (!fpVS) { return obj; }

	fseek(fpVS, 0, SEEK_END);
	obj.size = ftell(fpVS);
	rewind(fpVS);
	obj.binaryPtr = malloc(obj.size);
	fread(obj.binaryPtr, 1, obj.size, fpVS);

	fclose(fpVS);

	fpVS = nullptr;
	std::size_t strIdx = filename.find_last_of("/\\");
	std::size_t endIdx = filename.find_last_of(".");
	obj.name = filename.substr(strIdx + 1, (endIdx - strIdx) - 1);

	return obj;
}



BOOL ShaderManager::LoadMtFolder(const std::string foldername)
{
	using namespace std;

	WIN32_FIND_DATA file_data;
	HANDLE Dir;

	std::vector<std::future<ShaderObject>> results;
	// No files found.
	if ((Dir = FindFirstFile((foldername + "/*").c_str(), &file_data)) == INVALID_HANDLE_VALUE)
		return 0;

	do {
		const string file_name = file_data.cFileName;

		// Check the extension name.
		std::size_t idx = file_name.find_last_of(".");
		string extension = file_name.substr(idx);
		if (extension.compare(m_extensionName) == 0)
		{

			const string full_file_name = foldername + "/" + file_name;
			const bool is_directory = (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

			if (file_name[0] == '.')
				continue;
			if (is_directory)
				continue;
			// Create a new job for loading a shader file.
			results.resize(results.size() + 1);
			results[results.size() - 1] = std::async(loadCompiledShaderWorkThread, std::move(full_file_name));
		}
	} while (FindNextFile(Dir, &file_data));

	// Store every shader file to the map.
	for (unsigned int i = 0; i < results.size(); i++)
	{
		ShaderObject obj = results[i].get();
		m_shaderMap[obj.name] = obj;
	}

	FindClose(Dir);

	return true;
}



BOOL ShaderManager::LoadFolder(const std::string foldername)
{
	using namespace std;

	WIN32_FIND_DATA file_data;
	HANDLE Dir;

	// No files found.
	if ((Dir = FindFirstFile((foldername + "/*").c_str(), &file_data)) == INVALID_HANDLE_VALUE)
		return 0;

	do {

		const string file_name = file_data.cFileName;

		// Check the extension name.
		std::size_t idx = file_name.find_last_of(".");
		string extension = file_name.substr(idx);
		if (extension.compare(m_extensionName) == 0)
		{

			const string full_file_name = foldername + "/" + file_name;
			const bool is_directory = (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

			if (file_name[0] == '.')
				continue;

			if (is_directory)
				continue;
			// Load this file.
			LoadCompiledShader(full_file_name);
		}


	} while (FindNextFile(Dir, &file_data));

	FindClose(Dir);

	return true;
}

const ShaderObject* ShaderManager::GetShaderObj(const std::string filename)
{
	return &m_shaderMap[filename];
}

void ShaderManager::Clear()
{
	for (auto it = m_shaderMap.begin(); it != m_shaderMap.end(); ++it)
	{
		delete it->second.binaryPtr;
	}
	m_shaderMap.clear();
}
