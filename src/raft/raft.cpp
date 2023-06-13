// #include "raft.h"
// #include "peer.h"
// #include "raft/storage.h"
// #include <tinyev/Logger.h>

// using namespace raft;

// Raft::Raft(const Config& _config,const std::vector<RaftPeer*>& _peers)
//     // basic configuration
//     : id(_config.id) 
//     , peerNum(static_cast<int>(_peers.size()))
//     , storage(_config.storagePath)
//     , currentTerm(storage.getCurrentTerm())
//     , votedFor(storage.getVotedFor())
//     , log(&storage)
//     , heartbeatTimeout(_config.heartbeatTimeout)
//     , electionTimeout(_config.electionTimeout)
//     , randomGenerator(id,electionTimeout,2*electionTimeout)
//     , peers(_peers)
//     , applyMsgCallback(_config.applyMsgCallback)
//     , snapshotCallback(_config.snapshotCallback){
//     resetTimer();
//     // print info when init
//     DEBUG("raft[%d] %s, term %d, first_index %d, last_index %d",id, getRoleString(),currentTerm,log.getFirstIndex(),log.getLastIndex());
// }

// RaftState Raft::getState() const{
//     return {currentTerm,role == LEADER };
// }

// ProposeResult Raft::getPropose(const json::Value& command){
//     // new index
//     int index = log.getLastIndex() + 1;
//     int tempCurrentTerm = currentTerm;
//     bool isleader = (role == LEADER);
//     // is leader 
//     // ! not leader ? do what ?
//     if(isleader){
//         log.append(currentTerm,command);
//         DEBUG("raft[%d] %s, term %d, propose log %d",id, getRoleString(), currentTerm, index);
//     }
//     if(isStandalone()){
//         // only one node in raft cluster
//         commitIndex = index;
//         applyLog();
//     }
//     return {index,tempCurrentTerm,isleader};
// }

// void Raft::startRequestVote(){
//     // args to request vote
//     RequestVoteArgs args;
//     args.term = currentTerm;
//     args.candidateId = id;
//     args.lastLogIndex = log.getLastIndex();
//     args.lastLogTerm = log.getLastTerm();

//     for(int i = 0; i < peerNum; i++){
//         if(i!=id)
//             // request for voting to itself
//             peers[i]->requestVote(args); // peer->requestvote not raft?
//     }
// }

// void Raft::requestVote(const RequestVoteArgs& args,RequestVoteReply& reply){
//     // this function running in voter ? 

//     // check new term
//     onNewInputTerm(args.term);
//     resetTimer();
//     // send request to others, receive reply ?
//     reply.term = currentTerm;
//     // if term equal, check log's term and index
//     if(args.term == currentTerm && (votedFor == votedForNull || votedFor == args.candidateId)
//      && log.isUptoDate(args.lastLogIndex,args.lastLogTerm)){
//         // itself vote to candidate
//         DEBUG("raft[%d] -> raft[%d], candidate'Term: %d", id, args.candidateId,args.term);
//         // persistence
//         setVotedFor(args.candidateId);
//         // set vote result
//         reply.voteGranted = true;
//      }
//      else {
//         reply.voteGranted = false;
//      }
// }

// void Raft::onRequestVoteReply(int _peer,const RequestVoteArgs& args,const RequestVoteReply& reply){
//     // case : running in candidate after recieving a vote reply
//     // check term
//     onNewInputTerm(reply.term);
//     // not candidate | vote falied | expired vote
//     if(role!=CANDIDATE || !reply.voteGranted || currentTerm > reply.term){
//         return;
//     } 
//     // candidate know vote success, print info
//     DEBUG("raft[%d] <- raft[%d], currentTerm: %d", id, _peer,currentTerm);
//     votesGet++;
//     // check votesGet
//     if(votesGet > peerNum / 2){
//         transToLeader();
//     }
// }

