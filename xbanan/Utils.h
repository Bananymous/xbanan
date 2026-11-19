#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>
#include <BAN/Vector.h>

class Client;

template<typename T>
inline BAN::Optional<T> decode(BAN::ConstByteSpan& buffer)
{
	if (buffer.size() < sizeof(T))
		return {};
	T result = buffer.as<const T>();
	buffer = buffer.slice(sizeof(T));
	return result;
}

BAN::ErrorOr<void> encode_bytes(Client& client, const void* data, size_t size);

template<typename T> requires BAN::is_pod_v<T>
inline BAN::ErrorOr<void> encode(Client& client, const T& value)
{
	return encode_bytes(client, &value, sizeof(T));
}

template<typename T> requires requires(T value) { value.size(); value.data(); }
inline BAN::ErrorOr<void> encode(Client& client, const T& value)
{
	return encode_bytes(client, value.data(), value.size() * sizeof(typename T::value_type));
}
