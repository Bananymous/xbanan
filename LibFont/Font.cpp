#include <BAN/ScopeGuard.h>

#include <LibFont/Font.h>
#include <LibFont/PSF.h>

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

namespace LibFont
{

	BAN::ErrorOr<Font> Font::load(BAN::StringView path)
	{
		BAN::Vector<uint8_t> file_data;

		char path_buffer[PATH_MAX];
		strncpy(path_buffer, path.data(), path.size());
		path_buffer[path.size()] = '\0';

		int fd = open(path_buffer, O_RDONLY);
		if (fd == -1)
			return BAN::Error::from_errno(errno);
		BAN::ScopeGuard file_closer([fd] { close(fd); });

		struct stat st;
		if (fstat(fd, &st) == -1)
			return BAN::Error::from_errno(errno);
		TRY(file_data.resize(st.st_size));

		ssize_t total_read = 0;
		while (total_read < st.st_size)
		{
			ssize_t nread = read(fd, file_data.data() + total_read, st.st_size - total_read);
			if (nread == -1)
				return BAN::Error::from_errno(errno);
			total_read += nread;
		}

		return load(BAN::ConstByteSpan(file_data.span()));
	}

	BAN::ErrorOr<Font> Font::load(BAN::ConstByteSpan font_data)
	{
		if (is_psf1(font_data))
			return TRY(parse_psf1(font_data));
		if (is_psf2(font_data))
			return TRY(parse_psf2(font_data));
		return BAN::Error::from_errno(ENOTSUP);
	}

}
