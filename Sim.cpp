//------------------------------------------------------------------------------
/*
    This file is part of consensus-sim
    Copyright (c) 2013, Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/foreach.hpp>

#include "Core.h"

#define LEDGER_CONVERGE          4
#define LEDGER_FORCE_CONVERGE    7
#define AV_MIN_CONSENSUS        50
#define AV_AVG_CONSENSUS        60
#define AV_MAX_CONSENSUS        70


#define NUM_NODES             1000
#define NUM_MALICIOUS_NODES     15
#define CONSENSUS_PERCENT       80

// Latencies in milliseconds
// E2C - End to core, the latency from a node to a nearby node
// C2C - Core to core, the additional latency when nodes are far 
#define MIN_E2C_LATENCY          5
#define MAX_E2C_LATENCY         50
#define MIN_C2C_LATENCY          5
#define MAX_C2C_LATENCY        200

#define NUM_OUTBOUND_LINKS      10

#define UNL_MIN                 20
#define UNL_MAX                 30
#define UNL_THRESH              (UNL_MIN/2) // unl datapoints we have to have before we change position

#define BASE_DELAY               1 // extra time we delay a message to coalesce/suppress

#define SELF_WEIGHT              1 // how many UNL votes you give yourself

#define PACKETS_ON_WIRE          3 // how many packets can be "on the wire" per link per direction
                                   // simulates non-infinite bandwidth

int nodes_positive=0, nodes_negative=0;

void Node::receiveMessage(const Message& m, Network& network)
{
    ++messages_received;

    // If we were going to send any of this data to that node, skip it
    BOOST_FOREACH(Link& link, links)
    {
        if ((link.to_node == m.from_node) && (link.lm_send_time >= network.master_time))
        {
            // We can still update a waiting outbound message
            link.lm -> subPositions(m.data);
            break;
        }
    }

    // 1) Update our knowledge
    std::map<int, NodeState> changes;

    for (std::map<int, NodeState>::const_iterator change_it = m.data.begin();
        change_it != m.data.end(); ++change_it)
    {
        if ( (change_it->first != n) && (knowledge[change_it->first] != change_it->second.state) &&
                (change_it->second.ts > nts[change_it->first]) )
        {
            // This gives us new information about a node
            knowledge[change_it->first] = change_it->second.state;
            nts[change_it->first] = change_it->second.ts;
            changes.insert(std::make_pair(change_it->first, change_it->second));
        }
    }

    if (changes.empty()) return; // nothing changed

    // 2) Choose our position change, if any
    int unl_count = 0, unl_balance = 0;
    BOOST_FOREACH(int node, unl)
    {
        if (knowledge[node] == 1)
        {
            ++unl_count;
            ++unl_balance;
        }
        if (knowledge[node] == -1)
        {
            ++unl_count;
            --unl_balance;
        }
    }

    if (n < NUM_MALICIOUS_NODES) // if we are a malicious node, be contrarian
        unl_balance = -unl_balance;

    // add a bias in favor of 'no' as time passes
    // (agree to disagree)
    unl_balance -= network.master_time / 250;

    bool pos_change=false;
    if (unl_count >= UNL_THRESH)
    { // We have enough data to make decisions
        if ( (knowledge[n] == 1) && (unl_balance < (-SELF_WEIGHT)) )
        {
            // we switch to -
            knowledge[n] = -1;
            --nodes_positive;
            ++nodes_negative;
            changes.insert(std::make_pair(n, NodeState(n, ++nts[n], -1)));
            pos_change=true;
        }
        else if ( (knowledge[n] == -1) && (unl_balance > SELF_WEIGHT) )
        {
            // we switch to +
            knowledge[n] = 1;
            ++nodes_positive;
            --nodes_negative;
            changes.insert(std::make_pair(n, NodeState(n, ++nts[n], +1)));
            pos_change=true;
        }
    }

    // 3) Broadcast the message
    BOOST_FOREACH(Link& link, links)
    {
        if (pos_change || (link.to_node != m.from_node))
        {
            // can we update an unsent message?
            if (link.lm_send_time > network.master_time)
                link.lm->addPositions(changes);
            else
            {
                // No, we need a new mesage
                int send_time = network.master_time;
                if (!pos_change)
                {
                    // delay the messag a bit to permit coalescing and suppression
                    send_time += BASE_DELAY;
                    if (link.lm_recv_time > send_time) // a packet is on the wire
                        send_time += link.total_latency / PACKETS_ON_WIRE; // wait a bit extra to send
                }
                network.sendMessage(Message(n, link.to_node, changes), link, send_time);
                messages_sent++;
            }
        }
    }
}

int main(void)
{

    // This will produce the same results each time
    boost::random::mt19937 gen;
    boost::random::uniform_int_distribution<> r_e2c(MIN_E2C_LATENCY, MAX_E2C_LATENCY);
    boost::random::uniform_int_distribution<> r_c2c(MIN_C2C_LATENCY, MAX_C2C_LATENCY);
    boost::random::uniform_int_distribution<> r_unl(UNL_MIN, UNL_MAX);
    boost::random::uniform_int_distribution<> r_node(0, NUM_NODES-1);

    Node* nodes[NUM_NODES];

    // create nodes
    std::cerr << "Creating nodes" << std::endl;
    for (int i = 0; i < NUM_NODES; ++i)
    {
        nodes[i] = new Node(i, NUM_NODES);
        nodes[i]->e2c_latency = r_e2c(gen);

        // our own position starts as 50/50 split
        if (i%2)
        {
            nodes[i]->knowledge[i] = 1;
            nodes[i]->nts[i] = 1;
            ++nodes_positive;
        }
        else
        {
            nodes[i]->knowledge[i] = -1;
            nodes[i]->nts[i] = 1;
            ++nodes_negative;
        }

        // Build our UNL
        int unl_count = r_unl(gen);
        while (unl_count > 0)
        {
            int cn = r_node(gen);
            if ((cn != i) && !nodes[i]->isOnUNL(cn))
            {
                nodes[i]->unl.push_back(cn);
                --unl_count;
            }
        }
    }


    // create links
    std::cerr << "Creating links" << std::endl;
    for (int i = 0; i < NUM_NODES; ++i)
    {
        int links = NUM_OUTBOUND_LINKS;
        while (links > 0)
        {
            int lt = r_node(gen);
            if ((lt != i) && !nodes[i]->hasLinkTo(lt))
            {
                int ll = nodes[i]->e2c_latency + nodes[lt]->e2c_latency + r_c2c(gen);
                nodes[i]->links.push_back(Link(lt, ll));
                nodes[lt]->links.push_back(Link(i, ll));
                --links;
            }
        }
    }

    Network network;

    // trigger all nodes to make initial broadcasts of their own positions
    std::cerr << "Creating initial messages" << std::endl;
    for (int i = 0; i < NUM_NODES; ++i)
    {
        BOOST_FOREACH(Link& l, nodes[i]->links)
        {
            Message m(i, l.to_node);
            m.data.insert(std::make_pair(i, NodeState(i, 1, nodes[i]->knowledge[i])));
            network.sendMessage(m, l, 0);
        }
    }
    std::cerr << "Created " << network.messages.size()  << " events" << std::endl;
    
    // run simulation
    do
    {
        if (nodes_positive > (NUM_NODES * CONSENSUS_PERCENT / 100))
            break;
        if (nodes_negative > (NUM_NODES * CONSENSUS_PERCENT / 100))
            break;
        
        std::map<int, Event>::iterator ev=network.messages.begin();
        if (ev == network.messages.end())
        {
            std::cerr << "Fatal: Radio Silence" << std::endl;
            return 0;
        }

        if ((ev->first / 100) > (network.master_time / 100))
            std::cerr << "Time: " << ev->first << " ms  " <<
                nodes_positive << "/" << nodes_negative <<  std::endl;
        network.master_time = ev->first;

        BOOST_FOREACH(const Message& m, ev->second.messages)
        {
            if (m.data.empty()) // message was never sent
                --nodes[m.from_node]->messages_sent;
            else
                nodes[m.to_node]->receiveMessage(m, network);
        }
        
        network.messages.erase(ev);
    } while (1);

    int mc = 0;
    for (std::map<int, Event>::iterator it = network.messages.begin(); it != network.messages.end(); ++it)
            mc += it->second.messages.size();
        std::cerr << "Consensus reached in " << network.master_time << " ms with " << mc
        << " messages on the wire" << std::endl;
    
    // output results
    long total_messages_sent= 0 ;
    for (int i = 0; i < NUM_NODES; ++i)
        total_messages_sent += nodes[i]->messages_sent;
    std::cerr << "The average node sent " << total_messages_sent/NUM_NODES << " messages" << std::endl;
}
