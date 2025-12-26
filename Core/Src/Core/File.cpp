#include "CorePCH.h"
#include "Core/File.h"

st::fs::File::File(const std::string& path, OpenMode mode)
{
	Open(path, mode);	
}

st::fs::File::~File()
{
	Close();
}

bool st::fs::File::Open(const std::string& path, OpenMode mode)
{
	m_FileStream.open(path, (mode == OpenMode::Read ? std::ios::in : std::ios::out) | std::ios::binary);
	if (m_FileStream.is_open())
	{
		m_Mode = mode;
		if (m_Mode == OpenMode::Read)
		{
			m_FileStream.seekg(0, std::ios::end);
			m_FileSize = m_FileStream.tellg();
			m_FileStream.seekg(0, std::ios::beg);
		}
		else
		{
			m_FileSize = 0;
		}

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
	if (m_Mode != OpenMode::Read)
	{
		return std::unexpected("Invalid open mode for a read operation");
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

std::expected<size_t, std::string> st::fs::File::Write(const void* data, size_t size)
{
	if (!m_FileStream.is_open())
	{
		return std::unexpected("File is not opened");
	}
	if (m_Mode != OpenMode::Write)
	{
		return std::unexpected("Invalid open mode for a write operation");
	}

	m_FileStream.write((const char*)data, size);
	return size;
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