// void Raft::startAppendEntries(){
//     // start a new append entry process
//     for(int i = 0; i < peerNum; i++){
//         // peer to do this
//         if(i != id){
//             AppendEntriesArgs args;
//             args.term = currentTerm;
//             args.prevLogIndex = nextIndex[i] - 1;
//             args.prevLogTerm = log.getCurrentTerm(args.prevLogIndex);
//             // entries from [] ???
//             // TODO: getJsonEntries
//             args.entries = log.getJsonEntries(nextIndex[i],maxEntriesSend);
//             // commit index
//             args.leaderCommit = commitIndex;
//             // set peer to do append
//             peers[i]->appendEntries(args);
//         }
//     }
// }

// // used in node.cpp 
// void Raft::appendEntries(const AppendEntriesArgs& args,AppendEntriesReply& reply){
//     // check 
//     onNewInputTerm(args.term);
//     // reset timer
//     resetTimer();
//     // one node receive append, request it
//     reply.term = currentTerm;
//     // compare rpc.term with its term
//     if(currentTerm > args.term){
//         // refuse expired heartbeat
//         reply.success = false;
//         return;
//     } else if(role == CANDIDATE){
//         // newer term make node trans to follower
//         stepdownToFollower(currentTerm);
//     } else if(role == LEADER){
//         // >= 2 leader in cluster
//         FATAL("multiple leaders in term %d", currentTerm);
//     }
//     // log match while it is follower and be in same term
//     if(log.contain(args.prevLogIndex,args.prevLogTerm)){
//         // write log
//         log.overWrite(args.prevLogIndex+1,args.entries);
//         // 
//         int possibleCommit = std::min(args.leaderCommit,log.getLastIndex());
//         // repair log ?
//         if(commitIndex < possibleCommit){
//             commitIndex = possibleCommit;
//             // ? 
//             applyLog();
//         }
//         reply.success = true;
//     }else { // log match failed beacause last entry ?
//         // get follower's log last index and term
//         auto lastEntry = log.lastIndexinTerm(args.prevLogIndex,args.prevLogTerm);
//         reply.expectIndex = lastEntry.index;
//         reply.expectTerm = lastEntry.term;
//         reply.success = false;
//     }
// }

// void Raft::onAppendEntriesReply(int _peer, const AppendEntriesArgs &args, const AppendEntriesReply &reply){
//     // leader send rpc and receive this reply
//     onNewInputTerm(reply.term);
//     if(role != LEADER || currentTerm > reply.term){
//         // not a leader anymore and expired RPC
//         return;
//     }
//     if(!reply.success){
//         // log replication failed, need to repair log

//         // get peer's matched new next index
//         int newNxtIndex = nextIndex[_peer];
//         if(reply.expectTerm == args.prevLogTerm){
//             assert(reply.expectIndex < args.prevLogIndex);
//             // expect index is the answer
//             newNxtIndex = reply.expectIndex;
//         }else{
//             assert(reply.expectTerm < args.prevLogTerm);
//             // get expect term's last index
//             auto entry = log.lastIndexinTerm(newNxtIndex,reply.expectTerm);
//             newNxtIndex = entry.index;
//         }
//         // ----------- ? 
//         if(newNxtIndex > nextIndex[_peer]){
//             newNxtIndex = nextIndex[_peer] - 1;
//         }
//         if(newNxtIndex <= matchIndex[_peer]){
//             DEBUG("raft[%d] %s,nextIndex <= matchIndex[%d],set to %d",id,getRoleString(),_peer,matchIndex[_peer]+1);
//             newNxtIndex = matchIndex[_peer + 1];
//         }
//         nextIndex[_peer] = newNxtIndex;
//         return;
//     }
//     // log replication succeed do what?
//     int startIndex = args.prevLogIndex + 1;
//     int entryNum = static_cast<int>(args.entries.getSize());
//     int endIndex = startIndex + entryNum - 1;
//     for(int i=endIndex; i >= startIndex;i--){
//         if(i <= matchIndex[_peer])
//             break;
//         if(log.getCurrentTerm(i) < currentTerm)
//             break;
//         assert(log.getCurrentTerm(i) == currentTerm);

//         if(i <= commitIndex)
//             break;
            
