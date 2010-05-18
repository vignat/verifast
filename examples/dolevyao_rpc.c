// Dolev-Yao security verification of a simple request-response protocol.
// Uses an invariant-based approach inspired by the work of Ernie Cohen and the work of Andrew Gordon et al.

#include "stdlib.h"

/*

Items
=====

Dolev-Yao security of a protocol means that the intended security properties are verified under
the assumption that the cryptographic primitives used, such as key generation, keyed hashes, etc.
are perfect. This assumption is formalized by modelling items sent over public channels not as
bitstrings but as structured values.

For example, we represent the Nth key generated by principal P
as a structured value key_item(P, N). It automatically follows that key_item(P1, N1) == key_item(P2, N2)
if and only if P1 == P2 and N1 == N2. This formalizes the assumption that key generation generates
unique keys.

We represent the HMAC-SHA1 keyed hash of payload item I
generated with the Nth key generated by principal P
as hmacsha1_item(P, N, I). This formalizes the assumption that there are no hash collisions.

API wrapper functions
=====================

We are verifying real, working protocol implementations. These use real cryptographic functions, and
real network I/O functions. The
data objects manipulated by these functions, such as keys, messages, and hashes, are bitstrings.
However, to facilitate associating an item value with
such a physical data object, we wrap these functions in a thin layer that represents the data objects
as objects of type struct item. The wrapper functions are specified in terms of a predicate item(item, i)
that states that item object "item" represents item value "i".

We do not provide an implementation of the wrappers here, but such an implementation is straightforward.

Network I/O and "pub"
=====================

We also declare wrappers for network I/O, called net_send and net_receive. For simplicity, we ignore
addressing aspects: net_send sends an item into the world, and net_receive plucks an arbitrary
item from the world. The world is where the attacker is. We represent the world by a predicate
"world(pub)", where "pub" is a pure function from item values to booleans; that is, it is a predicate
on item values. It specifies an upper bound on which item values are sent into the world. That is,
net_send requires that the item being sent satisfies pub, and net_receive ensures that the item
that is returned satisfies pub.

This file implements two groups of functions: the functions that implement the protocol, and the
function that represents the attacker, called "attacker". Function "attacker" performs all the
operations that an attacker can perform. Specifically, it generates keys and publishes them; it
creates hashes of public items and publishes them; it constructs and destructs pairs; etc.

Crucially, both groups of functions operate on the same world, with the same "pub" function. (This is not
checked by VeriFast.)

Verifying protocol integrity
============================

Protocol integrity means that if the protocol implementation reports to the application that something has
happened, it has indeed happened. For example, in the case of an RPC protocol, if the protocol implementation
on the server reports an incoming request from a given client principal, this client principal must in fact
have made this request. Analogously, if the client protocol reports a response from the server principal, the
application at this principal must indeed have submitted this response.

This is formalized using "event predicates". Here, I use the term "predicate" not in the sense of
a VeriFast predicate, but in the sense of a function that returns a boolean. Specifically, for each event,
an unimplemented fixpoint function is declared that takes as arguments the values that identify the event,
and returns bool.

The example protocol performs RPC between pairs of clients and servers that have agreed on a secret key.
The key agreement mechanism is not modeled; it is assumed that client and server share a secret key.
Specifically, we declare a function shared_with(P, N) that takes a principal P and an index N and that
returns the principal with whom the Nth key created by P is shared, or -1 if the key was not shared. We
assume the client generated the key and shared it with the server.

Note: we do model bad principals. That is, we declare a function bad(P) that returns true if P is bad and
false if it is not. The integrity of the protocol is conditional on the client and server not being bad.
That is, integrity means that "good" principals can use the protocol safely, even if other principals that are
using the protocol are bad. Badness of principal P in this example means that P publishes keys that it creates.
Conversely, if P is not bad, it does not publish keys that it creates.

The example protocol uses two event predicates: "request(C, S, R)" states that client C sent a request item R to
server S; "response(C, S, R, R1)" states that server S responded to request item R from client C with response item
R1.

Defining "pub"
==============

The core task of verifying a protocol implementation is defining "pub". "pub" must be sufficiently weak so that
sends by the protocol functions and sends by the attacker are allowed; but sufficiently strong so that when the
protocol receives an item and the item is valid, the appropriate event predicate follows from it.

*/

struct item;

/*@

inductive item =
  | key_item(int creator, int id)
  | data_item(int id)
  | hmacsha1_item(int keyCreator, int id, item payload)
  | pair_item(item first, item second);

predicate item(struct item *item, item i);

predicate key(struct item *item, int creator, int id) = item(item, key_item(creator, id));

predicate world(fixpoint(item, bool) pub);

predicate principal(int id, int keyCount); // Keeps track of the number of keys generated by principal "id"

predicate principals(int count); // Keeps track of the number of principals generated, so as to ensure unique principal ids.

lemma void create_principal();
    requires principals(?count);
    ensures principals(count + 1) &*& principal(count, 0);

@*/

