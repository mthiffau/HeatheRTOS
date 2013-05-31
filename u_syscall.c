#include "u_tid.h"
#include "u_syscall.h"

#include "xbool.h"
#include "xdef.h"
#include "xassert.h"
#include "xstring.h"
#include "xmemcpy.h"
#include "ns.h"

int
RegisterAs(const char *name)
{
    struct ns_msg msg;
    int           rc, rplylen, namesize;
    namesize = strnlen(name, NS_NAME_MAXLEN) + 1;
    assert(namesize <= NS_NAME_MAXLEN);
    msg.type = NS_MSG_REGISTER;
    memcpy(msg.name, name, namesize);
    rplylen = Send(NS_TID, &msg, sizeof (msg), &rc, sizeof (rc));
    assert(rplylen == sizeof (rc));
    return rc;
}

tid_t
WhoIs(const char *name)
{
    struct ns_msg msg;
    int           tid, rplylen, namesize;
    namesize = strnlen(name, NS_NAME_MAXLEN) + 1;
    assert(namesize <= NS_NAME_MAXLEN);
    msg.type = NS_MSG_WHOIS;
    memcpy(msg.name, name, namesize);
    rplylen = Send(NS_TID, &msg, sizeof (msg), &tid, sizeof (tid));
    assert(rplylen == sizeof (tid));
    return tid;
}
