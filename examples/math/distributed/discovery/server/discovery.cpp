////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Bryce Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////

#include <list>
#include <algorithm>

#include <boost/foreach.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/export.hpp>

#include <hpx/hpx.hpp>
#include <hpx/lcos/eager_future.hpp>
#include <hpx/runtime/actions/plain_action.hpp>
#include <hpx/runtime/applier/applier.hpp>
#include <hpx/runtime/components/stubs/runtime_support.hpp>
#include <hpx/runtime/components/component_factory.hpp>
#include <hpx/runtime/components/plain_component_factory.hpp>
#include <hpx/runtime/naming/resolver_client.hpp>
#include <hpx/util/portable_binary_iarchive.hpp>
#include <hpx/util/portable_binary_oarchive.hpp>

#include <examples/math/distributed/discovery/server/discovery.hpp>

HPX_REGISTER_COMPONENT_MODULE();

using hpx::balancing::server::discovery;

HPX_REGISTER_MINIMAL_COMPONENT_FACTORY(
    hpx::components::simple_component<discovery>, 
    discovery_factory);

HPX_REGISTER_ACTION_EX(
    discovery::build_network_action,
    discovery_build_network_action);

HPX_REGISTER_ACTION_EX(
    discovery::deploy_action,
    discovery_deploy_action);

HPX_REGISTER_ACTION_EX(
    discovery::topology_lva_action,
    discovery_topology_lva_action);

HPX_REGISTER_ACTION_EX(
    discovery::empty_action,
    discovery_empty_action);

HPX_DEFINE_GET_COMPONENT_TYPE(discovery);

typedef hpx::actions::plain_result_action0<
    // result type
    boost::uint32_t 
    // function
  , hpx::balancing::server::discovery::report_shepherd_count
> report_shepherd_count_action;

HPX_REGISTER_PLAIN_ACTION(report_shepherd_count_action);

typedef hpx::lcos::eager_future<
    report_shepherd_count_action
> report_shepherd_count_future;

typedef hpx::lcos::eager_future<
    discovery::deploy_action
> deploy_future;

namespace hpx { namespace balancing { namespace server
{

boost::uint32_t discovery::report_shepherd_count()
{ return get_runtime().get_process().get_num_os_threads(); }

std::vector<naming::id_type> discovery::build_network()
{
    // Get the prefix that the root component is on.
    const boost::uint32_t root_prefix
        = naming::get_prefix_from_gid(this->get_base_gid());

    std::vector<naming::gid_type> localities;
    applier::get_applier().get_agas_client().get_prefixes(localities);

    // Ensure that the prefixes returned are ordered.    
    std::sort(localities.begin(), localities.end());

    std::vector<lcos::future_value<boost::uint32_t> > results0;

    BOOST_FOREACH(naming::gid_type const& locality, localities)
    { results0.push_back(report_shepherd_count_future(locality)); }

    BOOST_FOREACH(naming::gid_type const& locality, localities)
    {
        const boost::uint32_t current_prefix
            = naming::get_prefix_from_gid(locality);

        BOOST_ASSERT(!topology_.count(current_prefix));

        topology_[current_prefix] = results0[current_prefix - 1].get();
    }

    std::vector<lcos::future_value<naming::id_type, naming::gid_type> >
        results1; 

    BOOST_FOREACH(naming::gid_type const& locality, localities)
    {
        const boost::uint32_t current_prefix
            = naming::get_prefix_from_gid(locality);

        if (root_prefix != current_prefix)
            results1.push_back
                (components::stubs::runtime_support::create_component_async
                    (locality, components::get_component_type<discovery>()));
    }

    std::vector<naming::id_type> network;

    for (std::size_t i = 0; i < localities.size(); ++i)
    {
        if ((i + 1) == root_prefix)
            network.push_back(this->get_gid());
        else if ((i + 1) > root_prefix)
            network.push_back(results1[i - 1].get());
        else
            network.push_back(results1[i].get());
    }

    std::list<lcos::future_value<void> > results2;

    BOOST_FOREACH(naming::id_type const& node, network)
    { results2.push_back(deploy_future(node, topology_)); }

    BOOST_FOREACH(lcos::future_value<void> const& f, results2)
    { f.get(); }

    return network;
}

}}}

