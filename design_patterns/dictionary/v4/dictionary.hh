#pragma once

#include "../string_view.hh"

#include <string>
#include <vector>

// linear probing w/ & operation for hashing.
namespace v4 {

class Dictionary
{
public:
	template<typename Collection>
	explicit Dictionary(const Collection& words)
	{
		std::size_t hashSize = 1;
		if ((words.size() & (words.size() - 1)) == 0)
			hashSize = words.size() * 2; // words.size() is a power of two; use it directly as the hashSize
		else
		{
			while (hashSize <= words.size())
			{
				// Why do we need hashSize to be a power of two? Because of modulo arithmetic.
				// If hashSize is a power of two, we can avoid the % operation when taking the modulo
				// hash & (hashSize - 1), which is much faster
				hashSize <<= 1;
			}
		}

		hashTable_.resize(hashSize);

		for(const std::string& s : words)
		{
			insert(s);
		}
		size_ = words.size();
	}

	bool in_dictionary(string_view word) const
	{
		auto hash = std::hash<string_view>()(word);

		auto idx = index(hash);
		std::size_t attempts = 0;

		// Use attempts to prevent infinite loops
		while (attempts < size_)
		{
			const Entry& entry = hashTable_[idx];

			if (!entry.string)
				return false;

			if(entry.hash == hash && *entry.string == word)
				return true;

			idx = next(idx);
			attempts++;
		}
		return false;
	}

private:
    // Linear probing:
    // !! This section is the essence: given a string s, find the value it maps to
    // 1. string_to_string_view: s -> sv
    // 2. calculate hash<string_view>()(sv); // compute the hash value
    // 3. calculate idx = index(hash); // compute the storage-array index from the hash
	// 		index calculation: return hash & (hashTable_.size()-1);
	// 		This is equivalent to hash % hashTable_.size(). However, if hashTable.size() is 2.^n, 
	// 		then the modulo operation can be replaced with hash & (hashTable_.size() - 1), which is much faster)
    // 4. Use idx to find the Entry: Entry& entry = hashTable_[idx];
	// 		4.1 If the entry is already occupied (if (entry.string) == true), find the next idx = next(idx).
	// 			Calculation:
	// 			next = (idx + 1) & (hashTable_.size()-1);
	// 			Go back to step 4 and continue checking
	// 			
	// 		4.2 Otherwise, use the empty entry directly and set the fields:
	// 
	// 			entry.string = &s;
	// 			entry.hash = hash;
	// 
	// 			Break out of the loop
	// 	
	// 
	// 
	void insert(const std::string& s)
	{
		string_view sv(s);
		auto hash = std::hash<string_view>()(sv);
		auto idx = index(hash);

		while(true)
		{
			Entry& entry = hashTable_[idx];
			if (entry.string)
			{
				idx = next(idx);
			}
			else
			{
				entry.string = &s;
				entry.hash = hash;
				break;
			}
		}
	}

	// & operation takes less cpu cycles than mod operation
	std::size_t index(std::size_t hash) const { return hash & (hashTable_.size()-1); }
	std::size_t next(std::size_t idx) const { return (idx + 1) & (hashTable_.size()-1); }

	struct Entry
	{
		std::size_t hash;
		const std::string* string = nullptr;
	};

	std::vector<Entry> hashTable_;
	std::size_t size_ = 0;
};

}
