/*
 * Copyright (c) 2012-2016 Daniele Bartolini and individual contributors.
 * License: https://github.com/taylor001/crown/blob/master/LICENSE
 */

// http://sebastiansylvan.com/post/robin-hood-hashing-should-be-your-default-hash-table-implementation/

#pragma once

#include "container_types.h"
#include <algorithm> // std::swap
#include <new>
#include <string.h>  // memcpy

namespace crown
{
/// Functions to manipulate HashMap.
///
/// @ingroup Containers
namespace hash_map
{
	/// Returns the number of items in the map @a m.
	template <typename TKey, typename TValue, typename Hash> u32 size(const HashMap<TKey, TValue, Hash>& m);

	/// Returns the maximum number of items the map @a m can hold.
	template <typename TKey, typename TValue, typename Hash> u32 capacity(const HashMap<TKey, TValue, Hash>& m);

	/// Returns whether the given @a key exists in the map @a m.
	template <typename TKey, typename TValue, typename Hash> bool has(const HashMap<TKey, TValue, Hash>& m, const TKey& key);

	/// Returns the value for the given @a key or @a deffault if
	/// the key does not exist in the map.
	template <typename TKey, typename TValue, typename Hash> const TValue& get(const HashMap<TKey, TValue, Hash>& m, const TKey& key, const TValue& deffault);

	/// Sets the @a value for the @a key in the map.
	template <typename TKey, typename TValue, typename Hash> void set(HashMap<TKey, TValue, Hash>& m, const TKey& key, const TValue& value);

	/// Removes the @a key from the map if it exists.
	template <typename TKey, typename TValue, typename Hash> void remove(HashMap<TKey, TValue, Hash>& m, const TKey& key);

	/// Removes all the items in the map.
	///
	/// @note
	/// Calls destructor on the items.
	template <typename TKey, typename TValue, typename Hash> void clear(HashMap<TKey, TValue, Hash>& m);
} // namespace hash_map

namespace hash_map_internal
{
	const u32 END_OF_LIST = 0xffffffffu;
	const u32 DELETED = 0x80000000u;
	const u32 FREE = 0x00000000u;

	template <typename TKey, class Hash>
	inline u32 hash_key(const TKey& key)
	{
		const Hash hash;
		return hash(key);
	}

	inline bool is_deleted(u32 index)
	{
		// MSB set indicates that this hash is a "tombstone"
		return (index >> 31) != 0;
	}

	template <typename TKey, typename TValue, typename Hash>
	inline u32 probe_distance(const HashMap<TKey, TValue, Hash>& m, u32 hash, u32 slot_index)
	{
		const u32 hash_i = hash & m._mask;
		return (slot_index + m._capacity - hash_i) & m._mask;
	}

	template <typename TKey, typename TValue, typename Hash>
	u32 find(const HashMap<TKey, TValue, Hash>& m, const TKey& key)
	{
		if (m._size == 0)
			return END_OF_LIST;

		const u32 hash = hash_key<TKey, Hash>(key);
		u32 hash_i = hash & m._mask;
		u32 dist = 0;
		for(;;)
		{
			if (m._index[hash_i].index == FREE)
				return END_OF_LIST;
			else if (dist > probe_distance(m, m._index[hash_i].hash, hash_i))
				return END_OF_LIST;
			else if (!is_deleted(m._index[hash_i].index) && m._index[hash_i].hash == hash && m._data[hash_i].pair.first == key)
				return hash_i;

			hash_i = (hash_i + 1) & m._mask;
			++dist;
		}
	}

	template <typename TKey, typename TValue, typename Hash>
	void insert(HashMap<TKey, TValue, Hash>& m, u32 hash, const TKey& key, const TValue& value)
	{
		PAIR(TKey, TValue) new_item(*m._allocator);
		new_item.first  = key;
		new_item.second = value;

		u32 hash_i = hash & m._mask;
		u32 dist = 0;
		for(;;)
		{
			if (m._index[hash_i].index == FREE)
				goto INSERT_AND_RETURN;

			// If the existing elem has probed less than us, then swap places with existing
			// elem, and keep going to find another slot for that elem.
			u32 existing_elem_probe_dist = probe_distance(m, m._index[hash_i].hash, hash_i);
			if (is_deleted(m._index[hash_i].index) || existing_elem_probe_dist < dist)
			{
				if (is_deleted(m._index[hash_i].index))
					goto INSERT_AND_RETURN;

				std::swap(hash, m._index[hash_i].hash);
				m._index[hash_i].index = 0x0123abcd;
				PAIR(TKey, TValue) tmp(*m._allocator);
				memcpy(&tmp, &m._data[hash_i].pair, sizeof(new_item));
				memcpy(&m._data[hash_i].pair, &new_item, sizeof(new_item));
				memcpy(&new_item, &tmp, sizeof(new_item));

				dist = existing_elem_probe_dist;
			}

			hash_i = (hash_i + 1) & m._mask;
			++dist;
		}

	INSERT_AND_RETURN:
		new (m._data + hash_i) typename HashMap<TKey, TValue, Hash>::Entry(*m._allocator);
		memcpy(m._data + hash_i, &new_item, sizeof(new_item));
		m._index[hash_i].hash = hash;
		m._index[hash_i].index = 0x0123abcd;
		PAIR(TKey, TValue) empty(*m._allocator);
		memcpy(&new_item, &empty, sizeof(new_item));
	}

