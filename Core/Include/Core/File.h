#pragma once

#include <string>
#include <fstream>
#include <expected>
#include <filesystem>
#include "Core/Blob.h"

namespace alm::fs
{

enum class OpenMode
{
	Read,
	Write
};

class File
{
public:

	File() = default;
	File(const std::string& path, OpenMode mode = OpenMode::Read);
	~File();

	bool Open(const std::string& path, OpenMode mode = OpenMode::Read);
	void Close();

	// Reads from current pos size bytes. If size < 0, reads to the end of the file.
	// Error if open mode is not read
	std::expected<alm::Blob, std::string> Read(int size = -1);

	// Writes from eof size bytes.
	// Error if open mode is not write
	std::expected<size_t, std::string> Write(const void* data, size_t size);

	bool IsOpen() const;
	size_t Size() const;
	size_t Pos();

	const std::string& GetPath() const;

	static bool Exists(const std::filesystem::path& path);

private:

	std::string m_Path;
	std::fstream m_FileStream;
	size_t m_FileSize;
	OpenMode m_Mode;
};

} // namespace st::