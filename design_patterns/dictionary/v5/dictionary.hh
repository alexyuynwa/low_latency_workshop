#pragma once

#include "../string_view.hh"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"



namespace v5 {

class Dictionary
{
public:
	template<typename Collection>
	explicit Dictionary(const Collection& words) :
		_container(words.cbegin(), words.cend())
	{}

	bool in_dictionary(string_view word) const
	{
		return _container.find({word.data(), word.size()}) != _container.end();
	}

private:
	// absl::flat_hash_set<std::string> _container;
	absl::node_hash_set<std::string> _container;
};
}