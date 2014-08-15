#include "broker/broker.hh"
#include "broker/data/store.hh"
#include "broker/print_queue.hh"
#include "broker/data/response_queue.hh"
#include "data/result_type_info.hh"
#include "subscription.hh"
#include <caf/announce.hpp>
#include <caf/shutdown.hpp>
#include <cstdio>

int broker::init(int flags)
	{
	// TODO: need a better, more organized way to announce types.
	using namespace caf;
	using namespace std;
	using namespace broker::data;
	announce<subscription_type>();
	announce<subscription>(&subscription::type, &subscription::topic);
	announce(typeid(subscriptions),
	         unique_ptr<uniform_type_info>(new subscriptions_type_info));
	announce<subscriber>(&subscriber::first, &subscriber::second);
	announce<sequence_num>(&sequence_num::sequence);
	announce<snapshot>(&snapshot::datastore, &snapshot::sn);
	announce<std::unordered_set<broker::data::key>>();
	announce<std::deque<std::string>>();
	announce<result::type>();
	announce<result::status>();
	announce(typeid(result),
	         unique_ptr<uniform_type_info>(new result_type_info));
	announce<query::type>();
	announce<query>(&query::tag, &query::k);
	announce<response>(&response::request, &response::reply, &response::cookie);
	announce<std::deque<response>>();
	return 0;
	}

int broker_init(int flags)
	{
	return broker::init(flags);
	}

void broker::done()
	{
	caf::shutdown();
	}

void broker_done()
	{
	return broker::done();
	}

const char* broker::strerror(int arg_errno)
	{
	switch ( arg_errno ) {
	default:
		return ::strerror(arg_errno);
	};
	}

const char* broker_strerror(int arg_errno)
	{
	return broker::strerror(arg_errno);
	}