	template <typename TKey, typename TValue, typename Hash>
	void rehash(HashMap<TKey, TValue, Hash>& m, u32 new_capacity)
	{
		typedef typename HashMap<TKey, TValue, Hash>::Entry Entry;
		typedef typename HashMap<TKey, TValue, Hash>::Index Index;

		HashMap<TKey, TValue, Hash> nm(*m._allocator);
		nm._index = (Index*)nm._allocator->allocate(new_capacity*sizeof(Index), alignof(Index));
		nm._data = (Entry*)nm._allocator->allocate(new_capacity*sizeof(Entry), alignof(Entry));

		// Flag all elements as free
		for (u32 i = 0; i < new_capacity; ++i)
		{
			nm._index[i].hash = 0;
			nm._index[i].index = FREE;
		}

		nm._capacity = new_capacity;
		nm._size = m._size;
		nm._mask = new_capacity - 1;

		for (u32 i = 0; i < m._capacity; ++i)
		{
			typename HashMap<TKey, TValue, Hash>::Entry& e = m._data[i];
			const u32 hash = m._index[i].hash;
			const u32 index = m._index[i].index;

			if (index != FREE && !is_deleted(index))
				hash_map_internal::insert(nm, hash, e.pair.first, e.pair.second);
		}

		HashMap<TKey, TValue, Hash> empty(*m._allocator);
		m.~HashMap<TKey, TValue, Hash>();
		memcpy(&m, &nm, sizeof(HashMap<TKey, TValue, Hash>));
		memcpy(&nm, &empty, sizeof(HashMap<TKey, TValue, Hash>));
	}

	template <typename TKey, typename TValue, typename Hash>
	void grow(HashMap<TKey, TValue, Hash>& m)
	{
		const u32 new_capacity = (m._capacity == 0 ? 16 : m._capacity * 2);
		rehash(m, new_capacity);
	}

	template <typename TKey, typename TValue, typename Hash>
	bool full(const HashMap<TKey, TValue, Hash>& m)
	{
		return m._size >= m._capacity * 0.9f;
	}
} // namespace hash_map_internal

namespace hash_map
{
	template <typename TKey, typename TValue, typename Hash>
	u32 size(const HashMap<TKey, TValue, Hash>& m)
	{
		return m._size;
	}

	template <typename TKey, typename TValue, typename Hash>
	u32 capacity(const HashMap<TKey, TValue, Hash>& m)
	{
		return m._capacity;
	}

	template <typename TKey, typename TValue, typename Hash>
	bool has(const HashMap<TKey, TValue, Hash>& m, const TKey& key)
	{
		return hash_map_internal::find(m, key) != hash_map_internal::END_OF_LIST;
	}

	template <typename TKey, typename TValue, typename Hash>
	const TValue& get(const HashMap<TKey, TValue, Hash>& m, const TKey& key, const TValue& deffault)
	{
		const u32 i = hash_map_internal::find(m, key);
		if (i == hash_map_internal::END_OF_LIST)
			return deffault;
		else
			return m._data[i].pair.second;
	}

	template <typename TKey, typename TValue, typename Hash>
	void set(HashMap<TKey, TValue, Hash>& m, const TKey& key, const TValue& value)
	{
		if (m._capacity == 0)
			hash_map_internal::grow(m);

		// Find or make
		const u32 i = hash_map_internal::find(m, key);
		if (i == hash_map_internal::END_OF_LIST)
		{
			hash_map_internal::insert(m, hash_map_internal::hash_key<TKey, Hash>(key), key, value);
			++m._size;
		}
		else
		{
			m._data[i].pair.second = value;
		}
		if (hash_map_internal::full(m))
			hash_map_internal::grow(m);
	}

	template <typename TKey, typename TValue, typename Hash>
	void remove(HashMap<TKey, TValue, Hash>& m, const TKey& key)
	{
		const u32 i = hash_map_internal::find(m, key);
		if (i == hash_map_internal::END_OF_LIST)
			return;

		m._data[i].~Entry();
		m._index[i].index |= hash_map_internal::DELETED;
		--m._size;
	}

	template <typename TKey, typename TValue, typename Hash>
	void clear(HashMap<TKey, TValue, Hash>& m)
	{
		for (u32 i = 0; i < m._capacity; ++i)
		{
			if (m._index[i].index == 0x0123abcd)
				m._data[i].~Entry();
			m._index[i].index = hash_map_internal::FREE;
		}

		m._size = 0;
	}
} // namespace hash_map

template <typename TKey, typename TValue, typename Hash>
HashMap<TKey, TValue, Hash>::HashMap(Allocator& a)
	: _allocator(&a)
	, _capacity(0)
	, _size(0)
	, _mask(0)
	, _index(NULL)
	, _data(NULL)
{
}

template <typename TKey, typename TValue, typename Hash>
HashMap<TKey, TValue, Hash>::~HashMap()
{
	for (u32 i = 0; i < _capacity; ++i)
	{
		if (_index[i].index == 0x0123abcd)
			_data[i].~Entry();
	}

	_allocator->deallocate(_index);
	_allocator->deallocate(_data);
}

template <typename TKey, typename TValue, typename Hash>
const TValue& HashMap<TKey, TValue, Hash>::operator[](const TKey& key) const
{
	return hash_map::get(*this, key, TValue());
}

} // namespace crown