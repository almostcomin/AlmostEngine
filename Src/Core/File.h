#pragma once

#include <string>
#include <fstream>
#include <expected>
#include <filesystem>
#include "Core/Blob.h"

namespace st::fs
{

class File
{
public:

	File(const std::string& path);
	~File();

	bool Open(const std::string& path);
	void Close();

	// Reads from current pos size bytes. If size < 0, reads to the end of the file.
	std::expected<st::Blob, std::string> Read(int size = -1);

	bool IsOpen() const;
	size_t Size() const;
	size_t Pos();

	const std::string& GetPath() const;

	static bool Exists(const std::filesystem::path& path);

private:

	std::string m_Path;
	std::ifstream m_FileStream;
	size_t m_FileSize;
};

} // namespace st::