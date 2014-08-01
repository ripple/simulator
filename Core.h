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

#ifndef __CORE_STRUCTS__
#define __CORE_STRUCTS__

#include <vector>
#include <map>
#include <list>
#include <map>
#include <cassert>

class NodeState
{
    // A NodeState as propagated by the network
public:
    int node;
    int ts;
    unsigned char state;

    NodeState(int n, int t, unsigned char s) : node(n), ts(t), state(s) { ; }
};

class Message
{
    // A message sent from one node to another, containing the positions taken
public:
    int from_node, to_node;
    std::map<int, NodeState> data;

    Message(int from, int to) : from_node(from), to_node(to) { ; }
    Message(int from, int to, const std::map<int, NodeState>& d)
        : from_node(from), to_node(to), data(d) { ; }

    void addPositions(const std::map<int, NodeState>&);
    void subPositions(const std::map<int, NodeState>&);
};

class Event
{
    // One or more messages that are received at a particular time
public:
    std::list<Message> messages;

    Message *addMessage(const Message& m) { messages.push_back(m); return &*messages.rbegin(); }
};

class Link
{
    // A connection from one node to another
public:
    int to_node;
    int total_latency;
    int lm_send_time, lm_recv_time;
    Message *lm;

    Link(int t, int tl) : to_node(t), total_latency(tl), lm_send_time(0), lm_recv_time(0), lm(NULL) { ; }
};

class Network
{
public:
    int master_time;
    std::map<int, Event> messages;

    Network() : master_time(0) { ; }

    void sendMessage(const Message& message, Link& link, int send_time)
    {
        assert (message.to_node == link.to_node);
        link.lm_send_time = send_time;
        link.lm_recv_time = send_time + link.total_latency;
        link.lm = messages[link.lm_recv_time].addMessage(message);
    }
};

class Node
{
public:
    int n, e2c_latency;

    std::vector<int> unl;
    std::vector<Link> links;

    std::vector<int> nts; // node time stamps
    std::vector<signed char> knowledge; // node states

    int messages_sent, messages_received;

    Node(int nn, int mm) : n(nn), nts(mm, 0), knowledge(mm, 0), messages_sent(0), messages_received(0)
    { ; }

    void processMessage(const Message& m);

    bool isOnUNL(int j)
    {
        for (int v : unl)
            if (v==j) return true;
        return false;
    }

    bool hasLinkTo(int j)
    {
        for (const Link& l : links)
            if (l.to_node==j) return true;
        return false;
    }

    void receiveMessage(const Message& m, Network& n);

};

#endif