struct item *create_key();
    //@ requires principal(?principal, ?keyCount);
    //@ ensures principal(principal, keyCount + 1) &*& key(result, principal, keyCount);

// check_is_key aborts if the item is not a key.
void check_is_key(struct item *item);
    //@ requires item(item, ?i);
    /*@
    ensures
        switch (i) {
            case key_item(creator, id): return key(item, creator, id);
            case data_item(d): return false;
            case hmacsha1_item(c, id, p): return false;
            case pair_item(f, s): return false;
        };
    @*/

struct item *create_data_item(int data);
    //@ requires true;
    //@ ensures item(result, data_item(data));

// item_get_data aborts if the item is not a data item.
int item_get_data(struct item *item);
    //@ requires item(item, ?i);
    /*@
    ensures
        switch (i) {
            case key_item(creator, id): return false;
            case data_item(d): return item(item, i) &*& result == d;
            case hmacsha1_item(c, id, p): return false;
            case pair_item(f, s): return false;
        };
    @*/

struct item *hmacsha1(struct item *key, struct item *payload);
    //@ requires key(key, ?creator, ?id) &*& item(payload, ?p);
    //@ ensures key(key, creator, id) &*& item(payload, p) &*& item(result, hmacsha1_item(creator, id, p));

// A real implementation must encode the pair such that the first and second components can be extracted correctly.
struct item *create_pair(struct item *first, struct item *second);
    //@ requires item(first, ?f) &*& item(second, ?s);
    //@ ensures item(first, f) &*& item(second, s) &*& item(result, pair_item(f, s));

void net_send(struct item *datagram);
    //@ requires world(?pub) &*& item(datagram, ?d) &*& pub(d) == true;
    //@ ensures world(pub) &*& item(datagram, d);

struct item *net_receive();
    //@ requires world(?pub);
    //@ ensures world(pub) &*& item(result, ?d) &*& pub(d) == true;

struct item *pair_get_first(struct item *pair);
    //@ requires item(pair, ?p);
    /*@
    ensures
        item(pair, p) &*&
        switch (p) {
            case key_item(o, k): return false;
            case data_item(d): return false;
            case hmacsha1_item(creator, id, pl): return false;
            case pair_item(f, s): return item(result, f);
        };
    @*/

struct item *pair_get_second(struct item *pair);
    //@ requires item(pair, ?p);
    /*@
    ensures
        item(pair, p) &*&
        switch (p) {
            case key_item(o, k): return false;
            case data_item(d): return false;
            case hmacsha1_item(creator, id, pl): return false;
            case pair_item(f, s): return item(result, s);
        };
    @*/

void hmacsha1_verify(struct item *hash, struct item *key, struct item *payload);
    //@ requires item(hash, ?h) &*& key(key, ?creator, ?id) &*& item(payload, ?p);
    //@ ensures item(hash, h) &*& key(key, creator, id) &*& item(payload, p) &*& h == hmacsha1_item(creator, id, p);

// A real implementation can simply compare the bitstrings.
bool item_equals(struct item *item1, struct item *item2);
    //@ requires item(item1, ?i1) &*& item(item2, ?i2);
    //@ ensures item(item1, i1) &*& item(item2, i2) &*& result == (i1 == i2);

void item_free(struct item *item);
    //@ requires item(item, _);
    //@ ensures true;

/*@

fixpoint bool bad(int principal);

fixpoint bool request(int cl, int sv, item req);
fixpoint bool response(int cl, int sv, item req, item resp);
fixpoint int shared_with(int cl, int id);

// A definition of "pub" for the example protocol.
fixpoint bool mypub(item i) {
    switch (i) {
        case key_item(o, k): return bad(o) || bad(shared_with(o, k));
        case data_item(d): return true;
        case hmacsha1_item(creator, id, m): return
            bad(creator) || bad(shared_with(creator, id))
            ||
            switch (m) {
                case pair_item(f, s): return
                    switch (f) {
                        case data_item(tag): return
                            tag == 0 ?
                                request(creator, shared_with(creator, id), s)
                            : tag == 1 ?
                                switch (s) {
                                    case pair_item(req, resp): return
                                        response(creator, shared_with(creator, id), req, resp);
                                    default: return false;
                                }
                            : false
                              ;
                        default: return false;
                    };
                default: return false;
            };
        case pair_item(i1, i2): return mypub(i1) && mypub(i2);
    }
}

@*/

