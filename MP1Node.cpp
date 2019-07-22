/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"
#define dbg 0

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
        return false;
    }
    else {
        return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);
	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif
        // send JOINREQ message to introducer member
        // sender, receiver
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);
        free(msg);
    }

    return 1;
}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
    memberNode->memberList.clear();
    memberNode->inGroup = false;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
    // membernode recieves the message from ptr node
    MessageHdr* mymsg = (MessageHdr*) data;
    Member* node = (Member *) env;
    Address * adddr = (Address *) (mymsg+1);
    long *hbt = (long *)((char *)(mymsg+1) + 1 + sizeof(node->addr.addr));
    bool found = false;

    if(mymsg->msgType == JOINREQ) {
        //fprintf(stderr,"Message == JOINREQ \n");
        // send JOINREP  TODO:--- as a cleanup clean this send function
        MessageHdr *msg;
        size_t msgsize = sizeof(MessageHdr) + sizeof(Address) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREP message: format of data is {struct Address myaddr}
        msg->msgType = JOINREP;
        memcpy((char *)(msg+1), &node->addr.addr, sizeof(node->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(node->addr.addr), &node->heartbeat, sizeof(long));

        // send JOINREP message to introducer member
        // sender, receiver
        emulNet->ENsend(&node->addr, adddr, (char *)msg, msgsize);
        log->logNodeAdd(&node->addr, adddr);

        // add initiating (the one sending JOINREQ) node to your membershiplist
        node->memberList.push_back(MemberListEntry((int) adddr->addr[0], (short)adddr->addr[4], 0, 0));
    }
    else if(mymsg->msgType == JOINREP) {
        //fprintf(stderr,"Message == JOINREP \n");
        // add node to group
        if(!node->inGroup) {
            node->inGroup = true;
            log->logNodeAdd(&node->addr, adddr);
        }
        // add node from which you recieved the JOINREP to your membershiplist
        for ( int i = 0; i < node->memberList.size(); i++) {
            if( node->memberList[i].getid() == adddr->addr[0]) {
                found = true;
            }
        }
        if (!found) {
            node->memberList.push_back(MemberListEntry((int) adddr->addr[0], (short)adddr->addr[4], 0, 0));
        }
    }
    if(mymsg->msgType != GOSSIP) {

        //check if adddr is in my membership list
        found = false;

        //yes - do nothing; perhaps you'll need to check the heartbeat and update it.
        if(node->memberList.size() > 0) {
            for ( int i = 0; i < node->memberList.size(); i++) {
                if( node->memberList[i].getid() == adddr->addr[0]) {
                    found = true;
                }
            }
        }
        //no - add member to list with current ts
        if (!found) {
            //node->memberList.push_back(MemberListEntry((int) adddr->addr[0], (short)adddr->addr[4], *hbt, (long) par->getcurrtime() ));
            node->memberList.push_back(MemberListEntry((int) adddr->addr[0], (short)adddr->addr[4], 0, 0 ));
        }
    }
#if dbg
    if(node->memberList.size() > 0) {
        printf("\nMYNODEID =  %d NNB = %d Membership_list---->", node->addr.addr[0], node->nnb);
        for (int i = 0; i < node->memberList.size(); i++) {
            printf(" [[node=%d][hb=%d][ts=%d]] ", node->memberList[i].getid(), node->memberList[i].getheartbeat(), node->memberList[i].gettimestamp() );
        }
    }
#endif

    if(mymsg->msgType == GOSSIP) {
        //fprintf(stderr,"Message == GOSSIP   ");
        MessageHdr* mymsg = (MessageHdr*) data;
        int* sz = (int *)mymsg+1;
        MemberListEntry* list = (MemberListEntry *) (sz+1);
        //vector<MemberListEntry> *m = new vector<MemberListEntry>(*sz);
        int mysize = node->memberList.size();
        bool found = false;

        if(*sz > 0 && mysize > 0) {
            for (int i = 0; i < *sz; i++) {
                for(int j = 0; j < mysize; j++) {
                    if( (*list).getid() == node->memberList[j].getid()) {
                        //fprintf(stderr, "  did x  ");
                        found = true;
                        node->memberList[j].settimestamp((long) par->getcurrtime());
                        if( (*list).getheartbeat() > node->memberList[j].getheartbeat()) {
                            //update hb
                            node->memberList[j].setheartbeat((*list).getheartbeat());
                            //fprintf(stderr, "  did xx  ");
                        }
                    }
                }
                if(!found) {
                    node->memberList.emplace_back(MemberListEntry(*list));
                    Address recv = makeAddress((*list).getid(), (*list).getport());
                    log->logNodeAdd(&node->addr, &recv);
                }
                found = false;
                list++;
            }
#if 0
            //check self for failed nodes and delete them from the list
            for(int j = 0; j < mysize; j++) {
                if( (par->getcurrtime() - node->memberList[j].getheartbeat()) > TREMOVE ) {
                    //node->bFailed = true;
                    Address recv = makeAddress(node->memberList[j].getid(),node->memberList[j].getport());
                    log->logNodeRemove(&node->addr, &recv );
                    node->memberList.erase(node->memberList.begin() + j);
                }
            }
#endif
        }
        //update nnb
        node->nnb = node->memberList.size();

    }

    free(data);
    return true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

	/*
	 * Your code goes here
	 */

    bool found = false;
    memberNode->heartbeat++;

    //update self in membershiplist
    if(memberNode->memberList.size() > 0) {
        for ( int i = 0; i < memberNode->memberList.size(); i++) {
            if( memberNode->memberList[i].getid() == memberNode->addr.addr[0]) {
                found = true;
                memberNode->memberList[i].setheartbeat(memberNode->heartbeat);
            }
        }
    }

    if(!found) {
        //memberNode->memberList.push_back(MemberListEntry((int) memberNode->addr.addr[0], (short)memberNode->addr.addr[4], memberNode->heartbeat, (long) par->getcurrtime() ));
        
        //memberNode->memberList.push_back(MemberListEntry((int) memberNode->addr.addr[0], (short)memberNode->addr.addr[4], 0, 0 ));
        //Address recv = makeAddress(memberNode->addr.addr[0], (short)memberNode->addr.addr[4]);
        //log->logNodeAdd(&memberNode->addr, &recv);
    }

    // if HB not updated in time TREMOVE
    int recv = 0;
    for(int j = 0; j < memberNode->memberList.size(); j++) {
        if( (par->getcurrtime() - memberNode->memberList[j].getheartbeat()) > TREMOVE ) {
            Address recv = makeAddress(memberNode->memberList[j].getid(),memberNode->memberList[j].getport());
            log->logNodeRemove(&memberNode->addr, &recv );
            memberNode->memberList.erase(memberNode->memberList.begin() + j);
        }
    }

    // Gossip if you have any information; you'll send your membership list to receiver
    if(memberNode->memberList.size() > 1) {
        MessageHdr *msg;
        size_t msgsize = sizeof(MessageHdr) + sizeof(long) + memberNode->memberList.size() * sizeof(MemberListEntry); //sizeof(Address) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));
        int sz = (int)memberNode->memberList.size();
        
        // create GOSSIP message: format of data is {struct Address myaddr}
        msg->msgType = GOSSIP;
        memcpy((char *)(msg+1), &sz, sizeof(int));
        memcpy((char *)(msg+1) + sizeof(int), memberNode->memberList.data(), memberNode->memberList.size() * sizeof(MemberListEntry));

        Address receiver;

        for (int i = 0; i < 3; i++) {
            do {
                recv = rand() % memberNode->memberList.size();
            } while (recv == memberNode->addr.addr[0]); //do not gossip with yourself

            receiver = makeAddress( (int)(memberNode->memberList[recv].getid()), (short)(memberNode->memberList[recv].getport()));
            // send JOINREP message to introducer member
            emulNet->ENsend(&memberNode->addr, &receiver, (char *)msg, msgsize);
            // msg contains all the nodes in the recievers membership list
        }
    }
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: makeAddress
 *
 * DESCRIPTION: Returns the Address based on values provided
 */
Address MP1Node::makeAddress(int id, short port) {
    Address addr;

    memset(&addr, 0, sizeof(Address));
    *(int *)(&addr.addr) = id;
    *(short *)(&addr.addr[4]) = port;

    return addr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
