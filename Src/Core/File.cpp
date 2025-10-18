#include "Core/File.h"
#include <format>

st::fs::File::File(const std::string& path)
{
	Open(path);	
}

st::fs::File::~File()
{
	Close();
}

bool st::fs::File::Open(const std::string& path)
{
	m_FileStream.open(path, std::ios::binary);
	if (m_FileStream.is_open())
	{
		m_FileStream.seekg(0, std::ios::end);
		m_FileSize = m_FileStream.tellg();
		m_FileStream.seekg(0, std::ios::beg);

		return true;
	}
	else
	{
		m_FileSize = 0;
		return false;
	}
}

void st::fs::File::Close()
{
	m_FileStream.close();
}

std::expected<st::Blob, std::string> st::fs::File::Read(int size)
{
	if (!m_FileStream.is_open())
	{
		return std::unexpected("File is not opened");
	}

	size_t pos = m_FileStream.tellg();
	if (size < 0)
	{
		size = m_FileSize - pos;
	}
	else
	{
		if (size > (m_FileSize - pos))
		{
			return std::unexpected(std::format("Trying to read {} bytes, but only {} remaining", size, m_FileSize - pos));
		}
	}
	if (size == 0)
	{
		return std::unexpected("Can't read 0 bytes");
	}

	char* mem = (char*)malloc(size);
	m_FileStream.read(mem, size);
	
	return st::Blob( mem, (size_t)size );
}

bool st::fs::File::IsOpen() const
{
	return m_FileStream.is_open();
}

size_t st::fs::File::Size() const
{
	return m_FileSize;
}

size_t st::fs::File::Pos()
{
	if (m_FileStream.is_open())
	{
		return m_FileStream.tellg();
	}
	else
	{
		return 0;
	}
}

const std::string& st::fs::File::GetPath() const
{
	return m_Path;
}

bool st::fs::File::Exists(const std::filesystem::path& path)
{
	return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}