struct item *client(int server, struct item *key, struct item *request)
    /*@
    requires
        world(mypub) &*& key(key, ?creator, ?id) &*& item(request, ?req) &*&
        mypub(req) == true &*& request(creator, server, req) == true &*& shared_with(creator, id) == server;
    @*/
    /*@
    ensures
        world(mypub) &*& key(key, creator, id) &*& item(request, req) &*& item(result, ?resp) &*&
        bad(creator) || bad(server) || response(creator, server, req, resp) == true;
    @*/
{
    {
        struct item *tag = create_data_item(0);
        struct item *payload = create_pair(tag, request);
        item_free(tag);
        struct item *hash = hmacsha1(key, payload);
        struct item *m = create_pair(hash, payload);
        item_free(hash);
        item_free(payload);
        net_send(m);
        item_free(m);
    }
    
    {
        struct item *r = net_receive();
        struct item *hash = pair_get_first(r);
        struct item *payload = pair_get_second(r);
        item_free(r);
        hmacsha1_verify(hash, key, payload);
        item_free(hash);
        struct item *tag = pair_get_first(payload);
        int tagValue = item_get_data(tag);
        if (tagValue != 1) abort();
        item_free(tag);
        struct item *reqresp = pair_get_second(payload);
        item_free(payload);
        struct item *request1 = pair_get_first(reqresp);
        struct item *response = pair_get_second(reqresp);
        item_free(reqresp);
        bool eq = item_equals(request, request1);
        if (!eq) abort();
        item_free(request1);
        return response;
    }
}

// This function represents the server application.
// We pass in the key predicate just to get hold of the creator principal id.
struct item *compute_response(struct item *request);
    //@ requires key(?key, ?creator, ?id) &*& item(request, ?req) &*& bad(creator) || bad(shared_with(creator, id)) || request(creator, shared_with(creator, id), req) == true;
    /*@
    ensures
        key(key, creator, id) &*& item(request, req) &*& item(result, ?resp) &*&
        mypub(resp) == true &*& response(creator, shared_with(creator, id), req, resp) == true;
    @*/

void server(int serverId, struct item *key)
    //@ requires world(mypub) &*& key(key, ?creator, ?id) &*& shared_with(creator, id) == serverId; // BUG
    //@ ensures false;
{
    for (;;)
        //@ invariant world(mypub) &*& key(key, creator, id);
    {
        struct item *request = 0;
        {
            struct item *r = net_receive();
            struct item *hash = pair_get_first(r);
            struct item *payload = pair_get_second(r);
            item_free(r);
            hmacsha1_verify(hash, key, payload);
            item_free(hash);
            struct item *tag = pair_get_first(payload);
            int tagValue = item_get_data(tag);
            if (tagValue != 0) abort();
            item_free(tag);
            request = pair_get_second(payload);
            item_free(payload);
        }
        
        {
            struct item *response = compute_response(request);
            struct item *reqresp = create_pair(request, response);
            item_free(response);
            item_free(request);
            struct item *tag = create_data_item(1);
            struct item *payload = create_pair(tag, reqresp);
            item_free(tag);
            item_free(reqresp);
            struct item *hash = hmacsha1(key, payload);
            struct item *m = create_pair(hash, payload);
            item_free(hash);
            item_free(payload);
            net_send(m);
            item_free(m);
        }
    }
}

int choose();
    //@ requires true;
    //@ ensures true;

void attacker()
    //@ requires world(mypub) &*& principals(0);
    //@ ensures false;
{
    for (;;)
        //@ invariant world(mypub) &*& principals(?principalCount);
    {
        //@ create_principal(); // Attackers are arbitrary principals.
        for (;;)
            //@ invariant world(mypub) &*& principals(_) &*& principal(?me, ?keyCount);
        {
            int action = choose();
            switch (action) {
                case 0:
                    // Bad principals leak keys...
                    struct item *item = create_key();
                    //@ open key(item, ?creator, ?id);
                    //@ assume(bad(creator) || bad(shared_with(creator, id)));
                    net_send(item);
                    item_free(item);
                    break;
                case 1:
                    // Anyone can publish arbitrary data items...
                    int data = choose();
                    struct item *item = create_data_item(data);
                    net_send(item);
                    item_free(item);
                    break;
                case 2:
                    // Anyone can create pairs of public items...
                    struct item *first = net_receive();
                    struct item *second = net_receive();
                    struct item *pair = create_pair(first, second);
                    item_free(first);
                    item_free(second);
                    net_send(pair);
                    item_free(pair);
                    break;
                case 3:
                    // Anyone can hash a public item with a published key...
                    struct item *key = net_receive();
                    struct item *payload = net_receive();
                    check_is_key(key);
                    struct item *hash = hmacsha1(key, payload);
                    //@ open key(key, _, _);
                    item_free(key);
                    item_free(payload);
                    net_send(hash);
                    item_free(hash);
                    break;
                case 4:
                    // Anyone can deconstruct a public pair...
                    struct item *pair = net_receive();
                    struct item *first = pair_get_first(pair);
                    struct item *second = pair_get_second(pair);
                    item_free(pair);
                    net_send(first);
                    item_free(first);
                    net_send(second);
                    item_free(second);
                    break;
            }
        }
        //@ leak principal(_, _);
    }
}