//         int replica = 2; // ?
//         for(int p=0;p<peerNum;p++){
//             if(i<=matchIndex[p])
//                 replica++;
//         }
//         if(replica>peerNum / 2){
//             commitIndex= i;
//             break;
//         }
//     }

//     applyLog();
//     if(nextIndex[_peer] <= endIndex){
//         nextIndex[_peer] = endIndex + 1;
//         matchIndex[_peer] = endIndex;
//     }
// }

// void Raft::tick(){
//     switch (role){
//         case FOLLOWER:
//         case CANDIDATE:
//             // do leader election
//             tickOnElection();
//             break;
//         case LEADER:
//             // heartbeat rpc
//             tickOnHeartbeat();
//             break;
//         default:
//             assert(false && "error role");
//     }
// }

// void Raft::debugOutput() const{
//     DEBUG("raft[%d] %s, term %d, #votes %d, commit %d", id, getRoleString(), currentTerm, votesGet, commitIndex);
// }

// void Raft::applyLog(){
//     // lastApplied commitIndex ?
//     assert(lastApplied <= commitIndex);
//     if(commitIndex != lastApplied){
//         if(lastApplied + 1 == commitIndex){
//             // only print info 
//             DEBUG("raft[%d] %s, term %d, apply log [%d]",id,getRoleString(),currentTerm,commitIndex);
//         } else{
//             DEBUG("raft[%d] %s, term %d, apply log (%d,%d]",id,getRoleString(),currentTerm,lastApplied,commitIndex);
//         }
        
//     }
//     for(int i = lastApplied + 1; i <= commitIndex; i++){
//         ApplyMessage msg(i,log.getCurrentCommand(i));
//         // need config to set callback function when applying log
//         applyMsgCallback(msg);
//     }
//     lastApplied = commitIndex;
// }

// void Raft::tickOnElection(){
//     timeElapsed++;
//     if(timeElapsed >= randomElectionTimeout){
//         // time out timer
//         transToCandidate();
//     }
// }

// void Raft::tickOnHeartbeat(){
//     timeElapsed++;
//     if(timeElapsed >= heartbeatTimeout){
//         // heartbeat rpc ? need a special heartbeat rpc or what ?
//         startAppendEntries();
//         resetTimer();
//     }
// }

// void Raft::setCurrentTerm(int _term){
//     currentTerm = _term;
//     storage.putCurrentTerm(currentTerm);
// }

// void Raft::setVotedFor(int _votedfor){
//     votedFor = _votedfor;
//     storage.putVotedFor(votedFor);
// }

// void Raft::stepdownToFollower(int targetTerm){
//     // receive a new term
//     if(role != FOLLOWER){
//         DEBUG("raft[%d] %s -> follower,currentTerm = %d",id,getRoleString(),currentTerm);
//     }
//     assert(currentTerm <= targetTerm);

//     role = FOLLOWER;
//     if(currentTerm < targetTerm){
//         setCurrentTerm(targetTerm);
//         // new term no vote
//         setVotedFor(votedForNull);
//         votesGet = 0;
//     }
//     resetTimer();
// }

// void Raft::transToCandidate(){
//     if(role!=CANDIDATE){
//         DEBUG("raft[%d] %s -> candidate, currentTerm = %d",id,getRoleString(),currentTerm);
//     }

//     role = CANDIDATE;
//     setCurrentTerm(currentTerm + 1);
//     setVotedFor(id);
//     votesGet = 1;

//     if(isStandalone()){
//         transToLeader();
//     }else{
//         resetTimer();
//         startRequestVote();
//     }
// }

// void Raft::transToLeader(){
//     DEBUG("raft[%d] %s -> leader",id,getRoleString());
//     nextIndex.assign(peerNum,log.getLastIndex() + 1);
//     matchIndex.assign(peerNum,initMatchIndex);
//     role = LEADER;
//     resetTimer();
// }

// void Raft::onNewInputTerm(int _term){
//     if(currentTerm < _term){
//         stepdownToFollower(_term);
//     }
// }

// void Raft::resetTimer(){
//     timeElapsed = 0;
//     if(role != LEADER){
//         // get a timeout
//         randomElectionTimeout = randomGenerator.generate();
//     }
// }