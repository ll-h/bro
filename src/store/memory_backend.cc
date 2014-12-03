#include "memory_backend_impl.hh"
#include "../util/misc.hh"

broker::store::memory_backend::memory_backend()
    : pimpl(new impl)
	{}

broker::store::memory_backend::~memory_backend() = default;

broker::store::memory_backend::memory_backend(memory_backend& other)
    : pimpl(new impl(*other.pimpl))
	{}

broker::store::memory_backend::memory_backend(memory_backend&& other)
    : pimpl(std::move(other.pimpl))
	{}

broker::store::memory_backend&
broker::store::memory_backend::operator=(memory_backend other)
	{
	using std::swap;
	swap(pimpl, other.pimpl);
	return *this;
	}

void broker::store::memory_backend::do_increase_sequence()
	{ ++pimpl->sn; }

std::string broker::store::memory_backend::do_last_error() const
	{
	return pimpl->last_error;
	}

bool broker::store::memory_backend::do_init(snapshot sss)
	{
	pimpl->sn = std::move(sss.sn);
	pimpl->datastore = std::move(sss.datastore);
	return true;
	}

const broker::store::sequence_num&
broker::store::memory_backend::do_sequence() const
	{ return pimpl->sn; }

bool broker::store::memory_backend::do_insert(data k, data v,
                                              util::optional<expiration_time> t)
	{
	pimpl->datastore[std::move(k)] = value{std::move(v), std::move(t)};
	return true;
	}

int broker::store::memory_backend::do_increment(const data& k, int64_t by)
	{
	auto it = pimpl->datastore.find(k);

	if ( it == pimpl->datastore.end() )
		{
		pimpl->datastore[k] = value{by, {}};
		return 0;
		}

	if ( util::increment_data(it->second.item, by, &pimpl->last_error) )
		return 0;

	return 1;
	}

int broker::store::memory_backend::do_add_to_set(const data& k, data element)
	{
	auto it = pimpl->datastore.find(k);

	if ( it == pimpl->datastore.end() )
		{
		pimpl->datastore[k] = value{set{std::move(element)}, {}};
		return 0;
		}

	if ( util::add_data_to_set(it->second.item, std::move(element),
	                           &pimpl->last_error) )
		return 0;

	return 1;
	}

int broker::store::memory_backend::do_remove_from_set(const data& k,
                                                      const data& element)
	{
	auto it = pimpl->datastore.find(k);

	if ( it == pimpl->datastore.end() )
		{
		pimpl->datastore[k] = value{set{}, {}};
		return 0;
		}

	if ( util::remove_data_from_set(it->second.item, element,
	                                &pimpl->last_error) )
		return 0;

	return 1;
	}

bool broker::store::memory_backend::do_erase(const data& k)
	{
	pimpl->datastore.erase(k);
	return true;
	}

bool broker::store::memory_backend::do_clear()
	{
	pimpl->datastore.clear();
	return true;
	}

broker::util::optional<broker::util::optional<broker::data>>
broker::store::memory_backend::do_lookup(const data& k) const
	{
	try
		{
		return {pimpl->datastore.at(k).item};
		}
	catch ( const std::out_of_range& )
		{
		return {util::optional<data>{}};
		}
	}

broker::util::optional<bool>
broker::store::memory_backend::do_exists(const data& k) const
	{
	if ( pimpl->datastore.find(k) == pimpl->datastore.end() )
		return false;
	else
		return true;
	}

broker::util::optional<std::unordered_set<broker::data>>
broker::store::memory_backend::do_keys() const
	{
	std::unordered_set<data> rval;
	for ( const auto& kv : pimpl->datastore )
		rval.emplace(kv.first);
	return rval;
	}

broker::util::optional<uint64_t> broker::store::memory_backend::do_size() const
	{ return pimpl->datastore.size(); }

broker::util::optional<broker::store::snapshot>
broker::store::memory_backend::do_snap() const
	{ return snapshot{pimpl->datastore, pimpl->sn}; }

broker::util::optional<std::deque<broker::store::expirable>>
broker::store::memory_backend::do_expiries() const
	{
	std::deque<expirable> rval;

	for ( const auto& entry : pimpl->datastore )
		if ( entry.second.expiry )
			rval.push_back({entry.first, *entry.second.expiry});

	return rval;
	}