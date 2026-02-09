#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>
#include <BAN/Vector.h>

template<typename T>
inline BAN::Optional<T> decode(BAN::ConstByteSpan& buffer)
{
	if (buffer.size() < sizeof(T))
		return {};
	T result = buffer.as<const T>();
	buffer = buffer.slice(sizeof(T));
	return result;
}

inline BAN::ErrorOr<void> encode_bytes(BAN::Vector<uint8_t>& buffer, const void* data, size_t size)
{
	const size_t old_size = buffer.size();
	TRY(buffer.resize(old_size + size));
	memcpy(buffer.data() + old_size, data, size);
	return {};
}

template<typename T> requires BAN::is_pod_v<T>
inline BAN::ErrorOr<void> encode(BAN::Vector<uint8_t>& buffer, const T& value)
{
	return encode_bytes(buffer, &value, sizeof(T));
}

template<typename T> requires requires(T value) { value.size(); value.data(); }
inline BAN::ErrorOr<void> encode(BAN::Vector<uint8_t>& buffer, const T& value)
{
	return encode_bytes(buffer, value.data(), value.size());
}
