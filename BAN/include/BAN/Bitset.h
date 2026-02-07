#pragma once

#include <BAN/Array.h>

#include <stddef.h>

namespace BAN
{

	template<size_t N>
	class Bitset
	{
	public:
		using internal_type = uint64_t;

	public:

	private:
		BAN::Array<internal_type, BAN::Math::div_round_up(N, sizeof(internal_type) * 8)> m_bits;
	};

	void foo()
	{
		sizeof(Bitset<65>);
	};

}
